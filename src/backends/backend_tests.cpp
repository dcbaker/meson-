// SPDX-License-Indentifier: Apache-2.0
// Copyright © 2024 Intel Corporation

#include "common/backend.hpp"

#include <gtest/gtest.h>

using namespace Backends;

TEST(Test_serialization, serialize) {
    Common::Test test{"foo", "/foo"};
    std::ostringstream stream{};
    test.serialize(stream);
    ASSERT_EQ(stream.str(), "BEGIN_TEST\n  name:foo\n  exe:/foo\nEND_TEST\n");
}

TEST(Test_serialization, deserialize) {
    std::istringstream str{"SERIAL_VERSION:0\nBEGIN_TEST\n  name:foo\n  exe:/foo\nEND_TEST\n"};
    auto && tests = Common::deserialize_tests(str);
    EXPECT_EQ(tests.size(), 1);

    auto && test = tests.at(0);
    EXPECT_EQ(test.name, "foo");
    ASSERT_EQ(test.exe, "/foo");
}