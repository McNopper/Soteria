/// @file safety_macros.hpp
/// @brief Safety-critical utility macros for MISRA deviation tracking.
///
/// Every public deviation from MISRA C++:2023 MUST be annotated with
/// MISRA_DEVIATION at the point of use, providing the rule identifier and
/// a rationale.  This creates a traceable deviation record that commercial
/// static-analysis tools (e.g. Polyspace, Helix QAC, CodeSonar) can parse.
///
/// @satisfies   SWS_SafetyMacros_001  All MISRA deviations are annotated.
/// @verifiedby  UT_SafetyMacros_001

#ifndef VKSC_ENGINE_CORE_SAFETY_MACROS_HPP
#define VKSC_ENGINE_CORE_SAFETY_MACROS_HPP

/// @brief Annotate a justified MISRA C++:2023 rule deviation.
///
/// Usage:
/// @code
///   MISRA_DEVIATION("M.21.2.1", "fputs return value intentionally discarded"
///                   " — logging failure is non-recoverable.");
///   (void)fputs(msg, stdout);
/// @endcode
///
/// @param rule    String literal identifying the MISRA rule (e.g. "M.21.2.1").
/// @param reason  String literal containing the justification text.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define MISRA_DEVIATION(rule, reason) \
    static_assert(sizeof(rule) > 1U, "MISRA_DEVIATION: rule must not be empty"); \
    static_assert(sizeof(reason) > 1U, "MISRA_DEVIATION: reason must not be empty")

#endif /* VKSC_ENGINE_CORE_SAFETY_MACROS_HPP */
