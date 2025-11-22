#pragma once

#include <jni.h>
#include <memory>
#include <atomic>
#include "../../core/net/async_compressor.hpp"

namespace lattice {
namespace jni {
namespace net {

// 正确的网络压缩器JNI桥接接口 - 仅负责桥接
class AsyncCompressorJNIOptimized {
public:
    // 数据压缩 - 桥接到core
    static jbyteArray compressData(JNIEnv* env, jobject compressor, jbyteArray inputData);
    
    // 数据解压 - 桥接到core
    static jbyteArray decompressData(JNIEnv* env, jobject compressor, jbyteArray compressedData);
    
    // 批量压缩 - 桥接到core
    static jobjectArray compressBatch(JNIEnv* env, jobject compressor, jobjectArray inputDataArray);
    
    // 批量解压 - 桥接到core
    static jobjectArray decompressBatch(JNIEnv* env, jobject compressor, jobjectArray compressedDataArray);
    
    // 压缩器初始化 - 桥接到core
    static jlong initializeCompressor(JNIEnv* env, jint compressionLevel, jint windowBits);
    
    // 关闭压缩器 - 桥接到core
    static void shutdownCompressor(JNIEnv* env, jlong compressorPtr);
    
    // 获取压缩统计 - 桥接到core
    static jlong getTotalCompressedBytes(JNIEnv* env, jlong compressorPtr);
    static jlong getTotalUncompressedBytes(JNIEnv* env, jlong compressorPtr);
    static jfloat getCompressionRatio(JNIEnv* env, jlong compressorPtr);
    static jlong getAverageCompressionTimeMicros(JNIEnv* env, jlong compressorPtr);
    
    // 压缩器配置 - 桥接到core
    static void setCompressionLevel(JNIEnv* env, jlong compressorPtr, jint level);
    static void setWindowBits(JNIEnv* env, jlong compressorPtr, jint windowBits);
    
    // 压缩器重置 - 桥接到core
    static void resetCompressor(JNIEnv* env, jlong compressorPtr);
    
private:
    // 性能统计（仅统计JNI桥接调用）
    static std::atomic<uint64_t> totalOperations_;
    static std::atomic<uint64_t> totalTimeMicros_;
    static std::atomic<uint64_t> totalCompressedBytes_;
    static std::atomic<uint64_t> totalUncompressedBytes_;
    
    // 内部辅助方法
    static void logPerformanceStats(const char* operation, uint64_t durationMicros);
    static void checkJNIException(JNIEnv* env, const char* methodName);
};

// JNI方法映射
static JNINativeMethod asyncCompressorMethods[] = {
    {"compressData", "(Ljava/lang/Object;[B)[B", 
     (void*)AsyncCompressorJNIOptimized::compressData},
    {"decompressData", "(Ljava/lang/Object;[B)[B", 
     (void*)AsyncCompressorJNIOptimized::decompressData},
    {"compressBatch", "(Ljava/lang/Object;[Ljava/lang/Object;)[Ljava/lang/Object;", 
     (void*)AsyncCompressorJNIOptimized::compressBatch},
    {"decompressBatch", "(Ljava/lang/Object;[Ljava/lang/Object;)[Ljava/lang/Object;", 
     (void*)AsyncCompressorJNIOptimized::decompressBatch},
    {"initializeCompressor", "(II)J", 
     (void*)AsyncCompressorJNIOptimized::initializeCompressor},
    {"shutdownCompressor", "(J)V", 
     (void*)AsyncCompressorJNIOptimized::shutdownCompressor},
    {"getTotalCompressedBytes", "(J)J", 
     (void*)AsyncCompressorJNIOptimized::getTotalCompressedBytes},
    {"getTotalUncompressedBytes", "(J)J", 
     (void*)AsyncCompressorJNIOptimized::getTotalUncompressedBytes},
    {"getCompressionRatio", "(J)F", 
     (void*)AsyncCompressorJNIOptimized::getCompressionRatio},
    {"getAverageCompressionTimeMicros", "(J)J", 
     (void*)AsyncCompressorJNIOptimized::getAverageCompressionTimeMicros},
    {"setCompressionLevel", "(JI)V", 
     (void*)AsyncCompressorJNIOptimized::setCompressionLevel},
    {"setWindowBits", "(JI)V", 
     (void*)AsyncCompressorJNIOptimized::setWindowBits},
    {"resetCompressor", "(J)V", 
     (void*)AsyncCompressorJNIOptimized::resetCompressor}
};

// 注册JNI方法
static jint registerAsyncCompressorJNI(JNIEnv* env) {
    jclass compressorClass = env->FindClass("com/lattice/native/AsyncCompressor");
    if (compressorClass == nullptr) return JNI_ERR;
    
    return env->RegisterNatives(compressorClass, 
                               asyncCompressorMethods,
                               sizeof(asyncCompressorMethods) / sizeof(asyncCompressorMethods[0]));
}

} // namespace net
} // namespace jni
} // namespace lattice