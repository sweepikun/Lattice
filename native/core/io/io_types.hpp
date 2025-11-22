#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <atomic>

namespace lattice {
namespace io {

// ===== 数据结构定义 =====

// 区块数据结构（与 Minecraft 内部格式兼容）
struct ChunkData {
    int x, z;
    int worldId;
    uint64_t lastModified;
    std::vector<char> data; // 原始 NBT 数据
    size_t dataSize() const { return data.size(); }
};

// I/O 操作类型
enum class IOOperation {
    LOAD,
    SAVE,
    DELETE,
    REGION_SCAN
};

// 存储格式类型
enum class StorageFormat {
    LEGACY = 0,    // 传统格式（一区块一文件）
    ANVIL = 1      // Anvil格式（32x32区块一文件）
};

// 异步操作结果
struct AsyncIOResult {
    bool success;
    std::string errorMessage;
    ChunkData chunk;
    uint64_t completionTime;
    
    AsyncIOResult() : success(false), completionTime(0) {}
};

// 平台特性检测
struct PlatformFeatures {
    bool io_uring = false;      // Linux io_uring
    bool direct_io = false;     // Direct I/O
    bool apfs = false;          // macOS APFS
    bool apple_silicon = false; // Apple Silicon
    bool iocp = false;          // Windows IOCP
};

} // namespace io
} // namespace lattice