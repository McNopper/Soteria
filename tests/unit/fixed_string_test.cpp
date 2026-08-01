/// @file fixed_string_test.cpp
/// @brief Unit tests for engine::log::FixedString.
///
/// FixedString is the log-message builder: no heap, no C-library string
/// functions, silent truncation at capacity.  These tests run fully offline.

#include <gtest/gtest.h>
#include "engine/core/fixed_string.hpp"

#include <cstring>

namespace engine {
namespace log {

// ---------------------------------------------------------------------------
// Basic append / capacity
// ---------------------------------------------------------------------------

TEST(FixedStringTest, DefaultIsEmptyAndNullTerminated)
{
    FixedString<16U> s;
    EXPECT_STREQ(s.CStr(), "");
    EXPECT_EQ(s.Len(), 0U);
}

TEST(FixedStringTest, AppendString)
{
    FixedString<32U> s;
    s.Append("hello");
    EXPECT_STREQ(s.CStr(), "hello");
    EXPECT_EQ(s.Len(), 5U);
}

TEST(FixedStringTest, AppendNullptrIsNoOp)
{
    FixedString<16U> s;
    s.Append(nullptr);
    EXPECT_STREQ(s.CStr(), "");
    EXPECT_EQ(s.Len(), 0U);
}

TEST(FixedStringTest, AppendTruncatesAtCapacity)
{
    FixedString<6U> s;  // capacity 6 → max 5 chars + NUL
    s.Append("abcdefghij");
    EXPECT_STREQ(s.CStr(), "abcde");
    EXPECT_EQ(s.Len(), 5U);
}

TEST(FixedStringTest, AppendAfterTruncationStaysValid)
{
    FixedString<6U> s;
    s.Append("abcdef").Append("gh");
    EXPECT_STREQ(s.CStr(), "abcde");
    EXPECT_EQ(s.Len(), 5U);
}

// ---------------------------------------------------------------------------
// Integer formatting
// ---------------------------------------------------------------------------

TEST(FixedStringTest, AppendUDecZero)
{
    FixedString<16U> s;
    s.AppendUDec(0U);
    EXPECT_STREQ(s.CStr(), "0");
}

TEST(FixedStringTest, AppendUDecMax)
{
    FixedString<16U> s;
    s.AppendUDec(4294967295U);
    EXPECT_STREQ(s.CStr(), "4294967295");
}

TEST(FixedStringTest, AppendIDecPositive)
{
    FixedString<16U> s;
    s.AppendIDec(12345);
    EXPECT_STREQ(s.CStr(), "12345");
}

TEST(FixedStringTest, AppendIDecNegative)
{
    FixedString<16U> s;
    s.AppendIDec(-42);
    EXPECT_STREQ(s.CStr(), "-42");
}

TEST(FixedStringTest, AppendIDecInt32Min)
{
    FixedString<16U> s;
    s.AppendIDec(INT32_MIN);  // -2147483648 — must not overflow during negation
    EXPECT_STREQ(s.CStr(), "-2147483648");
}

TEST(FixedStringTest, AppendU64DecMax)
{
    FixedString<32U> s;
    s.AppendU64Dec(18446744073709551615ULL);
    EXPECT_STREQ(s.CStr(), "18446744073709551615");
}

// ---------------------------------------------------------------------------
// Hex formatting
// ---------------------------------------------------------------------------

TEST(FixedStringTest, AppendHexZeroPadded)
{
    FixedString<16U> s;
    s.AppendHex(0x0000ABCDU, 8U);
    EXPECT_STREQ(s.CStr(), "0000ABCD");
}

TEST(FixedStringTest, AppendHexClampsDigitsToEight)
{
    FixedString<16U> s;
    s.AppendHex(0xDEADBEEFU, 12U);
    EXPECT_STREQ(s.CStr(), "DEADBEEF");
}

TEST(FixedStringTest, AppendHexSingleDigit)
{
    FixedString<16U> s;
    s.AppendHex(0xFU, 1U);
    EXPECT_STREQ(s.CStr(), "F");
}

// ---------------------------------------------------------------------------
// Float formatting
// ---------------------------------------------------------------------------

TEST(FixedStringTest, AppendFloatPositive)
{
    FixedString<32U> s;
    s.AppendFloat(3.25F, 2U, false);
    EXPECT_STREQ(s.CStr(), "3.25");
}

TEST(FixedStringTest, AppendFloatNegative)
{
    FixedString<32U> s;
    s.AppendFloat(-12.5F, 1U, false);
    EXPECT_STREQ(s.CStr(), "-12.5");
}

TEST(FixedStringTest, AppendFloatForcedSign)
{
    FixedString<32U> s;
    s.AppendFloat(7.5F, 2U, true);
    EXPECT_STREQ(s.CStr(), "+7.50");
}

TEST(FixedStringTest, AppendFloatRoundsCarryIntoIntPart)
{
    FixedString<32U> s;
    s.AppendFloat(9.99F, 1U, false);  // rounds to 10.0
    EXPECT_STREQ(s.CStr(), "10.0");
}

TEST(FixedStringTest, AppendFloatZeroDecimals)
{
    FixedString<32U> s;
    s.AppendFloat(4.6F, 0U, false);
    EXPECT_STREQ(s.CStr(), "5");
}

TEST(FixedStringTest, AppendFloatLeadingZeroFraction)
{
    FixedString<32U> s;
    s.AppendFloat(1.05F, 2U, false);
    EXPECT_STREQ(s.CStr(), "1.05");
}

// ---------------------------------------------------------------------------
// Chaining
// ---------------------------------------------------------------------------

TEST(FixedStringTest, ChainedAppends)
{
    FixedString<64U> s;
    s.Append("roll=").AppendFloat(-25.0F, 1U, true).Append(" deg #").AppendU64Dec(60ULL);
    EXPECT_STREQ(s.CStr(), "roll=-25.0 deg #60");
}

} /* namespace log */
} /* namespace engine */
