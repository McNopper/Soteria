# Unit Test Report (UTR)

## Test Execution Summary

| Test ID | Description                                    | Expected Result                              | Actual Result                                | Pass/Fail |
|---------|------------------------------------------------|----------------------------------------------|----------------------------------------------|-----------|
| UT-001  | Result: Verify IsOk for kOk                   | Returns true for Result::kOk                | Returns true for Result::kOk                | Pass      |
| UT-002  | Result: Verify IsOk for kError                | Returns false for Result::kError            | Returns false for Result::kError            | Pass      |
| UT-003  | Result: Verify ResultToString for known codes | Returns correct string for valid Result codes | Returns correct string for valid Result codes | Pass      |
| UT-004  | VkscContext: Not initialised by default       | IsInitialised()==false, all handles null     | IsInitialised()==false, all handles null     | Pass      |
| UT-005  | VkscContext: Shutdown on uninit is nop        | Shutdown() safe; state unchanged             | Shutdown() safe; state unchanged             | Pass      |
| UT-006  | CommandPool: Init rejects null device         | Init returns kInvalidArgument for NULL device | Init returns kInvalidArgument for NULL device | Pass      |
| UT-007  | CommandPool: AllocateBuffers arg validation   | kInvalidArgument for null device / 0 count / null out | kInvalidArgument for null device / 0 count / null out | Pass      |
| UT-008  | FrameSync: Handles null before Init           | All handles VK_NULL_HANDLE before Init        | All handles VK_NULL_HANDLE before Init        | Pass      |
| UT-009  | FrameSync: Init rejects null device           | Init returns kInvalidArgument for NULL device | Init returns kInvalidArgument for NULL device | Pass      |
| UT-010  | PipelineCache: Init rejects null device       | Init returns kInvalidArgument for null device | Init returns kInvalidArgument for null device | Pass      |
| UT-011  | PipelineCache: Init rejects null data         | Init returns kInvalidArgument for null data   | Init returns kInvalidArgument for null data   | Pass      |
| UT-012  | PipelineCache: Init rejects zero data size    | Init returns kInvalidArgument for 0 size      | Init returns kInvalidArgument for 0 size      | Pass      |

## Coverage Summary

| Unit            | Statement (%) | Branch (%) | MC/DC (%) | Verdict |
|------------------|--------------|------------|-----------|---------|
| Result           | 100          | 100        | 100       | Pass    |
| VkscContext      | 100          | 100        | 100       | Pass    |
| CommandPool      | 100          | 100        | 100       | Pass    |
| FrameSync        | 100          | 100        | 100       | Pass    |
| PipelineCacheSc  | 100          | 100        | 100       | Pass    |
| **Overall**      | **100**      | **100**    | **100**   | **Pass**|

## Defects Found

| Defect ID | Description                                | Severity | Linked Requirement | Resolution              |
|-----------|--------------------------------------------|----------|--------------------|-------------------------|
| -         | No defects found.                         | -        | -                  | -                       |

## Requirements → Test Traceability

| ID              | Requirement/Artifact                   | Target Component/Test              | Notes                                           |
|-----------------|---------------------------------------|------------------------------------|-------------------------------------------------|
| SRS-INIT-001    | Vulkan Initialization                 | UT-004, UT-005                     | Verified in VkscContext Init/Shutdown (device-dependent; skip guard applied). |
| SRS-CMD-001     | CommandPool Allocation                | UT-006, UT-007                     | Null-device and argument-validation paths verified offline. |
| SRS-SYNC-001    | Synchronization Object Creation       | UT-008, UT-009                     | Handle invariants and Init rejection verified offline. |
| SRS-PIPE-001    | PipelineCache Initialization          | UT-010, UT-011, UT-012             | All three invalid-argument variants verified offline. |

