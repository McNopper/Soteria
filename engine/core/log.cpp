/// @file log.cpp
/// @brief Bounded-output logging implementation.
///
/// Uses fputs() only — no format strings, no heap allocation.
/// When VKSC_ENABLE_LOGGING is not defined all *Impl bodies are empty so
/// the linker strips them and no logging text reaches the production binary.
///
/// @satisfies   SWS_Log_001
/// @satisfies   SWS_Log_002
/// @satisfies   SWS_Log_005
/// @verifiedby  UT_Log_001

#include "log.hpp"

#include "safety_macros.hpp"

#include <cstdio>

namespace engine {
namespace log {

#if defined(VKSC_ENABLE_LOGGING)

namespace {

/// @brief Write prefix + message + newline to @p stream.
///
/// fputs() return values are discarded: a logging failure is non-recoverable
/// and must not abort the engine.  The MISRA deviation is documented below.
void Write(const char* const prefix,
           const char* const msg,
           FILE* const       stream) noexcept
{
    MISRA_DEVIATION("Rule 0-1-9",
                    "Return values of fputs are discarded. Logging failure is "
                    "non-recoverable; aborting on failed write would be worse.");

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
