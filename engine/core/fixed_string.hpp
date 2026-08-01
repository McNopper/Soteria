/// @file fixed_string.hpp
/// @brief Compile-time-capacity string builder — no heap, no C-library calls.
///
/// FixedString<N> accumulates a null-terminated character sequence of at most
/// N-1 characters in a stack-allocated buffer.  All append operations
/// truncate silently on capacity exhaustion so the buffer is always valid.
///
/// Integer and float conversions use only arithmetic operations — no C-library
/// string or I/O functions are involved.
///
/// Primary use: constructing log messages inside `if constexpr (log::kEnabled)`
/// blocks so the entire message-building path is compiled away in production.
///
/// MISRA C++:2023 Rule 7.0.3: the numerical value of a character is never
/// used — decimal digits are produced via a lookup table, not '0' + n.

#ifndef VKSC_ENGINE_CORE_FIXED_STRING_HPP
#define VKSC_ENGINE_CORE_FIXED_STRING_HPP

#include <array>
#include <cmath>    // std::isfinite
#include <cstdint>

namespace engine {
namespace log {

/// @brief Bounded string builder with compile-time stack storage.
///
/// @tparam kCapacity  Total buffer size including the null terminator (>= 2).
template<uint32_t kCapacity>
class FixedString
{
    static_assert(kCapacity >= 2U, "FixedString capacity must be at least 2");

public:
    FixedString() noexcept = default;

    FixedString(const FixedString&)            = delete;
    FixedString& operator=(const FixedString&) = delete;
    FixedString(FixedString&&)                 = delete;
    FixedString& operator=(FixedString&&)      = delete;

    // -------------------------------------------------------------------------
    // Append helpers — all return *this for chaining.
    // -------------------------------------------------------------------------

    /// @brief Append a null-terminated string.  Truncates at capacity.
    FixedString& Append(const char* const str) noexcept
    {
        if (str != nullptr)
        {
            const char* p = str;
            while ((*p != '\0') && (m_len < (kCapacity - 1U)))
            {
                m_buf[m_len++] = *p;
                ++p;
            }
            m_buf[m_len] = '\0';
        }
        return *this;
    }

    /// @brief Append a signed 32-bit decimal integer.
    FixedString& AppendIDec(int32_t val) noexcept
    {
        if (val < 0)
        {
            Append("-");
            // Two-step unsigned negation avoids INT32_MIN overflow:
            //   -(val+1) is valid for all negative int32_t, then +1U in
            //   unsigned domain yields the correct absolute value.
            const uint32_t uval = static_cast<uint32_t>(-(val + 1)) + 1U;
            AppendUDec(uval);
        }
        else
        {
            AppendUDec(static_cast<uint32_t>(val));
        }
        return *this;
    }

    /// @brief Append an unsigned 32-bit decimal integer.
    FixedString& AppendUDec(uint32_t val) noexcept
    {
        // uint32_t max = 4,294,967,295 — up to 10 decimal digits.
        std::array<char, 10U> tmp{};
        uint32_t pos{0U};

        if (val == 0U)
        {
            tmp[pos++] = '0';
        }
        else
        {
            while ((val != 0U) && (pos < tmp.size()))
            {
                tmp[pos++] = kDecChars[val % 10U];
                val /= 10U;
            }
        }
        // Digits accumulated in reverse order — copy in reverse.
        for (uint32_t i{pos}; i > 0U; --i)
        {
            if (m_len < (kCapacity - 1U))
            {
                m_buf[m_len++] = tmp[i - 1U];
            }
        }
        m_buf[m_len] = '\0';
        return *this;
    }

    /// @brief Append an unsigned 64-bit decimal integer.
    FixedString& AppendU64Dec(uint64_t val) noexcept
    {
        // uint64_t max = 18,446,744,073,709,551,615 — up to 20 decimal digits.
        std::array<char, 20U> tmp{};
        uint32_t pos{0U};

        if (val == 0U)
        {
            tmp[pos++] = '0';
        }
        else
        {
            while ((val != 0U) && (pos < tmp.size()))
            {
                tmp[pos++] = kDecChars[val % 10ULL];
                val /= 10ULL;
            }
        }
        for (uint32_t i{pos}; i > 0U; --i)
        {
            if (m_len < (kCapacity - 1U))
            {
                m_buf[m_len++] = tmp[i - 1U];
            }
        }
        m_buf[m_len] = '\0';
        return *this;
    }

