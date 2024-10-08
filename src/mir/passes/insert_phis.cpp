// SPDX-License-Identifier: Apache-2.0
// Copyright © 2021-2024 Intel Corporation

#include "passes.hpp"
#include "private.hpp"

namespace MIR::Passes {

namespace {

/// Does this block have only one parent?
inline bool is_strictly_dominated(const BasicBlock & block) { return block.parents.size() <= 1; }

/// Allows comparing Phi pointers, using the value comparisons
struct PhiComparator {
    bool operator()(const Instruction * l, const Instruction * r) const {
        return l->var.name < r->var.name && std::get<Phi>(*l->obj_ptr) < std::get<Phi>(*r->obj_ptr);
    };
};

template <typename T> struct reversion_wrapper {
    T & iterable;
};

template <typename T> auto begin(reversion_wrapper<T> w) { return std::rbegin(w.iterable); }

template <typename T> auto end(reversion_wrapper<T> w) { return std::rend(w.iterable); }

template <typename T> reversion_wrapper<T> reverse(T && iterable) { return {iterable}; }

} // namespace

bool insert_phis(BasicBlock & block, ValueTable & values) {
    // If there is only one path into this block then we don't need to worry
    // about variables, they should already be strictly dominated in the parent
    // blocks.
    if (block.parents.empty() || is_strictly_dominated(block)) {
        return false;
    }

    /*
     * Now calculate the phi nodes
     *
     * We can't rely on all branches defining all variables (we haven't checked
     * things like, does this branch actually continue?)
     * https://github.com/dcbaker/meson-plus-plus/issues/57
     *
     * So, we need to check each parent for variables, and if they exist in more
     * than one branch we need to insert a phi node.
     *
     * XXX: What happens if a variable is erroniously undefined in a branch?
     */
    std::list<Instruction> phis{};

    // Find all phis in the block arleady, so we don't re-add them
    std::set<const Instruction *, PhiComparator> existing_phis{};
    for (const auto & obj : block.instructions) {
        if (std::holds_alternative<Phi>(*obj.obj_ptr)) {
            // XXX: this seems sketchy?
            existing_phis.emplace(&obj);
        }
    }

    // Create a set of all variables in all parents, and one of dominated variables
    // TODO: we could probably do less rewalking here
    std::set<std::string> all_vars{};
    std::set<std::string> dominated{};
    for (const auto & p : block.parents) {
        for (const auto & i : p->instructions) {
            if (auto v = i.var) {
                if (all_vars.count(v.name)) {
                    dominated.emplace(v.name);
                }
                all_vars.emplace(v.name);
            }
        }
    }

    // For variables that are dominated, create phi nodes. The first value will
    // be a phi of two parent values, but any additional phis will be either the
    // previous phi or a parent value.
    for (const auto & name : dominated) {
        uint32_t last = 0;
        for (const auto p : block.parents) {
            for (const auto & i : reverse(p->instructions)) {
                if (const auto & var = i.var) {
                    if (last) {
                        auto phi = Instruction{Phi{last, var.gvn}, Variable{name}};
                        if (!existing_phis.count(&phi)) {
                            // Only set the version if we're actually using this phi
                            phi.var.gvn = ++values[name];
                            last = phi.var.gvn;
                            phis.emplace_back(phi);
                        }
                    } else {
                        last = var.gvn;
                    }
                    break;
                }
            }
        }
    }

    if (phis.empty()) {
        return false;
    }

    block.instructions.splice(block.instructions.begin(), phis);
    return true;
}

bool fixup_phis(BasicBlock & block) {
    bool progress = false;
    for (auto it = block.instructions.begin(); it != block.instructions.end(); ++it) {
        if (std::holds_alternative<Phi>(*it->obj_ptr)) {
            const auto & phi = std::get<Phi>(*it->obj_ptr);
            bool right = false;
            bool left = false;
            for (const auto & p : block.parents) {
                for (const Instruction & i : p->instructions) {
                    const auto & var = i.var;
                    if (var.name == it->var.name) {
                        if (var.gvn == phi.left) {
                            left = true;
                            break;
                        }
                        if (var.gvn == phi.right) {
                            right = true;
                            break;
                        }
                    }
                }
                if (left && right) {
                    break;
                }
            }

            if (left ^ right) {
                progress = true;
                auto id =
                    Instruction{Identifier{it->var.name, left ? phi.left : phi.right}, it->var};
                it = block.instructions.erase(it);
                it = block.instructions.emplace(it, std::move(id));
                continue;
            }

            // While we are walking the instructions in this block, we know that
            // if one side was found, then the other is found that the first
            // found is dead code after the second, so we can ignore it and
            // treat the second one as the truth
            for (auto it2 = block.instructions.begin(); it2 != it; ++it2) {
                if (it->var.name == it2->var.name) {
                    left = it2->var.gvn == phi.left;
                    right = it2->var.gvn == phi.right;
                }
            }

            if (left ^ right) {
                progress = true;
                auto id =
                    Instruction{Identifier{it->var.name, left ? phi.left : phi.right}, it->var};
                it = block.instructions.erase(it);
                it = block.instructions.emplace(it, std::move(id));
            }
        }
    }
    return progress;
}

} // namespace MIR::Passes
