// SPDX-license-identifier: Apache-2.0
// Copyright © 2021 Dylan Baker

#include "passes.hpp"

namespace MIR::Passes {

bool branch_pruning(BasicBlock * ir) {
    if (!ir->condition.has_value()) {
        return false;
    }

    const auto & con = ir->condition.value();
    if (!std::holds_alternative<std::unique_ptr<Boolean>>(con.condition)) {
        return false;
    }

    // Set the true condition to the true branch, then let join_branches worry
    // about cleaning up.
    const bool & con_v = std::get<std::unique_ptr<Boolean>>(con.condition)->value;
    auto new_v = con_v ? con.if_true : con.if_false;
    ir->next = new_v;
    ir->condition = std::nullopt;

    return true;
};

} // namespace MIR::Passes
