// SPDX-License-Identifier: Apache-2.0
// Copyright © 2021-2024 Intel Corporation
// Copyright © 2021-2024 Intel Corporation

#include <gtest/gtest.h>

#include "archiver.hpp"

TEST(detect_archivers, gnu) {
    // TODO: there are multiple binaries called ar, how can we be sure we're
    // getting the one we expect?

    // Skip if we don't have g++
    if (system("ar") == 127) {
        GTEST_SKIP();
    }
    const auto comp =
        MIR::Toolchain::Archiver::detect_archiver(MIR::Machines::Machine::BUILD, {"ar"});
    ASSERT_NE(comp, nullptr);
    ASSERT_EQ(comp->id(), "gnu");
}
