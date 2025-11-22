#pragma once

#include <jni.h>
#include <atomic>
#include "../../core/io/async_chunk_io.hpp"

namespace lattice {
namespace jni {
namespace io {

// ===== JNI 包装器类 =====

class NativeChunkIOJNI {
public:
    // JNI 方法声明
    static void initialize(JNIEnv* env, jclass clazz, jboolean enableDirectIO, jint maxOps);
    
    static void loadChunkAsync(JNIEnv* env, jclass clazz,
                              jint worldId, jint chunkX, jint chunkZ,
                              jobject callback);
    
    static void saveChunksBatch(JNIEnv* env, jclass clazz,
                               jobjectArray chunksArray,
                               jobject batchCallback);
    
    static jint getPlatformFeatures(JNIEnv* env, jclass clazz);
    
    static void setMaxConcurrentIO(JNIEnv* env, jclass clazz, jint maxOps);
    static void enableDirectIO(JNIEnv* env, jclass clazz, jboolean enable);
    
    // 获取 JNI 方法 ID
    static void cacheMethodIDs(JNIEnv* env);

private:
    // 方法 ID 缓存
    static jmethodID chunkCallback_onComplete_;
    static jmethodID batchCallback_onBatchComplete_;
    
    // 字段 ID 缓存
    static jfieldID chunk_x_field_;
    static jfieldID chunk_z_field_;
    static jfieldID chunk_data_field_;
    static jfieldID chunk_worldId_field_;
    
    // 全局状态
    static std::atomic<bool> initialized_;
    static std::atomic<int> maxConcurrentIO_;
    static std::atomic<bool> directIOEnabled_;
    
    // 内部辅助方法
    static void handleChunkCallback(JNIEnv* env, jobject callback, 
                                   const io::AsyncIOResult& result);
    static void handleBatchCallback(JNIEnv* env, jobject batchCallback,
                                   const std::vector<io::AsyncIOResult>& results);
    static jbyteArray createByteArray(JNIEnv* env, const std::vector<char>& data);
    static jstring createString(JNIEnv* env, const std::string& str);
    
    // 异常处理
    static void throwJavaException(JNIEnv* env, const char* message);
};

// ===== JNI 方法映射 =====

struct JNIMethodMapping {
    const char* name;
    const char* signature;
    void* functionPointer;
};

static const JNIMethodMapping g_chunk_io_methods[] = {
    {"initialize", "(ZI)V", (void*)NativeChunkIOJNI::initialize},
    {"loadChunkAsync", "(IIILcom/lattice/world/ChunkIOCallback;)V", (void*)NativeChunkIOJNI::loadChunkAsync},
    {"saveChunksBatch", "([Lcom/lattice/world/ChunkData;Lcom/lattice/world/BatchIOCallback;)V", (void*)NativeChunkIOJNI::saveChunksBatch},
    {"getPlatformFeatures", "()I", (void*)NativeChunkIOJNI::getPlatformFeatures},
    {"setMaxConcurrentIO", "(I)V", (void*)NativeChunkIOJNI::setMaxConcurrentIO},
    {"enableDirectIO", "(Z)V", (void*)NativeChunkIOJNI::enableDirectIO},
    {nullptr, nullptr, nullptr}
};

// ===== 工具函数 =====

inline jclass findGlobalClass(JNIEnv* env, const char* className) {
    jclass localClass = env->FindClass(className);
    if (!localClass) return nullptr;
    
    jclass globalClass = (jclass)env->NewGlobalRef(localClass);
    env->DeleteLocalRef(localClass);
    return globalClass;
}

inline jmethodID getMethodID(JNIEnv* env, jclass clazz, const char* name, const char* signature) {
    jmethodID methodId = env->GetMethodID(clazz, name, signature);
    if (!methodId) {
        throw std::runtime_error(std::string("Failed to find method: ") + name + " " + signature);
    }
    return methodId;
}

inline jfieldID getFieldID(JNIEnv* env, jclass clazz, const char* name, const char* signature) {
    jfieldID fieldId = env->GetFieldID(clazz, name, signature);
    if (!fieldId) {
        throw std::runtime_error(std::string("Failed to find field: ") + name + " " + signature);
    }
    return fieldId;
}

} // namespace io
} // namespace jni
} // namespace lattice