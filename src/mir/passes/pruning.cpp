// SPDX-License-Identifier: Apache-2.0
// Copyright © 2021-2024 Intel Corporation

#include <algorithm>

#include "passes.hpp"

namespace MIR::Passes {

namespace {

bool branch_pruning_impl(std::shared_ptr<CFGNode> node) {
    // If we don't have at least 2 potential exits from this block then we don't
    // have anything to do
    if (node->successors.size() < 2) {
        return false;
    }

    bool progress = false;

    // XXX: this heavily assumes that there is one and only one way to get from
    // one node to a second node. That is not true
    for (auto it = node->block->instructions.begin(); it != node->block->instructions.end(); ++it) {
        if (Jump * j = std::get_if<Jump>(it->obj_ptr.get()); j && j->predicate) {
            if (Boolean * b = std::get_if<Boolean>(j->predicate->obj_ptr.get())) {
                if (b->value) {
                    // If this predicate is true, then we always make this jump.
                    // delete the predicate, erase all of the rest of the instructions
                    // Break all of the other links, and leave
                    j->predicate = std::make_shared<Instruction>(std::monostate{});
                    for (auto && s : node->successors) {
                        if (s != j->target) {
                            unlink_nodes(node, s);
                        }
                    }
                    it++;
                    while (it != node->block->instructions.end()) {
                        it = node->block->instructions.erase(it);
                    }
                    return true;
                } else {
                    // Otherwise, if the predicate is false, then we can unlink it's
                    // target, and remove the instruction
                    unlink_nodes(node, j->target);
                    it = node->block->instructions.erase(it);
                    progress = true;
                    continue;
                }
            }
        } else if (auto * b = std::get_if<Branch>(it->obj_ptr.get())) {
            assert(!b->branches.empty());

            for (auto bit = b->branches.begin(); bit != b->branches.end(); ++bit) {
                if (const Boolean * v = std::get_if<Boolean>(std::get<0>(*bit).obj_ptr.get())) {
                    if (v->value) {
                        // If this branch is true then we can remove all
                        // branches *after* this one, as we know that we will
                        for (bit++; bit != b->branches.end();) {
                            auto next = std::get<1>(*bit);
                            unlink_nodes(node, next);
                            bit = b->branches.erase(bit);
                            progress |= true;
                        }
                        break;
                    } else {
                        // If this branch is known to be false then we can remove it
                        unlink_nodes(node, std::get<1>(*bit));
                        bit = b->branches.erase(bit);
                        progress |= true;
                    }
                }
            }

            if (b->branches.size() == 1) {
                Jump jump{std::get<1>(b->branches.at(0))};
                it = node->block->instructions.erase(it);
                it = node->block->instructions.emplace(it, std::move(jump));
                progress |= true;
            } else if (b->branches.empty()) {
                assert(node->successors.empty());
                it = node->block->instructions.erase(it);
                progress |= true;
            }
        }
    }

    return progress;
};

} // namespace

bool branch_pruning(std::shared_ptr<CFGNode> block) {
    bool progress = false;
    bool lprogress;

    // Run this on the same block as long as it's making progress. We do this so
    // that if the new next block can also be pruned we do that with few
    // iterations.
    do {
        lprogress = branch_pruning_impl(block);
        progress |= lprogress;
    } while (lprogress);

    return progress;
}

} // namespace MIR::Passes
