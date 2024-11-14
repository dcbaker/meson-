// SPDX-License-Identifier: Apache-2.0
// Copyright © 2021-2024 Intel Corporation

#include <gtest/gtest.h>

#include "arguments.hpp"
#include "exceptions.hpp"
#include "passes.hpp"
#include "passes/private.hpp"
#include "state/state.hpp"
#include "toolchains/archiver.hpp"
#include "toolchains/common.hpp"
#include "toolchains/compilers/cpp/cpp.hpp"
#include "toolchains/linker.hpp"

#include "test_utils.hpp"

namespace {

using ToolchainMap =
    std::unordered_map<MIR::Toolchain::Language,
                       MIR::Machines::PerMachine<std::shared_ptr<MIR::Toolchain::Toolchain>>>;

bool wrapper(std::shared_ptr<MIR::CFGNode> & node, const ToolchainMap & tc) {
    return MIR::Passes::instruction_walker(*node, {[&tc](const MIR::Instruction & obj) {
        return MIR::Passes::insert_compilers(obj, tc);
    }});
}

} // namespace

TEST(insert_compiler, simple) {
    const std::vector<std::string> init{"null"};
    auto comp = std::make_unique<MIR::Toolchain::Compiler::CPP::Clang>(init);
    auto tc = std::make_shared<MIR::Toolchain::Toolchain>(
        std::move(comp),
        std::make_unique<MIR::Toolchain::Linker::Drivers::Gnu>(MIR::Toolchain::Linker::GnuBFD{init},
                                                               comp.get()),
        std::make_unique<MIR::Toolchain::Archiver::Gnu>(init));
    ToolchainMap tc_map{};
    tc_map[MIR::Toolchain::Language::CPP] =
        MIR::Machines::PerMachine<std::shared_ptr<MIR::Toolchain::Toolchain>>{tc};

    auto irlist = lower("x = meson.get_compiler('cpp')");
    bool progress = wrapper(irlist, tc_map);
    ASSERT_TRUE(progress);
    ASSERT_EQ(irlist->block->instructions.size(), 1);

    const auto & e = irlist->block->instructions.front();
    ASSERT_TRUE(std::holds_alternative<MIR::Compiler>(*e.obj_ptr));

    const auto & c = std::get<MIR::Compiler>(*e.obj_ptr);
    ASSERT_EQ(c.toolchain->compiler->id(), "clang");
}

TEST(insert_compiler, unknown_language) {
    ToolchainMap tc_map{};

    auto irlist = lower("x = meson.get_compiler('cpp')");
    try {
        (void)wrapper(irlist, tc_map);
        FAIL();
    } catch (Util::Exceptions::MesonException & e) {
        std::string m{e.what()};
        ASSERT_EQ(m, "No compiler for language");
    }
}
