#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <chrono>
#include <functional>
#include <unordered_map>
#include "io_types.hpp"

namespace lattice {
namespace io {
namespace anvil {

// ===== Anvil文件格式常量 =====
constexpr size_t REGION_SIZE = 32;           // 32x32 区块每个region
constexpr size_t REGION_FILE_SIZE = 32 * 1024 * 1024; // 32MB 每个region文件
constexpr size_t CHUNK_ENTRY_SIZE = 4096;   // 每个区块条目4KB
constexpr size_t TIMESTAMP_BYTES = 4;       // 时间戳4字节
constexpr size_t CHUNK_COORD_BYTES = 4;     // 区块坐标4字节

// ===== Minecraft NBT标签类型 =====
enum class NBTType : uint8_t {
    END = 0,
    BYTE = 1,
    SHORT = 2,
    INT = 3,
    LONG = 4,
    FLOAT = 5,
    DOUBLE = 6,
    BYTE_ARRAY = 7,
    STRING = 8,
    LIST = 9,
    COMPOUND = 10,
    INT_ARRAY = 11,
    LONG_ARRAY = 12
};

// ===== Anvil区块数据结构 =====
struct AnvilChunkData {
    int x, z;                    // 区块坐标
    int worldId;                 // 世界ID
    uint32_t lastModified;       // 最后修改时间戳
    std::vector<uint8_t> data;   // 压缩后的NBT数据
    
    // 性能指标
    struct PerformanceMetrics {
        std::chrono::microseconds loadTime{0};
        std::chrono::microseconds saveTime{0};
        std::chrono::microseconds decompressTime{0};
        size_t compressedSize{0};
        size_t uncompressedSize{0};
        double compressionRatio{0.0};
    } metrics;
    
    AnvilChunkData() : x(0), z(0), worldId(0), lastModified(0) {}
    AnvilChunkData(int x_, int z_, int worldId_) 
        : x(x_), z(z_), worldId(worldId_), 
          lastModified(static_cast<uint32_t>(std::time(nullptr))) {}
};

// ===== Minecraft压缩算法 =====
class MinecraftCompressor {
public:
    // 注意：Minecraft原版仅支持ZLIB/DEFLATE格式，其他格式作为可选选项并默认关闭
    // 使用libdeflate库（比标准zlib更快）实现DEFLATE压缩算法
    enum class CompressionType : uint8_t {
        NONE = 0,      // 无压缩（小数据）
        ZLIB = 1,      // DEFLATE压缩（唯一支持的Minecraft原生格式，默认，使用libdeflate库）
        // 以下格式作为可选选项，默认关闭
        GZIP = 2,      // GZIP压缩 (可选，旧格式)
        CUSTOM = 3     // 自定义压缩 (可选)
    };
    
    static std::vector<uint8_t> compressData(const std::vector<uint8_t>& data, 
                                           CompressionType type = CompressionType::ZLIB);
    static std::vector<uint8_t> decompressData(const std::vector<uint8_t>& compressedData,
                                              CompressionType type);
    static CompressionType detectCompressionType(const std::vector<uint8_t>& data);
    
    // 批量压缩优化
    static void compressBatch(std::vector<std::shared_ptr<AnvilChunkData>>& chunks,
                            CompressionType type = CompressionType::ZLIB);
    static void decompressBatch(std::vector<std::shared_ptr<AnvilChunkData>>& chunks);
    
private:
    static constexpr size_t MIN_COMPRESSION_SIZE = 64; // 最小压缩大小
};

// ===== NBT序列化器 =====
class NBTSerializer {
public:
    // 将Minecraft区块数据序列化为标准NBT格式（Minecraft 1.21.10兼容）
    static std::vector<uint8_t> serializeChunkToNBT(const AnvilChunkData& chunk);
    
    // 从NBT数据反序列化区块（支持Minecraft 1.21.10格式）
    static AnvilChunkData deserializeChunkFromNBT(const std::vector<uint8_t>& nbtData,
                                                 int worldId, int x, int z);
    
    // 批量序列化优化（支持Minecraft 1.21.10）
    static std::vector<std::vector<uint8_t>> batchSerializeToNBT(
        const std::vector<std::shared_ptr<AnvilChunkData>>& chunks);
    
    // 从Java字节数组反序列化（Minecraft 1.21.10兼容）
    static AnvilChunkData deserializeChunkFromJavaBytes(const std::vector<uint8_t>& javaBytes,
                                                       int worldId, int x, int z);
    
    // 转换为Java兼容的字节数组（Minecraft 1.21.10兼容）
    static std::vector<uint8_t> serializeChunkToJavaBytes(const AnvilChunkData& chunk);
    
    // ===== Minecraft 1.21.10特定方法 =====
    
    // 验证NBT格式兼容性
    static bool isNBTFormatCompatible(const std::vector<uint8_t>& nbtData);
    
    // 转换为Minecraft 1.21.10格式
    static std::vector<uint8_t> convertToMinecraft1210Format(const std::vector<uint8_t>& data);
    
    // 验证区块数据完整性
    static bool validateChunkData(const AnvilChunkData& chunk);
    
    // 获取Minecraft版本兼容性信息
    static std::string getMinecraftVersionCompatibility();
    
