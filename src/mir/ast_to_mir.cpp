// SPDX-license-identifier: Apache-2.0
// Copyright © 2021 Intel Corporation

#include <filesystem>

#include "ast_to_mir.hpp"
#include "exceptions.hpp"

namespace MIR {

namespace {

/**
 * Lowers AST expressions into MIR objects.
 */
struct ExpressionLowering {

    ExpressionLowering(const MIR::State::Persistant & ps) : pstate{ps} {};

    const MIR::State::Persistant & pstate;

    Object operator()(const std::unique_ptr<Frontend::AST::String> & expr) const {
        return std::make_unique<String>(expr->value);
    };

    Object operator()(const std::unique_ptr<Frontend::AST::FunctionCall> & expr) const {
        // I think that a function can only be an ID, I think
        auto fname_id = std::visit(*this, expr->id);
        auto fname_ptr = std::get_if<std::unique_ptr<Identifier>>(&fname_id);
        if (fname_ptr == nullptr) {
            // TODO: Better error message witht the thing being called
            throw Util::Exceptions::MesonException{"Object is not callable"};
        }
        auto fname = (*fname_ptr)->value;

        // Get the positional arguments
        std::vector<Object> pos{};
        for (const auto & i : expr->args->positional) {
            pos.emplace_back(std::visit(*this, i));
        }

        std::unordered_map<std::string, Object> kwargs{};
        for (const auto & [k, v] : expr->args->keyword) {
            auto key_obj = std::visit(*this, k);
            auto key_ptr = std::get_if<std::unique_ptr<MIR::Identifier>>(&key_obj);
            if (key_ptr == nullptr) {
                // TODO: better error message
                throw Util::Exceptions::MesonException{"keyword arguments must be identifiers"};
            }
            auto key = (*key_ptr)->value;
            kwargs[key] = std::visit(*this, v);
        }

        std::filesystem::path path{expr->loc.filename};

        // We have to move positional arguments because Object isn't copy-able
        // TODO: filename is currently absolute, but we need the source dir to make it relative
        return std::make_unique<FunctionCall>(
            fname, std::move(pos), std::move(kwargs),
            std::filesystem::relative(path.parent_path(), pstate.build_root));
    };

    Object operator()(const std::unique_ptr<Frontend::AST::Boolean> & expr) const {
        return std::make_unique<Boolean>(expr->value);
    };

    Object operator()(const std::unique_ptr<Frontend::AST::Number> & expr) const {
        return std::make_unique<Number>(expr->value);
    };

    Object operator()(const std::unique_ptr<Frontend::AST::Identifier> & expr) const {
        return std::make_unique<Identifier>(expr->value);
    };

    Object operator()(const std::unique_ptr<Frontend::AST::Array> & expr) const {
        auto arr = std::make_unique<Array>();
        for (const auto & i : expr->elements) {
            arr->value.emplace_back(std::visit(*this, i));
        }
        return arr;
    };

    Object operator()(const std::unique_ptr<Frontend::AST::Dict> & expr) const {
        auto dict = std::make_unique<Dict>();
        for (const auto & [k, v] : expr->elements) {
            auto key_obj = std::visit(*this, k);
            if (!std::holds_alternative<std::unique_ptr<String>>(key_obj)) {
                throw Util::Exceptions::InvalidArguments("Dictionary keys must be strintg");
            }
            auto key = std::get<std::unique_ptr<MIR::String>>(key_obj)->value;

            dict->value[key] = std::visit(*this, v);
        }
        return dict;
    };

    Object operator()(const std::unique_ptr<Frontend::AST::GetAttribute> & expr) const {
        // XXX: This is wrong, we can have things like:
        // meson.get_compiler('c').get_id()
        // Which this code *cannot* handle here.
        auto holding_obj = std::visit(*this, expr->object);
        assert(std::holds_alternative<std::unique_ptr<MIR::Identifier>>(holding_obj));

        // Meson only allows methods in objects, so we can enforce that this is a function
        auto method = std::visit(*this, expr->id);
        assert(std::holds_alternative<std::unique_ptr<MIR::FunctionCall>>(method));

        auto func = std::move(std::get<std::unique_ptr<MIR::FunctionCall>>(method));
        func->holder = std::get<std::unique_ptr<MIR::Identifier>>(holding_obj)->value;

        return func;
    };

    // XXX: all of thse are lies to get things compiling
    Object operator()(const std::unique_ptr<Frontend::AST::AdditiveExpression> & expr) const {
        return std::make_unique<String>("placeholder: add");
    };
    Object operator()(const std::unique_ptr<Frontend::AST::MultiplicativeExpression> & expr) const {
        return std::make_unique<String>("placeholder: mul");
    };
    Object operator()(const std::unique_ptr<Frontend::AST::UnaryExpression> & expr) const {
        return std::make_unique<String>("placeholder: unary");
    };
    Object operator()(const std::unique_ptr<Frontend::AST::Subscript> & expr) const {
        return std::make_unique<String>("placeholder: subscript");
    };
    Object operator()(const std::unique_ptr<Frontend::AST::Relational> & expr) const {
        return std::make_unique<String>("placeholder: rel");
    };
    Object operator()(const std::unique_ptr<Frontend::AST::Ternary> & expr) const {
        return std::make_unique<String>("placeholder: tern");
    };
};

/**
 * Lowers AST statements into MIR objects.
 */
struct StatementLowering {

    StatementLowering(const MIR::State::Persistant & ps) : pstate{ps} {};

    const MIR::State::Persistant & pstate;

