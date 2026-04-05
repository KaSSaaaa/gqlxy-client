#include <gtest/gtest.h>
#include "server_fixture.h"

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new gqlxy::e2e::ServerEnvironment());
    return RUN_ALL_TESTS();
}
