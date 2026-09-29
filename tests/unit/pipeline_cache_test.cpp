/// @file pipeline_cache_test.cpp
/// @brief Unit tests for engine::rendering::PipelineCacheSc.
///
/// These tests exercise only input-validation paths and are fully offline
/// (no VulkanSC device required).  Tests that exercise the success path
/// with a real device are in tests/integration/.

#include <gtest/gtest.h>
#include "engine/rendering/pipeline_cache.hpp"
#include "app/rendering/pipeline_cache_data.hpp"

#include <array>

namespace engine::rendering {

TEST(PipelineCacheScTest, HandleIsNullBeforeInit)
{
    PipelineCacheSc cache;
    EXPECT_EQ(cache.Handle(), VK_NULL_HANDLE);
}

TEST(PipelineCacheScTest, InitFailsWithNullDevice)
{
    PipelineCacheSc cache;
    uint8_t data[8]{0xAB, 0xCD, 0xEF, 0x01, 0x02, 0x03, 0x04, 0x05};
    const VkPhysicalDevice pd = VK_NULL_HANDLE;
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, pd, data, sizeof(data)), Result::kInvalidArgument);
}

TEST(PipelineCacheScTest, InitFailsWithNullData)
{
    // Null data pointer is rejected even when device would be valid.
    // (Here device is VK_NULL_HANDLE too, but null data check is independent.)
    PipelineCacheSc cache;
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr, 8U), Result::kInvalidArgument);
}

TEST(PipelineCacheScTest, InitFailsWithZeroDataSize)
{
    PipelineCacheSc cache;
    uint8_t data[4]{};
    EXPECT_EQ(cache.Init(VK_NULL_HANDLE, VK_NULL_HANDLE, data, 0U), Result::kInvalidArgument);
}

TEST(PipelineCacheScTest, ShutdownOnUninitIsNop)
{
    // Shutdown on an uninitialised cache must not crash; handle stays null.
    PipelineCacheSc cache;
    cache.Shutdown(VK_NULL_HANDLE);
    EXPECT_EQ(cache.Handle(), VK_NULL_HANDLE);
}

// ---------------------------------------------------------------------------
// ValidatePipelineCacheHeader — synthetic blobs, fully offline
// ---------------------------------------------------------------------------

namespace {

/// Build a well-formed 56-byte SC header + index-area padding.
/// Layout per VkPipelineCacheHeaderVersionSafetyCriticalOne (little-endian).
void WriteLe32(std::array<uint8_t, 128U>& b, const uint32_t off, const uint32_t v)
{
    b[off]      = static_cast<uint8_t>(v & 0xFFU);
    b[off + 1U] = static_cast<uint8_t>((v >> 8U) & 0xFFU);
    b[off + 2U] = static_cast<uint8_t>((v >> 16U) & 0xFFU);
    b[off + 3U] = static_cast<uint8_t>((v >> 24U) & 0xFFU);
}

std::array<uint8_t, 128U> MakeValidBlob(const uint32_t vendorId, const uint32_t deviceId)
{
    std::array<uint8_t, 128U> b{};
    WriteLe32(b,  0U, 56U);  // headerSize
    WriteLe32(b,  4U, static_cast<uint32_t>(VK_PIPELINE_CACHE_HEADER_VERSION_SAFETY_CRITICAL_ONE));
    WriteLe32(b,  8U, vendorId);
    WriteLe32(b, 12U, deviceId);
    WriteLe32(b, 32U, static_cast<uint32_t>(VK_PIPELINE_CACHE_VALIDATION_VERSION_SAFETY_CRITICAL_ONE));
    WriteLe32(b, 40U, 1U);   // pipelineIndexCount
    WriteLe32(b, 44U, 56U);  // pipelineIndexStride
    // pipelineIndexOffset = 56 (bytes 48..55, LE) → 56 + 1*56 = 112 <= 128
    WriteLe32(b, 48U, 56U);
    WriteLe32(b, 52U, 0U);
    return b;
}

} /* anonymous namespace */

TEST(PipelineCacheHeaderTest, ValidHeaderIsAccepted)
{
    const auto blob = MakeValidBlob(0x1111U, 0x2222U);
    EXPECT_EQ(ValidatePipelineCacheHeader(blob.data(), static_cast<uint32_t>(blob.size()),
                                          0x1111U, 0x2222U),
              Result::kOk);
}

