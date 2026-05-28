#include <gtest/gtest.h>
#include "engine/core/result.hpp"

namespace engine {

TEST(ResultTest, IsOkReturnsTrueForKOk) {
    EXPECT_TRUE(IsOk(Result::kOk));
}

TEST(ResultTest, IsOkReturnsFalseForError) {
    EXPECT_FALSE(IsOk(Result::kError));
}

TEST(ResultTest, ResultToStringReturnsCorrectString) {
    EXPECT_STREQ(ResultToString(Result::kOk), "Ok");
    EXPECT_STREQ(ResultToString(Result::kVkscInstanceFailed), "VkscInstanceFailed");
    EXPECT_STREQ(ResultToString(static_cast<Result>(999)), "Unknown");
}

}