    // 支持的Minecraft版本列表
    static const std::vector<int>& getSupportedMinecraftVersions();
    
private:
    // NBT写入工具函数
    static void writeNBTHeader(std::vector<uint8_t>& buffer, NBTType type, const std::string& name);
    static void writeNBTInt(std::vector<uint8_t>& buffer, int32_t value);
    static void writeNBTLong(std::vector<uint8_t>& buffer, int64_t value);
    static void writeNBTFloat(std::vector<uint8_t>& buffer, float value);
    static void writeNBTDouble(std::vector<uint8_t>& buffer, double value);
    static void writeNBTString(std::vector<uint8_t>& buffer, const std::string& value);
    static void writeNBTByteArray(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& value);
    static void writeNBTIntArray(std::vector<uint8_t>& buffer, const std::vector<int32_t>& value);
    static void writeNBTLongArray(std::vector<uint8_t>& buffer, const std::vector<int64_t>& value);
    
    // NBT读取工具函数
    static bool readNBTHeader(const std::vector<uint8_t>& buffer, size_t& offset, NBTType& type, std::string& name);
    static int32_t readNBTInt(const std::vector<uint8_t>& buffer, size_t& offset);
    static int64_t readNBTLong(const std::vector<uint8_t>& buffer, size_t& offset);
    static float readNBTFloat(const std::vector<uint8_t>& buffer, size_t& offset);
    static double readNBTDouble(const std::vector<uint8_t>& buffer, size_t& offset);
    static std::string readNBTString(const std::vector<uint8_t>& buffer, size_t& offset);
    static std::vector<uint8_t> readNBTByteArray(const std::vector<uint8_t>& buffer, size_t& offset);
    static std::vector<int32_t> readNBTIntArray(const std::vector<uint8_t>& buffer, size_t& offset);
    static std::vector<int64_t> readNBTLongArray(const std::vector<uint8_t>& buffer, size_t& offset);
};

// ===== Anvil I/O 类 =====
class AnvilChunkIO {
public:
    explicit AnvilChunkIO(const std::string& worldPath);
    ~AnvilChunkIO();
    
    // 异步加载区块 - 与原版Minecraft兼容
    void loadChunkAsync(int worldId, int chunkX, int chunkZ,
                       std::function<void(AsyncIOResult)> callback);
    
    // 异步保存区块 - 使用Minecraft原生压缩
    void saveChunkAsync(const AnvilChunkData& chunk,
                       std::function<void(AsyncIOResult)> callback);
    
    // 批量操作优化（保持高性能）
    void saveChunksBatch(const std::vector<std::shared_ptr<AnvilChunkData>>& chunks,
                        std::function<void(std::vector<AsyncIOResult>)> callback);
    
    // 每线程实例（与AsyncChunkIO保持一致）
    static AnvilChunkIO* forThread(const std::string& worldPath);
    
    // 与PaperMC API的桥接
    std::vector<uint8_t> getChunkDataForJava(int worldId, int chunkX, int chunkZ);
    void setChunkDataFromJava(int worldId, int chunkX, int chunkZ, const std::vector<uint8_t>& data);
    
    // 性能监控
    struct AnvilPerformanceStats {
        size_t totalAnvilLoads{0};
        size_t totalAnvilSaves{0};
        uint64_t totalLoadTime{0};
        uint64_t totalSaveTime{0};
        uint64_t maxLoadTime{0};
        uint64_t maxSaveTime{0};
    };
    
    const AnvilPerformanceStats& getPerformanceStats() const { return stats_; }
    
    // 世界路径管理
    void setWorldPath(const std::string& worldPath) { worldPath_ = worldPath; }
    const std::string& getWorldPath() const { return worldPath_; }
    
private:
    std::string worldPath_;
    mutable std::mutex ioMutex_;
    mutable AnvilPerformanceStats stats_;
    
    // 简单缓存（可选实现）
    std::unordered_map<std::string, std::shared_ptr<AnvilChunkData>> cache_;
    mutable std::mutex cacheMutex_;
    
    // 区块在region中的位置计算
    void getRegionCoordinates(int chunkX, int chunkZ, int& regionX, int& regionZ, 
                            int& localX, int& localZ) const;
    
    // 区块数据读写
    std::shared_ptr<AnvilChunkData> readChunkFromRegion(const std::string& regionPath,
                                                       int localX, int localZ);
    void writeChunkToRegion(const std::string& regionPath, const AnvilChunkData& chunk,
                          int localX, int localZ);
};

// ===== 工具函数 =====

/**
 * 将区块坐标转换为region文件路径
 */
std::string chunkCoordinatesToRegionPath(int chunkX, int chunkZ, const std::string& worldPath);

/**
 * 将世界坐标转换为区块坐标
 */
void worldCoordinatesToChunkCoordinates(int worldX, int worldZ, int& chunkX, int& chunkZ);

/**
 * 检查区块坐标是否有效
 */
bool isValidChunkCoordinates(int chunkX, int chunkZ);

/**
 * 创建Anvil兼容的文件路径
 */
std::string createAnvilFilePath(const std::string& worldPath, int worldId, int regionX, int regionZ);

} // namespace anvil
} // namespace io
} // namespace lattice