    /// @brief Append a zero-padded uppercase hex representation of @p val.
    ///
    /// @param val     Value to format.
    /// @param digits  Number of hex digits to emit (1–8).
    FixedString& AppendHex(uint32_t val, uint32_t digits) noexcept
    {
        static constexpr std::array<char, 17U> kHexChars{"0123456789ABCDEF"};
        // Clamp to valid range; max 8 hex digits for a 32-bit value.
        const uint32_t d = (digits > 8U) ? 8U : ((digits == 0U) ? 1U : digits);
        for (uint32_t shift{d}; shift > 0U; --shift)
        {
            const uint32_t nibble = (val >> ((shift - 1U) * 4U)) & 0xFU;
            if (m_len < (kCapacity - 1U))
            {
                m_buf[m_len++] = kHexChars[nibble];
            }
        }
        m_buf[m_len] = '\0';
        return *this;
    }

    /// @brief Append a finite float value with @p decimals decimal places.
    ///
    /// Non-finite values (NaN, Inf) are rendered as "?" to remain visible
    /// in diagnostics.  In a correctly operating system these should never
    /// appear because AttitudeData::valid must be checked before use.
    ///
    /// @param val       Value to format.
    /// @param decimals  Number of digits after the decimal point (0–4).
    /// @param showSign  Always prepend '+' or '-'.
    FixedString& AppendFloat(float val, uint32_t decimals, bool showSign) noexcept
    {
        if (!std::isfinite(val))
        {
            return Append("?");
        }

        const bool negative = (val < 0.0F);
        if (negative)
        {
            Append("-");
        }
        else if (showSign)
        {
            Append("+");
        }
        else
        {
            // no sign prefix
        }

        const float absVal = negative ? -val : val;

        // Compute the decimal scale factor (10^decimals).
        uint32_t scale{1U};
        const uint32_t clampedDec = (decimals > 4U) ? 4U : decimals;
        for (uint32_t i{0U}; i < clampedDec; ++i)
        {
            scale *= 10U;
        }

        const uint32_t intPart  = static_cast<uint32_t>(absVal);
        uint32_t fracPart =
            static_cast<uint32_t>(
                (absVal - static_cast<float>(intPart)) * static_cast<float>(scale)
                + 0.5F);

        // Propagate carry if rounding pushed fracPart to scale.
        uint32_t adjustedInt = intPart;
        if (fracPart >= scale)
        {
            fracPart -= scale;
            ++adjustedInt;
        }

        AppendUDec(adjustedInt);

        if (clampedDec > 0U)
        {
            Append(".");
            // Left-pad the fractional part with zeros if needed.  The final
            // digit always comes from AppendUDec (even when fracPart == 0),
            // so the loop must stop at padScale == 1 to avoid one zero too
            // many (e.g. 9.99 with 1 decimal → "10.0", not "10.00").
            uint32_t padScale = scale / 10U;
            while ((padScale > 1U) && (fracPart < padScale))
            {
                Append("0");
                padScale /= 10U;
            }
            AppendUDec(fracPart);
        }

        return *this;
    }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /// @returns Null-terminated string; never nullptr.
    [[nodiscard]] const char* CStr() const noexcept { return m_buf.data(); }

    /// @returns Current length (excluding the null terminator).
    [[nodiscard]] uint32_t Len() const noexcept { return m_len; }

private:
    /// Decimal digit characters as a lookup table.
    ///
    /// Used instead of '0' + n arithmetic so the numerical value of a
    /// character is never used (MISRA C++:2023 Rule 7.0.3).
    static constexpr std::array<char, 10U> kDecChars{
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

    std::array<char, kCapacity> m_buf{};
    uint32_t                    m_len{0U};
};

} /* namespace log */
} /* namespace engine */

#endif /* VKSC_ENGINE_CORE_FIXED_STRING_HPP */
