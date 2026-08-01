/// @file log.cpp
/// @brief Bounded-output logging implementation.
///
/// Uses fputs() only — no format strings, no heap allocation.
/// When VKSC_ENABLE_LOGGING is not defined all *Impl bodies are empty so
/// the linker strips them and no logging text reaches the production binary.

#include "log.hpp"

#include "safety_macros.hpp"

#include <cstdio>

namespace engine {
namespace log {

#if defined(VKSC_ENABLE_LOGGING)

namespace {

/// @brief Write prefix + message + newline to @p stream.
///
/// The (void) casts are the MISRA-compliant way to discard the fputs()
/// return values (Rule 0.1.2, permitted by the Rule 8.2.2 void-cast
/// exception): a logging failure is non-recoverable and must not abort
/// the engine.  Use of the C stdio facility itself is deviated below.
void Write(const char* const prefix,
           const char* const msg,
           FILE* const       stream) noexcept
{
    MISRA_DEVIATION("Rule 30.0.1",
                    "C library I/O (fputs/stdout/stderr) is used for debug "
                    "logging. This code is compiled out of production builds "
                    "(VKSC_ENABLE_LOGGING is never defined there); no file "
                    "streams are opened and no format strings are used.");

    (void)fputs(prefix, stream);
    (void)fputs(msg,    stream);
    (void)fputs("\n",   stream);
}

} /* anonymous namespace */

void InfoImpl (const char* const msg) noexcept { Write("[INFO]  ", msg, stdout); }
void WarnImpl (const char* const msg) noexcept { Write("[WARN]  ", msg, stderr); }
void ErrorImpl(const char* const msg) noexcept { Write("[ERROR] ", msg, stderr); }

#else  /* VKSC_ENABLE_LOGGING not defined — production build */

// Empty definitions satisfy the ODR for translation units that have the
// *Impl declarations visible; the linker will discard them as unreferenced.
void InfoImpl (const char* /*msg*/) noexcept {}
void WarnImpl (const char* /*msg*/) noexcept {}
void ErrorImpl(const char* /*msg*/) noexcept {}

#endif /* VKSC_ENABLE_LOGGING */

} /* namespace log */
} /* namespace engine */
