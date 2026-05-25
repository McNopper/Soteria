/// @file i_memory_allocator.hpp
/// @brief Replaceable memory allocator interface.
///
/// The engine's allocator strategy is dependency-injected.  Production builds
/// may supply a certified, statically-bounded allocator; simulation builds may
/// use the system allocator.  This file contains the interface only — one
/// class per header, per coding rules.
///
/// @satisfies   SWS_Memory_001  The allocator is replaceable at construction time.
/// @verifiedby  UT_Memory_001

#ifndef VKSC_ENGINE_CORE_I_MEMORY_ALLOCATOR_HPP
#define VKSC_ENGINE_CORE_I_MEMORY_ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>

namespace engine {

/// @brief Abstract allocator interface injected into engine subsystems.
///
/// Implementors must guarantee that Alloc / Free are safe to call from any
/// thread simultaneously (i.e. internally synchronised).
class IMemoryAllocator
{
public:
    IMemoryAllocator()                                       = default;
    virtual ~IMemoryAllocator()                              = default;

    IMemoryAllocator(const IMemoryAllocator&)            = delete;
    IMemoryAllocator& operator=(const IMemoryAllocator&) = delete;
    IMemoryAllocator(IMemoryAllocator&&)                 = delete;
    IMemoryAllocator& operator=(IMemoryAllocator&&)      = delete;

    /// @brief Allocate @p size bytes aligned to @p alignment.
    ///
    /// @returns  Non-null pointer on success; nullptr on failure.
    [[nodiscard]] virtual void* Alloc(size_t size, size_t alignment) noexcept = 0;

    /// @brief Release a block previously returned by Alloc.
    ///
    /// Passing nullptr is defined as a no-op (mirrors free() semantics).
    virtual void Free(void* ptr) noexcept = 0;
};

} /* namespace engine */

#endif /* VKSC_ENGINE_CORE_I_MEMORY_ALLOCATOR_HPP */