    BasicBlock * operator()(BasicBlock * list,
                            const std::unique_ptr<Frontend::AST::Statement> & stmt) const {
        const ExpressionLowering l{pstate};
        list->instructions.emplace_back(std::visit(l, stmt->expr));
        return list;
    };

    BasicBlock * operator()(BasicBlock * list,
                            const std::unique_ptr<Frontend::AST::IfStatement> & stmt) const {
        assert(list != nullptr);
        const ExpressionLowering l{pstate};

        // This is the block that all exists from the conditional web will flow
        // back into if they don't exit. I think this is safe even for cases where
        // The blocks don't really rejoin, as this will just be empty and that's fine.
        //
        // This has the added bonus of giving us a place to put phi nodes?
        auto next_block = std::make_shared<BasicBlock>();

        // The last block that was encountered, we need this to add the next block to it.
        BasicBlock * last_block;

        // Get the value of the coindition itself (`if <condition>\n`)
        assert(list->condition == nullptr);
        list->condition = std::make_unique<Condition>(std::visit(l, stmt->ifblock.condition));

        auto * cur = list->condition.get();

        // Walk over the statements, adding them to the if_true branch.
        for (const auto & i : stmt->ifblock.block->statements) {
            last_block = std::visit(
                [&](const auto & a) {
                    assert(cur != nullptr);
                    return this->operator()(cur->if_true.get(), a);
                },
                i);
        }

        // We shouldn't have a condition here, this is where we wnat to put our next target
        assert(last_block->condition == nullptr);
        assert(last_block->next == nullptr);
        last_block->next = next_block;

        // for each elif branch create a new condition in the `else` of the
        // Condition, then assign the condition to the `if_true`. Then go down
        // the `else` of that new block for the next `elif`
        if (!stmt->efblock.empty()) {
            for (const auto & el : stmt->efblock) {
                // cur = cur->condition->if_false = std::make_unique<Condition>();
                cur->if_false = std::make_unique<Condition>(std::visit(l, el.condition));
                for (const auto & i : el.block->statements) {
                    last_block = std::visit(
                        [&](const auto & a) {
                            return this->operator()(cur->if_false->if_true.get(), a);
                        },
                        i);
                }
                cur = cur->if_false.get();

                assert(last_block->condition == nullptr);
                assert(last_block->next == nullptr);
                last_block->next = next_block;
            }
        }

        // Finally, handle an else block.
        if (stmt->eblock.block != nullptr) {
            cur->if_false = std::make_unique<Condition>(std::make_unique<MIR::Boolean>(true));
            for (const auto & i : stmt->eblock.block->statements) {
                last_block = std::visit(
                    [&](const auto & a) {
                        return this->operator()(cur->if_false->if_true.get(), a);
                    },
                    i);
            }
            assert(last_block->condition == nullptr);
            assert(last_block->next == nullptr);
            last_block->next = next_block;
        } else {
            /*
             * Codegen time!
             * If we're here that means we have the following situation:
             *   <block 1>
             *   if condition
             *     <block 2>
             *   endif
             *   <block 3>
             * Which means that if condition is false, that we need <block 1>
             * continue to <block 2>. To achieve that we create an else block
             * which continnues on, ie:
             *   <block 1>
             *   if condition
             *     <block 2>
             *   else
             *     <block 3>
             *   endif
             *   <block 4>
             *
             * By treating all if's as having an else block we simplify our handling considerably.
             */
            cur->if_false =
                std::make_unique<Condition>(std::make_unique<Boolean>(true), next_block);
        }

        // The last leg of the tree should be empty
        // XXX: or should it point to next, in the event of `if/elif/endif`?
        assert(last_block->condition == nullptr || last_block->condition->if_false == nullptr);

        // Return the raw pointer, which is fine because we're not giving the
        // caller ownership of the pointer, the other basic blocks are the owners.
        return next_block.get();
    };

    BasicBlock * operator()(BasicBlock * list,
                            const std::unique_ptr<Frontend::AST::Assignment> & stmt) const {
        const ExpressionLowering l{pstate};
        auto target = std::visit(l, stmt->lhs);
        auto value = std::visit(l, stmt->rhs);

        // XXX: need to handle mutative assignments
        assert(stmt->op == Frontend::AST::AssignOp::EQUAL);

        // XXX: need to handle other things that can be assigned to, like subscript
        auto name_ptr = std::get_if<std::unique_ptr<Identifier>>(&target);
        if (name_ptr == nullptr) {
            throw Util::Exceptions::MesonException{
                "This might be a bug, or might be an incomplete implementation"};
        }
        std::visit([&](const auto & t) { t->var.name = (*name_ptr)->value; }, value);

        list->instructions.emplace_back(std::move(value));
        return list;
    };

    // XXX: None of this is actually implemented
    BasicBlock * operator()(BasicBlock * list,
                            const std::unique_ptr<Frontend::AST::ForeachStatement> & stmt) const {
        return list;
    };
    BasicBlock * operator()(BasicBlock * list,
                            const std::unique_ptr<Frontend::AST::Break> & stmt) const {
        return list;
    };
    BasicBlock * operator()(BasicBlock * list,
                            const std::unique_ptr<Frontend::AST::Continue> & stmt) const {
        return list;
    };
};

} // namespace

/**
 * Lower AST representation into MIR.
 */
BasicBlock lower_ast(const std::unique_ptr<Frontend::AST::CodeBlock> & block,
                     const MIR::State::Persistant & pstate) {
    BasicBlock bl{};
    BasicBlock * current_block = &bl;
    const StatementLowering lower{pstate};
    for (const auto & i : block->statements) {
        current_block = std::visit([&](const auto & a) { return lower(current_block, a); }, i);
    }

    return bl;
}

} // namespace MIR
