#include "cc_wrapper_utils.h"
#include <gtest/gtest.h>
#include <vector>
#include <unordered_set>

// Test splitting logic
TEST(WrapperUtilsTest, Split) {
    std::string input = "-S,-g,-01";

    std::vector<std::string> expectedVec = {"-S", "-g", "-01"};
    auto resultVec = split<std::vector<std::string>>(input, ',');
    EXPECT_EQ(expectedVec, resultVec);

    std::unordered_set<std::string> expectedSet = {"-S", "-g", "-01"};
    auto resultSet = split<std::unordered_set<std::string>>(input, ',');
    EXPECT_EQ(expectedSet, resultSet);
}

// Test parsing arguments and WrapperArgs initialization
TEST(WrapperUtilsTest, ArgParse) {
    char *argv[] = {
        (char *)"./diffkemp-cc-wrapper",
        (char *)"--clang=clang",
        (char *)"--dbf=diffkemp.db",
        (char *)"--debug=1",
        (char *)"--cla=-foo,-bar",
        (char *)"--cld=-drop,-me",
        (char *)"--llink=llvm-link",
        (char *)"--lldis=llvm-dis",
        (char *)"--noo=1",
        (char *)"--",
        (char *)"-std=c99",
        (char *)"main.c"
    };

    WrapperArgs wArgs(12, argv);

    // Verify simple string fields
    EXPECT_EQ(wArgs.clang, "clang");
    EXPECT_EQ(wArgs.dbFilename, "diffkemp.db");
    EXPECT_EQ(wArgs.llvmLink, "llvm-link");
    EXPECT_EQ(wArgs.llvmDis, "llvm-dis");

    // Verify boolean flags
    EXPECT_TRUE(wArgs.debug);
    EXPECT_TRUE(wArgs.noOptOverride);

    // Verify container fields (cla/cld)
    std::vector<std::string> expectedAppend = {"-foo", "-bar"};
    EXPECT_EQ(wArgs.clangAppend, expectedAppend);
    
    EXPECT_NE(wArgs.clangDrop.find("-drop"), wArgs.clangDrop.end());
    EXPECT_NE(wArgs.clangDrop.find("-me"), wArgs.clangDrop.end());

    // Verify post-delimiter arguments
    ASSERT_EQ(wArgs.clangArgs.size(), 2);
    EXPECT_EQ(wArgs.clangArgs[0], "-std=c99");
    EXPECT_EQ(wArgs.clangArgs[1], "main.c");
}
