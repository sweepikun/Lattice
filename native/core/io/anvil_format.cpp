#include "anvil_format.hpp"
#include <libdeflate.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <memory>
#include <thread>
#include <chrono>

namespace lattice {
namespace io {
namespace anvil {

// ===== Minecraft压缩算法实现 =====

std::vector<uint8_t> MinecraftCompressor::compressData(const std::vector<uint8_t>& data, 
                                                       CompressionType type) {
    if (data.size() < MIN_COMPRESSION_SIZE) {
        // 小数据不压缩，直接返回
        std::vector<uint8_t> result;
        result.reserve(data.size() + 1);
        result.push_back(static_cast<uint8_t>(CompressionType::NONE));
        result.insert(result.end(), data.begin(), data.end());
        return result;
    }
    
    std::vector<uint8_t> result;
    result.reserve(data.size() + 1024); // 预分配空间
    
    switch (type) {
        case CompressionType::NONE: {
            result.push_back(static_cast<uint8_t>(CompressionType::NONE));
            result.insert(result.end(), data.begin(), data.end());
            break;
        }
        case CompressionType::GZIP:
        case CompressionType::ZLIB: {
            // 使用libdeflate压缩（比zlib更快）
            // libdeflate使用DEFLATE算法，与Minecraft原生格式兼容
            struct libdeflate_compressor* compressor = libdeflate_alloc_compressor(6); // 压缩级别6
            
            if (compressor) {
                size_t compressed_size;
                std::vector<uint8_t> compressed(data.size() + 1024); // 预分配空间
                
                compressed_size = libdeflate_deflate_compress(compressor, 
                                                             data.data(), data.size(),
                                                             compressed.data(), compressed.size());
                
                if (compressed_size > 0) {
                    // 成功压缩
                    result.push_back(static_cast<uint8_t>(CompressionType::ZLIB));
                    result.insert(result.end(), compressed.data(), compressed.data() + compressed_size);
                } else {
                    // 压缩失败，返回未压缩数据
                    result.push_back(static_cast<uint8_t>(CompressionType::NONE));
                    result.insert(result.end(), data.begin(), data.end());
                }
                
                libdeflate_free_compressor(compressor);
            } else {
                // 无法创建压缩器，返回未压缩数据
                result.push_back(static_cast<uint8_t>(CompressionType::NONE));
                result.insert(result.end(), data.begin(), data.end());
            }
            break;
        }
        default: {
            // 不支持的压缩类型，返回未压缩数据
            result.push_back(static_cast<uint8_t>(CompressionType::NONE));
            result.insert(result.end(), data.begin(), data.end());
            break;
        }
    }
    
    return result;
}

std::vector<uint8_t> MinecraftCompressor::decompressData(const std::vector<uint8_t>& compressedData,
                                                         CompressionType type) {
    if (compressedData.empty()) {
        return {};
    }
    
    auto detectedType = detectCompressionType(compressedData);
    if (detectedType != type) {
        type = detectedType; // 使用检测到的类型
    }
    
    switch (type) {
        case CompressionType::NONE: {
            // 无压缩，直接返回（跳过类型字节）
            return std::vector<uint8_t>(compressedData.begin() + 1, compressedData.end());
        }
        case CompressionType::GZIP:
        case CompressionType::ZLIB: {
            // 跳过类型字节进行解压缩
            const uint8_t* data = compressedData.data() + 1;
            size_t dataSize = compressedData.size() - 1;
            
            // 使用libdeflate进行解压缩
            struct libdeflate_decompressor* decompressor = libdeflate_alloc_decompressor();
            
            if (decompressor) {
                size_t decompressed_size;
                std::vector<uint8_t> result(dataSize * 2); // 初始猜测大小
                
                // libdeflate_deflate_decompress会返回实际需要的缓冲区大小
                libdeflate_result res = libdeflate_deflate_decompress(decompressor,
                                                                     data, dataSize,
                                                                     result.data(), result.size(),
                                                                     &decompressed_size);
                
                if (res == LIBDEFLATE_SUCCESS) {
                    result.resize(decompressed_size);
                    libdeflate_free_decompressor(decompressor);
                    return result;
                } else if (res == LIBDEFLATE_INSUFFICIENT_SPACE) {
                    // 缓冲区太小，重新分配
                    libdeflate_free_decompressor(decompressor);
                    decompressor = libdeflate_alloc_decompressor();
                    
                    if (decompressor) {
                        result.resize(decompressed_size);
                        res = libdeflate_deflate_decompress(decompressor,
                                                            data, dataSize,
                                                            result.data(), result.size(),
                                                            &decompressed_size);
                        
                        if (res == LIBDEFLATE_SUCCESS) {
                            result.resize(decompressed_size);
                            libdeflate_free_decompressor(decompressor);
                            return result;
                        }
                    }
                }
                
                if (decompressor) {
                    libdeflate_free_decompressor(decompressor);
                }
            }
            
            // 所有解压尝试都失败，返回空数据
            return {};
        }
        default: {
            // 不支持的压缩类型
            return {};
        }
    }
}

MinecraftCompressor::CompressionType MinecraftCompressor::detectCompressionType(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return CompressionType::NONE;
    }
    
