/// @file log.hpp
/// @brief Zero-overhead compile-time selectable logging interface.
///
/// Define VKSC_ENABLE_LOGGING to activate runtime log output.
/// When the macro is not defined every log call compiles to an empty inline
/// function.  The compiler's dead-code elimination removes the call and its
/// string-literal argument entirely — no logging text or code reaches the
/// production binary.
///
/// Complex messages should be built with engine::log::FixedString inside
/// `if constexpr (engine::log::kEnabled)` blocks so that the message-building
/// code is also excluded from production builds.
///
/// CMake automatically defines VKSC_ENABLE_LOGGING for Debug configurations.
///
/// Constraints: no dynamic memory, no variadic format functions (MISRA
/// C++:2023 Rule 21.10.1), fully removable at compile time.

#ifndef VKSC_ENGINE_CORE_LOG_HPP
#define VKSC_ENGINE_CORE_LOG_HPP

namespace engine {
namespace log {

/// @brief Compile-time logging switch.
///
/// True only when VKSC_ENABLE_LOGGING is defined (Debug builds).
/// Use in `if constexpr (engine::log::kEnabled)` to guard message-building
/// code that should produce zero overhead in production.
inline constexpr bool kEnabled{
#if defined(VKSC_ENABLE_LOGGING)
    true
#else
    false
#endif
};

/// @cond INTERNAL
/// Implementation entry points — always defined (empty when logging is off).
/// Call the inline wrappers below; never call these directly.
void InfoImpl (const char* msg) noexcept;
void WarnImpl (const char* msg) noexcept;
void ErrorImpl(const char* msg) noexcept;
/// @endcond

/// @brief Write an informational message to stdout.
/// Compiled away entirely when kEnabled == false.
///
/// @param msg  Null-terminated string.  Must not be nullptr.
inline void Info(const char* msg) noexcept
{
    if constexpr (kEnabled) { InfoImpl(msg); }
}

/// @brief Write a warning message to stderr.
/// Compiled away entirely when kEnabled == false.
///
/// @param msg  Null-terminated string.  Must not be nullptr.
inline void Warn(const char* msg) noexcept
{
    if constexpr (kEnabled) { WarnImpl(msg); }
}

/// @brief Write an error message to stderr.
/// Compiled away entirely when kEnabled == false.
///
/// @param msg  Null-terminated string.  Must not be nullptr.
inline void Error(const char* msg) noexcept
{
    if constexpr (kEnabled) { ErrorImpl(msg); }
}

} /* namespace log */
} /* namespace engine */

#endif /* VKSC_ENGINE_CORE_LOG_HPP */
