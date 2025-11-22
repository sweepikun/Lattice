#pragma once

#include <jni.h>
#include <memory>
#include <string>
#include <functional>
#include "../core/io/anvil_format.hpp"
#include "../core/io/async_chunk_io.hpp"

namespace lattice {
namespace jni {

// ===== 统一的ChunkIO桥接类 =====
// Java -> JNI -> Anvil -> 区块IO (一体化接口)

class ChunkIOBridge {
public:
    explicit ChunkIOBridge(const std::string& worldPath);
    ~ChunkIOBridge();
    
    // ===== Java JNI接口方法 =====
    
    // 异步加载区块
    static void JNICALL loadChunkAsync(JNIEnv* env, jobject obj, 
                                      jint worldId, jint chunkX, jint chunkZ);
    
    // 异步保存区块
    static void JNICALL saveChunkAsync(JNIEnv* env, jobject obj,
                                      jint worldId, jint chunkX, jint chunkZ,
                                      jbyteArray data);
    
    // 批量保存区块
    static void JNICALL saveChunksBatch(JNIEnv* env, jobject obj,
                                       jobjectArray chunkArray);
    
    // 设置存储格式（LEGACY或ANVIL）
    static void JNICALL setStorageFormat(JNIEnv* env, jobject obj, jint format);
    
    // 获取存储格式
    static jint JNICALL getStorageFormat(JNIEnv* env, jobject obj);
    
    // 切换到Anvil格式（1.21.10兼容）
    static void JNICALL enableAnvilFormat(JNIEnv* env, jobject obj, jboolean enable);
    
    // 转换为Anvil格式
    static jboolean JNICALL convertToAnvilFormat(JNIEnv* env, jobject obj, jint worldId);
    
    // 检查是否为Anvil格式
    static jboolean JNICALL isAnvilFormat(JNIEnv* env, jobject obj, jint worldId);
    
    // 检测世界格式
    static jint JNICALL detectStorageFormat(JNIEnv* env, jobject obj, jint worldId);
    
    // ===== Java兼容方法 =====
    
    // 直接获取Java字节数组
    static jbyteArray JNICALL getChunkDataForJava(JNIEnv* env, jobject obj,
                                                  jint worldId, jint chunkX, jint chunkZ);
    
    // 设置来自Java的区块数据
    static void JNICALL setChunkDataFromJava(JNIEnv* env, jobject obj,
                                            jint worldId, jint chunkX, jint chunkZ,
                                            jbyteArray data);
    
    // 批量设置Java数据
    static void JNICALL setChunksDataFromJava(JNIEnv* env, jobject obj,
                                             jobjectArray dataArray);
    
    // ===== Minecraft 1.21.10特定兼容 =====
    
    // 检查NBT格式兼容性
    static jboolean JNICALL isNBTFormatCompatible(JNIEnv* env, jobject obj, jbyteArray data);
    
    // 转换NBT数据格式
    static jbyteArray JNICALL convertNBTFormat(JNIEnv* env, jobject obj, 
                                              jbyteArray data, jint fromVersion, jint toVersion);
    
    // 验证区块数据完整性
    static jboolean JNICALL validateChunkData(JNIEnv* env, jobject obj,
                                             jint worldId, jint chunkX, jint chunkZ);
    
    // ===== 性能监控 =====
    
    // 获取IO统计信息
    static jobject JNICALL getIOStatistics(JNIEnv* env, jobject obj);
    
    // 重置统计信息
    static void JNICALL resetIOStatistics(JNIEnv* env, jobject obj);
    
    // ===== 内存管理 =====
    
    // 初始化JNI桥接
    static bool initialize(JNIEnv* env);
    
    // 清理JNI桥接
    static void cleanup(JNIEnv* env);
    
    // 获取全局实例
    static ChunkIOBridge* getInstance();
    
private:
    std::string worldPath_;
    std::unique_ptr<lattice::io::AsyncChunkIO> asyncIO_;
    std::unique_ptr<lattice::io::anvil::AnvilChunkIO> anvilIO_;
    
    // Java方法ID缓存
    static jclass chunkIOClass_;
    static jmethodID callbackMethod_;
    static jclass resultClass_;
    static jmethodID resultConstructor_;
    
    // 全局实例
    static std::unique_ptr<ChunkIOBridge> globalInstance_;
    
    // 辅助方法
    static jbyteArray createJavaByteArray(JNIEnv* env, const std::vector<uint8_t>& data);
    static std::vector<uint8_t> getDataFromJavaByteArray(JNIEnv* env, jbyteArray javaArray);
    static void throwJavaException(JNIEnv* env, const char* message);
    static void invokeCallback(JNIEnv* env, jobject callback, bool success, const std::string& error);
};

// ===== JNI本地方法注册 =====

extern "C" {
    // 原生库加载
    JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
    JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);
    
    // 初始化原生桥接
    JNIEXPORT void JNICALL Java_lattice_io_ChunkIOBridge_nativeInit(JNIEnv* env, jobject obj, jstring worldPath);
    JNIEXPORT void JNICALL Java_lattice_io_ChunkIOBridge_nativeDestroy(JNIEnv* env, jobject obj);
}

} // namespace jni
} // namespace lattice