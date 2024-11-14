// SPDX-License-Identifier: Apache-2.0
// Copyright © 2021-2024 Intel Corporation

#include <stdexcept>

#include "exceptions.hpp"
#include "passes.hpp"
#include "private.hpp"

namespace MIR::Passes {

namespace {

inline bool valid_holder(const std::optional<Instruction> & holder) {
    if (!holder) {
        return false;
    }
    auto && held = holder.value();

    if (!std::holds_alternative<Identifier>(*held.obj_ptr)) {
        return false;
    }
    return std::get<Identifier>(*held.obj_ptr).value == "meson";
}

using ToolchainMap =
    std::unordered_map<MIR::Toolchain::Language,
                       MIR::Machines::PerMachine<std::shared_ptr<MIR::Toolchain::Toolchain>>>;

std::optional<Instruction> lower_get_id_method(const FunctionCall & func) {
    if (!func.pos_args.empty()) {
        throw Util::Exceptions::InvalidArguments(
            "compiler.get_id(): takes no positional arguments");
    }
    if (!func.kw_args.empty()) {
        throw Util::Exceptions::InvalidArguments("compiler.get_id(): takes no keyword arguments");
    }

    const auto & comp = std::get<Compiler>(func.holder.object());

    return String{comp.toolchain->compiler->id()};
}

} // namespace

std::optional<Instruction> insert_compilers(const Instruction & obj, const ToolchainMap & tc) {
    if (!std::get_if<FunctionCall>(obj.obj_ptr.get())) {
        return std::nullopt;
    }
    const auto & f = std::get<FunctionCall>(*obj.obj_ptr);

    if (!(valid_holder(f.holder) && f.name == "get_compiler")) {
        return std::nullopt;
    }

    // XXX: if there is no argument here this is going to blow up spectacularly
    const auto & l = f.pos_args[0];
    // If we haven't reduced this to a string then we need to wait and try again later
    if (!std::get_if<String>(l.obj_ptr.get())) {
        return std::nullopt;
    }

    const auto & lang = MIR::Toolchain::from_string(std::get<String>(*l.obj_ptr).value);

    MIR::Machines::Machine m;
    try {
        const auto & n = f.kw_args.at("native");
        // If we haven't lowered this away yet, then we can't reduce this.
        if (!std::get_if<Boolean>(n.obj_ptr.get())) {
            return std::nullopt;
        }
        const auto & native = std::get<Boolean>(*n.obj_ptr).value;

        m = native ? MIR::Machines::Machine::BUILD : MIR::Machines::Machine::HOST;
    } catch (std::out_of_range &) {
        m = MIR::Machines::Machine::HOST;
    }

    try {
        return std::make_shared<Object>(Compiler{tc.at(lang).get(m)});
    } catch (std::out_of_range &) {
        // TODO: add a better error message
        throw Util::Exceptions::MesonException{"No compiler for language"};
    }
}

std::optional<Instruction> lower_compiler_methods(const Instruction & obj) {
    if (!std::holds_alternative<FunctionCall>(*obj.obj_ptr)) {
        return std::nullopt;
    }
    const auto & f = std::get<FunctionCall>(*obj.obj_ptr);

    if (!std::holds_alternative<Compiler>(f.holder.object())) {
        return std::nullopt;
    }

    if (!all_args_reduced(f.pos_args, f.kw_args)) {
        return std::nullopt;
    }

    std::optional<Instruction> i = std::nullopt;
    if (f.name == "get_id") {
        i = lower_get_id_method(f);
    }

    if (i) {
        i.value().var = obj.var;
        return i;
    }

    // XXX: Shouldn't really be able to get here...
    return std::nullopt;
}

} // namespace MIR::Passes