TEST(PipelineCacheHeaderTest, NullDataRejected)
{
    EXPECT_EQ(ValidatePipelineCacheHeader(nullptr, 128U, 0U, 0U), Result::kVkscCacheInvalid);
}

TEST(PipelineCacheHeaderTest, TooSmallRejected)
{
    const auto blob = MakeValidBlob(0U, 0U);
    EXPECT_EQ(ValidatePipelineCacheHeader(blob.data(), 40U, 0U, 0U), Result::kVkscCacheInvalid);
}

TEST(PipelineCacheHeaderTest, WrongHeaderVersionRejected)
{
    auto blob = MakeValidBlob(0U, 0U);
    WriteLe32(blob, 4U, 1U);  // base-Vulkan VK_PIPELINE_CACHE_HEADER_VERSION_ONE
    EXPECT_EQ(ValidatePipelineCacheHeader(blob.data(), static_cast<uint32_t>(blob.size()),
                                          0U, 0U),
              Result::kVkscCacheInvalid);
}

TEST(PipelineCacheHeaderTest, VendorMismatchAcceptedWithWarning)
{
    // Emulation-compiled caches legitimately carry PCC-stamped IDs that
    // differ from the runtime physical device — structural validity governs.
    const auto blob = MakeValidBlob(0x1111U, 0x2222U);
    EXPECT_EQ(ValidatePipelineCacheHeader(blob.data(), static_cast<uint32_t>(blob.size()),
                                          0x3333U, 0x2222U),
              Result::kOk);
}

TEST(PipelineCacheHeaderTest, DeviceMismatchAcceptedWithWarning)
{
    const auto blob = MakeValidBlob(0x1111U, 0x2222U);
    EXPECT_EQ(ValidatePipelineCacheHeader(blob.data(), static_cast<uint32_t>(blob.size()),
                                          0x1111U, 0x4444U),
              Result::kOk);
}

TEST(PipelineCacheHeaderTest, IndexOverflowRejected)
{
    auto blob = MakeValidBlob(0U, 0U);
    WriteLe32(blob, 40U, 100U);  // 56 + 100*56 >> 128 bytes
    EXPECT_EQ(ValidatePipelineCacheHeader(blob.data(), static_cast<uint32_t>(blob.size()),
                                          0U, 0U),
              Result::kVkscCacheInvalid);
}

TEST(PipelineCacheHeaderTest, StrideBelowMinimumRejected)
{
    auto blob = MakeValidBlob(0U, 0U);
    WriteLe32(blob, 44U, 32U);
    EXPECT_EQ(ValidatePipelineCacheHeader(blob.data(), static_cast<uint32_t>(blob.size()),
                                          0U, 0U),
              Result::kVkscCacheInvalid);
}

TEST(PipelineCacheHeaderTest, WrongValidationVersionRejected)
{
    auto blob = MakeValidBlob(0U, 0U);
    WriteLe32(blob, 32U, 0xDEADU);
    EXPECT_EQ(ValidatePipelineCacheHeader(blob.data(), static_cast<uint32_t>(blob.size()),
                                          0U, 0U),
              Result::kVkscCacheInvalid);
}

TEST(PipelineCacheHeaderTest, RealEmbeddedCacheIsWellFormed)
{
    // The shipped blob must always pass structural validation (vendor/device
    // IDs are driver-specific, so check structure only via a self-match).
    // This guards against a corrupted/truncated pipeline_cache_data.hpp.
    ASSERT_GE(sim::kPipelineCacheDataSize, 56U);
    const uint32_t vendorId =
        static_cast<uint32_t>(sim::kPipelineCacheData[8])        |
       (static_cast<uint32_t>(sim::kPipelineCacheData[9])  <<  8U) |
       (static_cast<uint32_t>(sim::kPipelineCacheData[10]) << 16U) |
       (static_cast<uint32_t>(sim::kPipelineCacheData[11]) << 24U);
    const uint32_t deviceId =
        static_cast<uint32_t>(sim::kPipelineCacheData[12])        |
       (static_cast<uint32_t>(sim::kPipelineCacheData[13]) <<  8U) |
       (static_cast<uint32_t>(sim::kPipelineCacheData[14]) << 16U) |
       (static_cast<uint32_t>(sim::kPipelineCacheData[15]) << 24U);
    EXPECT_EQ(ValidatePipelineCacheHeader(sim::kPipelineCacheData,
                                          sim::kPipelineCacheDataSize,
                                          vendorId, deviceId),
              Result::kOk);
}

} /* namespace engine::rendering */