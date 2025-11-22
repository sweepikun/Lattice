#include "async_compressor_optimized_jni.hpp"
#include <chrono>

namespace lattice {
namespace jni {
namespace net {

// 初始化性能统计
std::atomic<uint64_t> AsyncCompressorJNIOptimized::totalOperations_{0};
std::atomic<uint64_t> AsyncCompressorJNIOptimized::totalTimeMicros_{0};
std::atomic<uint64_t> AsyncCompressorJNIOptimized::totalCompressedBytes_{0};
std::atomic<uint64_t> AsyncCompressorJNIOptimized::totalUncompressedBytes_{0};

jbyteArray AsyncCompressorJNIOptimized::compressData(JNIEnv* env, jobject compressor, jbyteArray inputData) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (inputData == nullptr || compressor == nullptr) {
        return nullptr;
    }
    
    jsize inputSize = env->GetArrayLength(inputData);
    if (inputSize <= 0) {
        return nullptr;
    }
    
    // 2. 获取输入数据
    jbyte* inputBytes = env->GetByteArrayElements(inputData, nullptr);
    if (!inputBytes) {
        return nullptr;
    }
    
    try {
        // 3. 调用core中的压缩功能
        // 简化实现：这里应该调用core中的AsyncCompressor
        // auto* compressor = reinterpret_cast<lattice::net::AsyncCompressor*>(compressorPtr);
        // auto result = compressor->compress(inputBytes, inputSize);
        
        // 临时：使用zlib直接压缩
        uLongf compressedSize = compressBound(inputSize);
        std::vector<Bytef> compressedData(compressedSize);
        
        int result = compress(compressedData.data(), &compressedSize, 
                             reinterpret_cast<const Bytef*>(inputBytes), inputSize);
        
        if (result != Z_OK) {
            return nullptr;
        }
        
        // 4. 创建结果数组
        jbyteArray resultArray = env->NewByteArray(compressedSize);
        env->SetByteArrayRegion(resultArray, 0, compressedSize, 
                               reinterpret_cast<jbyte*>(compressedData.data()));
        
        // 5. 更新统计
        totalCompressedBytes_ += compressedSize;
        totalUncompressedBytes_ += inputSize;
        
        return resultArray;
        
    } catch (const std::exception& e) {
        // 6. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return nullptr;
    } finally {
        // 7. 释放Java数组
        env->ReleaseByteArrayElements(inputData, inputBytes, 0);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("compressData", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jbyteArray AsyncCompressorJNIOptimized::decompressData(JNIEnv* env, jobject compressor, jbyteArray compressedData) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (compressedData == nullptr || compressor == nullptr) {
        return nullptr;
    }
    
    jsize compressedSize = env->GetArrayLength(compressedData);
    if (compressedSize <= 0) {
        return nullptr;
    }
    
    // 2. 获取压缩数据
    jbyte* compressedBytes = env->GetByteArrayElements(compressedData, nullptr);
    if (!compressedBytes) {
        return nullptr;
    }
    
    try {
        // 3. 调用core中的解压功能
        // 简化实现：这里应该调用core中的AsyncCompressor
        // auto* compressor = reinterpret_cast<lattice::net::AsyncCompressor*>(compressorPtr);
        // auto result = compressor->decompress(compressedBytes, compressedSize);
        
        // 临时：使用zlib直接解压
        uLongf decompressedSize = compressedSize * 4; // 估算解压后大小
        std::vector<Bytef> decompressedData(decompressedSize);
        
        int result = uncompress(decompressedData.data(), &decompressedSize,
                               reinterpret_cast<const Bytef*>(compressedBytes), compressedSize);
        
        if (result != Z_OK) {
            return nullptr;
        }
        
        // 4. 创建结果数组
        jbyteArray resultArray = env->NewByteArray(decompressedSize);
        env->SetByteArrayRegion(resultArray, 0, decompressedSize, 
                               reinterpret_cast<jbyte*>(decompressedData.data()));
        
        // 5. 更新统计
        totalUncompressedBytes_ += decompressedSize;
        totalCompressedBytes_ += compressedSize;
        
        return resultArray;
        
    } catch (const std::exception& e) {
        // 6. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return nullptr;
    } finally {
        // 7. 释放Java数组
        env->ReleaseByteArrayElements(compressedData, compressedBytes, 0);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("decompressData", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jobjectArray AsyncCompressorJNIOptimized::compressBatch(JNIEnv* env, jobject compressor, jobjectArray inputDataArray) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (inputDataArray == nullptr || compressor == nullptr) {
        return nullptr;
    }
    
    jsize arraySize = env->GetArrayLength(inputDataArray);
    if (arraySize <= 0) {
        return nullptr;
    }
    
    try {
        // 2. 创建结果数组
        jclass byteArrayClass = env->FindClass("[B");
        jobjectArray resultArray = env->NewObjectArray(arraySize, byteArrayClass, nullptr);
        
        // 3. 批量处理每个数据块
        for (jsize i = 0; i < arraySize; i++) {
            jobject inputData = env->GetObjectArrayElement(inputDataArray, i);
            if (inputData == nullptr) {
                continue;
            }
            
            // 转换为jbyteArray
            jbyteArray inputByteArray = reinterpret_cast<jbyteArray>(inputData);
            
            // 4. 调用单个压缩方法
            jbyteArray compressedResult = compressData(env, compressor, inputByteArray);
            
            // 5. 设置结果
            env->SetObjectArrayElement(resultArray, i, compressedResult);
            
            // 6. 清理局部引用
            env->DeleteLocalRef(inputData);
            if (compressedResult) {
                env->DeleteLocalRef(compressedResult);
            }
        }
        
        return resultArray;
        
    } catch (const std::exception& e) {
        // 7. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return nullptr;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("compressBatch", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jobjectArray AsyncCompressorJNIOptimized::decompressBatch(JNIEnv* env, jobject compressor, jobjectArray compressedDataArray) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (compressedDataArray == nullptr || compressor == nullptr) {
        return nullptr;
    }
    
    jsize arraySize = env->GetArrayLength(compressedDataArray);
    if (arraySize <= 0) {
        return nullptr;
    }
    
    try {
        // 2. 创建结果数组
        jclass byteArrayClass = env->FindClass("[B");
        jobjectArray resultArray = env->NewObjectArray(arraySize, byteArrayClass, nullptr);
        
        // 3. 批量处理每个数据块
        for (jsize i = 0; i < arraySize; i++) {
            jobject compressedData = env->GetObjectArrayElement(compressedDataArray, i);
            if (compressedData == nullptr) {
                continue;
            }
            
            // 转换为jbyteArray
            jbyteArray compressedByteArray = reinterpret_cast<jbyteArray>(compressedData);
            
            // 4. 调用单个解压方法
            jbyteArray decompressedResult = decompressData(env, compressor, compressedByteArray);
            
            // 5. 设置结果
            env->SetObjectArrayElement(resultArray, i, decompressedResult);
            
            // 6. 清理局部引用
            env->DeleteLocalRef(compressedData);
            if (decompressedResult) {
                env->DeleteLocalRef(decompressedResult);
            }
        }
        
        return resultArray;
        
    } catch (const std::exception& e) {
        // 7. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return nullptr;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("decompressBatch", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jlong AsyncCompressorJNIOptimized::initializeCompressor(JNIEnv* env, jint compressionLevel, jint windowBits) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (compressionLevel < 0 || compressionLevel > 9) {
        compressionLevel = 6; // 默认压缩级别
    }
    if (windowBits < 8 || windowBits > 15) {
        windowBits = 15; // 默认窗口大小
    }
    
    try {
        // 2. 创建压缩器实例（简化实现）
        // 这里应该调用core中的AsyncCompressor构造函数
        // auto* compressor = new lattice::net::AsyncCompressor(compressionLevel, windowBits);
        
        // 临时：返回压缩级别作为标识符
        return static_cast<jlong>(compressionLevel);
        
    } catch (const std::exception& e) {
        // 3. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return 0;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("initializeCompressor", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void AsyncCompressorJNIOptimized::shutdownCompressor(JNIEnv* env, jlong compressorPtr) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (compressorPtr == 0) {
        return;
    }
    
    try {
        // 2. 清理压缩器资源
        // auto* compressor = reinterpret_cast<lattice::net::AsyncCompressor*>(compressorPtr);
        // if (compressor) {
        //     delete compressor;
        // }
        
    } catch (const std::exception& e) {
        // 3. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("shutdownCompressor", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jlong AsyncCompressorJNIOptimized::getTotalCompressedBytes(JNIEnv* env, jlong compressorPtr) {
    return static_cast<jlong>(totalCompressedBytes_.load());
}

jlong AsyncCompressorJNIOptimized::getTotalUncompressedBytes(JNIEnv* env, jlong compressorPtr) {
    return static_cast<jlong>(totalUncompressedBytes_.load());
}

jfloat AsyncCompressorJNIOptimized::getCompressionRatio(JNIEnv* env, jlong compressorPtr) {
    uint64_t compressed = totalCompressedBytes_.load();
    uint64_t uncompressed = totalUncompressedBytes_.load();
    
    if (uncompressed == 0) {
        return 0.0f;
    }
    
    return static_cast<jfloat>(compressed) / static_cast<jfloat>(uncompressed);
}

jlong AsyncCompressorJNIOptimized::getAverageCompressionTimeMicros(JNIEnv* env, jlong compressorPtr) {
    uint64_t operations = totalOperations_.load();
    if (operations == 0) {
        return 0;
    }
    
    return static_cast<jlong>(totalTimeMicros_.load() / operations);
}

void AsyncCompressorJNIOptimized::setCompressionLevel(JNIEnv* env, jlong compressorPtr, jint level) {
    // 简化实现：验证压缩级别范围
    if (level >= 0 && level <= 9) {
        // 委托给core设置压缩级别
        // auto* compressor = reinterpret_cast<lattice::net::AsyncCompressor*>(compressorPtr);
        // compressor->setCompressionLevel(level);
    }
}

void AsyncCompressorJNIOptimized::setWindowBits(JNIEnv* env, jlong compressorPtr, jint windowBits) {
    // 简化实现：验证窗口大小范围
    if (windowBits >= 8 && windowBits <= 15) {
        // 委托给core设置窗口大小
        // auto* compressor = reinterpret_cast<lattice::net::AsyncCompressor*>(compressorPtr);
        // compressor->setWindowBits(windowBits);
    }
}

void AsyncCompressorJNIOptimized::resetCompressor(JNIEnv* env, jlong compressorPtr) {
    // 简化实现：重置统计信息
    if (compressorPtr != 0) {
        // 委托给core重置压缩器
        // auto* compressor = reinterpret_cast<lattice::net::AsyncCompressor*>(compressorPtr);
        // compressor->reset();
        
        // 重置JNI统计
        totalOperations_ = 0;
        totalTimeMicros_ = 0;
        totalCompressedBytes_ = 0;
        totalUncompressedBytes_ = 0;
    }
}

void AsyncCompressorJNIOptimized::logPerformanceStats(const char* operation, uint64_t durationMicros) {
    totalOperations_++;
    totalTimeMicros_.fetch_add(durationMicros);
}

void AsyncCompressorJNIOptimized::checkJNIException(JNIEnv* env, const char* methodName) {
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

} // namespace net
} // namespace jni
} // namespace lattice