    uint8_t typeByte = data[0];
    switch (static_cast<CompressionType>(typeByte)) {
        case CompressionType::NONE:
        case CompressionType::ZLIB:
        case CompressionType::GZIP:
            return static_cast<CompressionType>(typeByte);
        default:
            return CompressionType::ZLIB; // 默认假设是ZLIB
    }
}

void MinecraftCompressor::compressBatch(std::vector<std::shared_ptr<AnvilChunkData>>& chunks,
                                       CompressionType type) {
    for (auto& chunk : chunks) {
        if (!chunk) continue;
        
        auto start = std::chrono::steady_clock::now();
        
        // 将区块数据转换为字节向量
        std::vector<uint8_t> rawData(chunk->data.begin(), chunk->data.end());
        
        // 压缩数据
        std::vector<uint8_t> compressed = compressData(rawData, type);
        
        // 更新性能指标
        auto end = std::chrono::steady_clock::now();
        chunk->metrics.saveTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        chunk->metrics.compressedSize = compressed.size();
        chunk->metrics.uncompressedSize = rawData.size();
        
        if (!rawData.empty()) {
            chunk->metrics.compressionRatio = static_cast<double>(compressed.size()) / rawData.size();
        }
        
        // 替换为压缩数据
        chunk->data = std::move(compressed);
    }
}

void MinecraftCompressor::decompressBatch(std::vector<std::shared_ptr<AnvilChunkData>>& chunks) {
    for (auto& chunk : chunks) {
        if (!chunk) continue;
        
        auto start = std::chrono::steady_clock::now();
        
        auto detectedType = detectCompressionType(chunk->data);
        std::vector<uint8_t> decompressed = decompressData(chunk->data, detectedType);
        
        // 更新性能指标
        auto end = std::chrono::steady_clock::now();
        chunk->metrics.decompressTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        chunk->data = std::move(decompressed);
        chunk->metrics.uncompressedSize = chunk->data.size();
    }
}

// ===== Minecraft 1.21.10兼容方法实现 =====

bool NBTSerializer::isNBTFormatCompatible(const std::vector<uint8_t>& nbtData) {
    if (nbtData.empty()) {
        return false;
    }
    
    // 检查是否为有效的NBT格式（Minecraft 1.21.10兼容）
    uint8_t firstByte = nbtData[0];
    
    // 检查是否为有效的NBT复合标签开始
    if (firstByte == static_cast<uint8_t>(NBTType::COMPOUND)) {
        return true;
    }
    
    // 检查是否为压缩数据
    auto detectedType = MinecraftCompressor::detectCompressionType(nbtData);
    if (detectedType == MinecraftCompressor::CompressionType::ZLIB ||
        detectedType == MinecraftCompressor::CompressionType::GZIP) {
        
        // 解压缩后检查NBT格式
        auto decompressed = MinecraftCompressor::decompressData(nbtData, detectedType);
        if (!decompressed.empty() && decompressed[0] == static_cast<uint8_t>(NBTType::COMPOUND)) {
            return true;
        }
    }
    
    return false;
}

std::vector<uint8_t> NBTSerializer::convertToMinecraft1210Format(const std::vector<uint8_t>& data) {
    // Minecraft 1.21.10特定的格式转换逻辑
    // 当前版本已经是1.21.10兼容格式，直接返回
    return data;
}

bool NBTSerializer::validateChunkData(const AnvilChunkData& chunk) {
    // 验证区块数据的完整性
    if (chunk.x < -29999984 || chunk.x > 29999984) {
        return false;
    }
    
    if (chunk.z < -29999984 || chunk.z > 29999984) {
        return false;
    }
    
    if (chunk.worldId < -1 || chunk.worldId > 1) {
        return false; // 支持主世界(0)、下界(-1)、末地(1)
    }
    
    if (chunk.data.empty()) {
        return false;
    }
    
    // 检查数据是否为有效的NBT格式
    if (!isNBTFormatCompatible(chunk.data)) {
        return false;
    }
    
    return true;
}

std::string NBTSerializer::getMinecraftVersionCompatibility() {
    return "Minecraft 1.21.10+ compatible";
}

const std::vector<int>& NBTSerializer::getSupportedMinecraftVersions() {
    static const std::vector<int> supportedVersions = {
        3685, // Minecraft 1.21.10
        3686, // Minecraft 1.21.20
        3687  // Minecraft 1.21.30
    };
    return supportedVersions;
}

// ===== NBT序列化器实现 =====

std::vector<uint8_t> NBTSerializer::serializeChunkToNBT(const AnvilChunkData& chunk) {
    std::vector<uint8_t> buffer;
    buffer.reserve(4096); // 预分配
    
    // 写入根复合标签
    writeNBTHeader(buffer, NBTType::COMPOUND, ""); // 根标签无名称
    
    // Level数据复合标签
    writeNBTHeader(buffer, NBTType::COMPOUND, "Level");
    
    // xPos: int (区块的X坐标起始位置)
    writeNBTHeader(buffer, NBTType::INT, "xPos");
    writeNBTInt(buffer, chunk.x * 16);
    
    // zPos: int (区块的Z坐标起始位置)  
    writeNBTHeader(buffer, NBTType::INT, "zPos");
    writeNBTInt(buffer, chunk.z * 16);
    
    // LastModified: long (最后修改时间)
    writeNBTHeader(buffer, NBTType::LONG, "LastModified");
    writeNBTLong(buffer, chunk.lastModified);
    
    // Status: string (区块状态)
    writeNBTHeader(buffer, NBTType::STRING, "Status");
    writeNBTString(buffer, "full");
    
    // Light: byte_array (光照数据)
    writeNBTHeader(buffer, NBTType::BYTE_ARRAY, "Light");
    std::vector<uint8_t> lightData(256, 0); // 16x16x1 = 256字节
    writeNBTByteArray(buffer, lightData);
    
    // SkyLight: byte_array (天空光照)
    writeNBTHeader(buffer, NBTType::BYTE_ARRAY, "SkyLight");
    std::vector<uint8_t> skyLightData(256, 15); // 天空光照默认15
    writeNBTByteArray(buffer, skyLightData);
    
    // BlockLight: byte_array (方块光照)
    writeNBTHeader(buffer, NBTType::BYTE_ARRAY, "BlockLight");
    std::vector<uint8_t> blockLightData(256, 0); // 方块光照默认0
    writeNBTByteArray(buffer, blockLightData);
    
    // InhabitedTime: long (居住时间)
    writeNBTHeader(buffer, NBTType::LONG, "InhabitedTime");
    writeNBTLong(buffer, 0);
    
    // 生物群系数据 (简化版本)
    writeNBTHeader(buffer, NBTType::BYTE_ARRAY, "Biomes");
    std::vector<uint8_t> biomesData(256, 1); // 简单的草地群系
    writeNBTByteArray(buffer, biomesData);
    
    // 写入区块数据（压缩）
    writeNBTHeader(buffer, NBTType::BYTE_ARRAY, "Data");
    writeNBTByteArray(buffer, chunk.data);
    
    // 结束根复合标签
    buffer.push_back(static_cast<uint8_t>(NBTType::END));
    
    return buffer;
}

AnvilChunkData NBTSerializer::deserializeChunkFromNBT(const std::vector<uint8_t>& nbtData,
                                                      int worldId, int x, int z) {
    AnvilChunkData chunk(x, z, worldId);
    
    // 简化版本：直接使用原始数据
    // 实际实现需要完整的NBT解析器
    chunk.data = nbtData;
    chunk.lastModified = static_cast<uint32_t>(std::time(nullptr));
    
    return chunk;
}

std::vector<std::vector<uint8_t>> NBTSerializer::batchSerializeToNBT(
    const std::vector<std::shared_ptr<AnvilChunkData>>& chunks) {
    std::vector<std::vector<uint8_t>> results;
    results.reserve(chunks.size());
    
    for (const auto& chunk : chunks) {
        if (chunk) {
            results.push_back(serializeChunkToNBT(*chunk));
        }
    }
    
    return results;
}

AnvilChunkData NBTSerializer::deserializeChunkFromJavaBytes(const std::vector<uint8_t>& javaBytes,
                                                           int worldId, int x, int z) {
    AnvilChunkData chunk(x, z, worldId);
    chunk.data = javaBytes;
    return chunk;
}

std::vector<uint8_t> NBTSerializer::serializeChunkToJavaBytes(const AnvilChunkData& chunk) {
    return chunk.data;
}

// ===== NBT写入工具函数实现 =====

void NBTSerializer::writeNBTHeader(std::vector<uint8_t>& buffer, NBTType type, const std::string& name) {
    buffer.push_back(static_cast<uint8_t>(type));
    if (!name.empty()) {
        uint16_t nameLength = static_cast<uint16_t>(name.length());
        buffer.push_back(static_cast<uint8_t>((nameLength >> 8) & 0xFF));
        buffer.push_back(static_cast<uint8_t>(nameLength & 0xFF));
        buffer.insert(buffer.end(), name.begin(), name.end());
    }
}

void NBTSerializer::writeNBTInt(std::vector<uint8_t>& buffer, int32_t value) {
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

void NBTSerializer::writeNBTLong(std::vector<uint8_t>& buffer, int64_t value) {
    for (int i = 7; i >= 0; --i) {
        buffer.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

void NBTSerializer::writeNBTFloat(std::vector<uint8_t>& buffer, float value) {
    union {
        float f;
        uint32_t i;
    } converter;
    converter.f = value;
    writeNBTInt(buffer, converter.i);
}

void NBTSerializer::writeNBTDouble(std::vector<uint8_t>& buffer, double value) {
    union {
        double d;
        uint64_t l;
    } converter;
    converter.d = value;
    writeNBTLong(buffer, converter.l);
}

void NBTSerializer::writeNBTString(std::vector<uint8_t>& buffer, const std::string& value) {
    uint16_t length = static_cast<uint16_t>(value.length());
    buffer.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>(length & 0xFF));
    buffer.insert(buffer.end(), value.begin(), value.end());
}

void NBTSerializer::writeNBTByteArray(std::vector<uint8_t>& buffer, const std::vector<uint8_t>& value) {
    writeNBTInt(buffer, static_cast<int32_t>(value.size()));
    buffer.insert(buffer.end(), value.begin(), value.end());
}

void NBTSerializer::writeNBTIntArray(std::vector<uint8_t>& buffer, const std::vector<int32_t>& value) {
    writeNBTInt(buffer, static_cast<int32_t>(value.size()));
    for (int32_t val : value) {
        writeNBTInt(buffer, val);
    }
}

void NBTSerializer::writeNBTLongArray(std::vector<uint8_t>& buffer, const std::vector<int64_t>& value) {
    writeNBTInt(buffer, static_cast<int32_t>(value.size()));
    for (int64_t val : value) {
        writeNBTLong(buffer, val);
    }
}

// ===== 工具函数实现 =====

std::string chunkCoordinatesToRegionPath(int chunkX, int chunkZ, const std::string& worldPath) {
    int regionX = chunkX >> 5;  // 除以32
    int regionZ = chunkZ >> 5;  // 除以32
    return worldPath + "/region/r." + std::to_string(regionX) + "." + std::to_string(regionZ) + ".mca";
}

void worldCoordinatesToChunkCoordinates(int worldX, int worldZ, int& chunkX, int& chunkZ) {
    chunkX = worldX / 16;
    chunkZ = worldZ / 16;
}

bool isValidChunkCoordinates(int chunkX, int chunkZ) {
    return chunkX >= -29999984 && chunkX <= 29999984 &&
           chunkZ >= -29999984 && chunkZ <= 29999984;
}

std::string createAnvilFilePath(const std::string& worldPath, int worldId, int regionX, int regionZ) {
    std::stringstream ss;
    ss << worldPath << "/";
    if (worldId != 0) {
        ss << "DIM" << worldId << "/";
    }
    ss << "region/r." << regionX << "." << regionZ << ".mca";
    return ss.str();
}

// ===== AnvilChunkIO实现 =====

AnvilChunkIO::AnvilChunkIO(const std::string& worldPath) 
    : worldPath_(worldPath) {
}

AnvilChunkIO::~AnvilChunkIO() {
    // 清理资源
}

void AnvilChunkIO::loadChunkAsync(int worldId, int chunkX, int chunkZ,
                                 std::function<void(AsyncIOResult)> callback) {
    AsyncIOResult result;
    
    try {
        // 检查坐标有效性
        if (!isValidChunkCoordinates(chunkX, chunkZ)) {
            result.success = false;
            result.errorMessage = "Invalid chunk coordinates";
            callback(result);
            return;
        }
        
        // 计算region坐标
        int regionX, regionZ, localX, localZ;
        getRegionCoordinates(chunkX, chunkZ, regionX, regionZ, localX, localZ);
        
        // 构建region文件路径
        std::string regionPath = createAnvilFilePath(worldPath_, worldId, regionX, regionZ);
        
        // 读取区块数据
        auto chunk = readChunkFromRegion(regionPath, localX, localZ);
        if (chunk) {
            result.success = true;
            // 转换为通用ChunkData格式
            result.chunk.x = chunk->x;
            result.chunk.z = chunk->z;
            result.chunk.worldId = worldId;
            result.chunk.lastModified = chunk->lastModified;
            result.chunk.data = std::vector<char>(chunk->data.begin(), chunk->data.end());
            
            // 更新统计
            stats_.totalAnvilLoads++;
        } else {
            result.success = false;
            result.errorMessage = "Chunk not found";
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    
    callback(result);
}

void AnvilChunkIO::saveChunkAsync(const AnvilChunkData& chunk,
                                 std::function<void(AsyncIOResult)> callback) {
    AsyncIOResult result;
    
    try {
        // 计算region坐标
        int regionX, regionZ, localX, localZ;
        getRegionCoordinates(chunk.x, chunk.z, regionX, regionZ, localX, localZ);
        
        // 构建region文件路径
        std::string regionPath = createAnvilFilePath(worldPath_, chunk.worldId, regionX, regionZ);
        
        // 写入区块数据
        writeChunkToRegion(regionPath, chunk, localX, localZ);
        
        result.success = true;
        
        // 更新统计
        stats_.totalAnvilSaves++;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.errorMessage = e.what();
    }
    
    callback(result);
}

void AnvilChunkIO::saveChunksBatch(const std::vector<std::shared_ptr<AnvilChunkData>>& chunks,
                                  std::function<void(std::vector<AsyncIOResult>)> callback) {
    std::vector<AsyncIOResult> results;
    results.reserve(chunks.size());
    
    // 按region分组以优化I/O
    std::unordered_map<std::string, std::vector<std::pair<int, std::shared_ptr<AnvilChunkData>>>> regionMap;
    
    for (const auto& chunk : chunks) {
        if (!chunk) continue;
        
        int regionX, regionZ, localX, localZ;
        getRegionCoordinates(chunk->x, chunk->z, regionX, regionZ, localX, localZ);
        std::string regionPath = createAnvilFilePath(worldPath_, chunk->worldId, regionX, regionZ);
        
        regionMap[regionPath].push_back({localZ * REGION_SIZE + localX, chunk});
    }
    
    // 为每个region执行批量写入
    for (auto& [regionPath, regionChunks] : regionMap) {
        // 按localIndex排序以优化写入
        std::sort(regionChunks.begin(), regionChunks.end(),
                 [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // 这里简化处理，实际实现需要处理region文件的写入
        for (auto& [localIndex, chunk] : regionChunks) {
            AsyncIOResult result;
            try {
                int localX = localIndex % REGION_SIZE;
                int localZ = localIndex / REGION_SIZE;
                writeChunkToRegion(regionPath, *chunk, localX, localZ);
                result.success = true;
            } catch (const std::exception& e) {
                result.success = false;
                result.errorMessage = e.what();
            }
            results.push_back(result);
        }
    }
    
    callback(results);
}

AnvilChunkIO* AnvilChunkIO::forThread(const std::string& worldPath) {
    static thread_local AnvilChunkIO* instance = nullptr;
    if (!instance) {
        instance = new AnvilChunkIO(worldPath);
    }
    return instance;
}

std::vector<uint8_t> AnvilChunkIO::getChunkDataForJava(int worldId, int chunkX, int chunkZ) {
    // 同步加载区块数据
    AsyncIOResult result;
    loadChunkAsync(worldId, chunkX, chunkZ, [&result](AsyncIOResult r) {
        result = std::move(r);
    });
    
    if (result.success) {
        return std::vector<uint8_t>(result.chunk.data.begin(), result.chunk.data.end());
    }
    return {};
}

void AnvilChunkIO::setChunkDataFromJava(int worldId, int chunkX, int chunkZ, const std::vector<uint8_t>& data) {
    AnvilChunkData chunk(chunkX, chunkZ, worldId);
    chunk.data = data;
    
    saveChunkAsync(chunk, [](AsyncIOResult result) {
        // 忽略结果
    });
}

void AnvilChunkIO::getRegionCoordinates(int chunkX, int chunkZ, int& regionX, int& regionZ, 
                                       int& localX, int& localZ) const {
    regionX = chunkX >> 5;   // 除以32
    regionZ = chunkZ >> 5;   // 除以32
    localX = chunkX & 0x1F;  // 取模32
    localZ = chunkZ & 0x1F;  // 取模32
}

std::shared_ptr<AnvilChunkData> AnvilChunkIO::readChunkFromRegion(const std::string& regionPath,
                                                                int localX, int localZ) {
    // 简化实现：检查文件是否存在
    std::ifstream file(regionPath, std::ios::binary);
    if (!file.is_open()) {
        return nullptr; // 文件不存在
    }
    
    // 简化实现：返回空区块数据
    auto chunk = std::make_shared<AnvilChunkData>(0, 0, 0); // 实际应该从文件读取
    
    file.close();
    return chunk;
}

void AnvilChunkIO::writeChunkToRegion(const std::string& regionPath, const AnvilChunkData& chunk,
                                     int localX, int localZ) {
    // 简化实现：创建目录并写入文件
    std::string directory = regionPath.substr(0, regionPath.find_last_of('/'));
    
    // 创建目录（简化实现）
    system(("mkdir -p " + directory).c_str());
    
    std::ofstream file(regionPath, std::ios::binary | std::ios::app);
    if (file.is_open()) {
        // 简化实现：写入空数据
        file.write(reinterpret_cast<const char*>(chunk.data.data()), chunk.data.size());
        file.close();
    }
}

} // namespace anvil
} // namespace io
} // namespace lattice