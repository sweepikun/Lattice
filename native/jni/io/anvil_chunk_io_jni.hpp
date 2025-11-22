#pragma once

#include <jni.h>
#include <memory>
#include <string>
#include <vector>
#include "../../core/io/async_chunk_io.hpp"
#include "../../core/io/anvil_format.hpp"

namespace lattice {
namespace jni {
namespace io {

/**
 * Anvil格式Chunk I/O JNI桥接层
 * 提供Java接口访问Native Anvil格式功能
 */
class AnvilChunkIOJNI {
public:
    // ===== Java方法映射 =====
    
    /**
     * 初始化Anvil格式支持
     * JNI: public static native boolean initializeAnvilSupport();
     */
    static jboolean initializeAnvilSupport(JNIEnv* env, jclass clazz);
    
    /**
     * 设置存储格式
     * JNI: public static native void setStorageFormat(int format);
     */
    static void setStorageFormat(JNIEnv* env, jclass clazz, jint format);
    
    /**
     * 获取当前存储格式
     * JNI: public static native int getStorageFormat();
     */
    static jint getStorageFormat(JNIEnv* env, jclass clazz);
    
    /**
     * 检测世界存储格式
     * JNI: public static native int detectStorageFormat(String worldPath);
     */
    static jint detectStorageFormat(JNIEnv* env, jclass clazz, jstring worldPath);
    
    /**
     * 异步加载Anvil格式区块
     * JNI: public static native void loadChunkAnvilAsync(int worldId, int chunkX, int chunkZ, long callbackHandle);
     */
    static void loadChunkAnvilAsync(JNIEnv* env, jclass clazz, jint worldId, jint chunkX, jint chunkZ, jlong callbackHandle);
    
    /**
     * 异步保存Anvil格式区块
     * JNI: public static native void saveChunkAnvilAsync(byte[] chunkData, int chunkX, int chunkZ, int worldId, long callbackHandle);
     */
    static void saveChunkAnvilAsync(JNIEnv* env, jclass clazz, jbyteArray chunkData, jint chunkX, jint chunkZ, jint worldId, jlong callbackHandle);
    
    /**
     * 批量保存Anvil格式区块
     * JNI: public static native void saveChunksAnvilBatch(jlongArray chunkDataHandles, long callbackHandle);
     */
    static void saveChunksAnvilBatch(JNIEnv* env, jclass clazz, jlongArray chunkDataHandles, jlong callbackHandle);
    
    /**
     * 检查Anvil格式可用性
     * JNI: public static native boolean isAnvilFormatAvailable(String worldPath);
     */
    static jboolean isAnvilFormatAvailable(JNIEnv* env, jclass clazz, jstring worldPath);
    
    /**
     * 设置压缩格式（仅ZLIB）
     * JNI: public static native void setCompressionFormat(int format);
     */
    static void setCompressionFormat(JNIEnv* env, jclass clazz, jint format);
    
    /**
     * 获取压缩格式
     * JNI: public static native int getCompressionFormat();
     */
    static jint getCompressionFormat(JNIEnv* env, jclass clazz);
    
    /**
     * 获取Anvil格式性能统计
     * JNI: public static native String getAnvilPerformanceStats();
     */
    static jstring getAnvilPerformanceStats(JNIEnv* env, jclass clazz);
    
    /**
     * 转换为Anvil格式
     * JNI: public static native boolean convertToAnvilFormat(String worldPath, boolean backup);
     */
    static jboolean convertToAnvilFormat(JNIEnv* env, jclass clazz, jstring worldPath, jboolean backup);
    
    /**
     * 从Anvil格式转换
     * JNI: public static native boolean convertFromAnvilFormat(String worldPath, boolean backup);
     */
    static jboolean convertFromAnvilFormat(JNIEnv* env, jclass clazz, jstring worldPath, jboolean backup);
    
    // ===== 回调函数处理 =====
    
    /**
     * Java回调处理函数类型
     */
    using JavaCallback = std::function<void(JNIEnv*, jobject, bool, jbyteArray, jstring)>;
    
    /**
     * 注册Java回调
     */
    static void registerCallback(JNIEnv* env, jlong handle, JavaCallback callback);
    
    /**
     * 清理回调
     */
    static void cleanupCallback(JNIEnv* env, jlong handle);
    
private:
    // ===== 内部工具方法 =====
    
    /**
     * 转换Java字符串到C++字符串
     */
    static std::string jStringToString(JNIEnv* env, jstring jstr);
    
    /**
     * 转换C++字节向量到Java字节数组
     */
    static jbyteArray vectorToByteArray(JNIEnv* env, const std::vector<uint8_t>& data);
    
    /**
     * 转换Java字节数组到C++字节向量
     */
    static std::vector<uint8_t> byteArrayToVector(JNIEnv* env, jbyteArray jarr);
    
    /**
     * 创建异步结果对象
     */
    static jobject createAsyncIOResult(JNIEnv* env, const io::AsyncIOResult& result);
    
    /**
     * 获取AsyncChunkIO实例
     */
    static io::AsyncChunkIO* getAsyncChunkIO();
    
    // ===== 静态成员 =====
    
    /**
     * 回调映射表
     */
    static std::unordered_map<jlong, JavaCallback> callbacks_;
    
    /**
     * AsyncChunkIO实例（线程局部存储）
     */
    static thread_local io::AsyncChunkIO* asyncIOInstance_;
    
    /**
     * 类引用缓存
     */
    static jclass asyncIOResultClass_;
    static jclass byteArrayClass_;
};

// ===== 回调包装器 =====

/**
 * Java回调包装器
 */
struct JavaCallbackWrapper {
    jobject callbackObj;
    jmethodID onSuccessMethod;
    jmethodID onFailureMethod;
    
    JavaCallbackWrapper() : callbackObj(nullptr), onSuccessMethod(nullptr), onFailureMethod(nullptr) {}
    JavaCallbackWrapper(jobject obj, jmethodID success, jmethodID failure)
        : callbackObj(obj), onSuccessMethod(success), onFailureMethod(failure) {}
};

} // namespace io
} // namespace jni
} // namespace lattice