// SPDX-License-Identifier: Apache-2.0
// Copyright © 2021-2024 Intel Corporation

#include "exceptions.hpp"
#include "passes.hpp"
#include "private.hpp"

namespace MIR::Passes {

bool delete_unreachable(std::shared_ptr<CFGNode> block) {
    // If we see an Message object that is an error, that block will not return,
    // break it's next connection
    for (auto itr = block->block->instructions.begin(); itr != block->block->instructions.end();
         ++itr) {
        if (auto * m = std::get_if<Message>(itr->obj_ptr.get())) {
            if (m->level == MessageLevel::ERROR) {
                bool progress = false;

                // Delete any children point to this block
                if (std::holds_alternative<std::shared_ptr<CFGNode>>(block->next)) {
                    auto & b = std::get<std::shared_ptr<CFGNode>>(block->next);
                    unlink_nodes(block, b);
                    progress = true;
                } else if (std::holds_alternative<std::unique_ptr<Condition>>(block->next)) {
                    const auto & con = *std::get<std::unique_ptr<Condition>>(block->next);
                    for (const auto & c : {con.if_true, con.if_false}) {
                        unlink_nodes(block, c);
                    }
                    progress = true;
                }

                // Set next to monostate, there is nothing after this
                if (!std::holds_alternative<std::monostate>(block->next)) {
                    progress = true;
                    block->next = std::monostate{};
                }

                if (++itr != block->block->instructions.end()) {
                    // Delete all instructions after this message, they don't actually exists
                    // This may delete additional errors, but we can't be sure
                    // they're not spurious or caused by the first error
                    block->block->instructions.erase(itr, block->block->instructions.end());
                    progress = true;
                }

                return progress;
            }
        }
    }

    return false;
}

} // namespace MIR::Passes
