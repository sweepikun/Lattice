#pragma once

#include <jni.h>
#include <memory>
#include <functional>

namespace lattice {
namespace jni {
namespace io {

/**
 * @brief 异步区块I/O JNI桥接类 - 正确的JNI架构
 * 
 * JNI职责：
 * 1. 参数验证
 * 2. Java -> C++ 数据转换
 * 3. 调用core中的优化函数
 * 4. 结果转换 (C++ -> Java)
 * 5. JNI错误处理
 */
class AsyncChunkIOOptimizedBridge {
public:
    AsyncChunkIOOptimizedBridge() = default;
    ~AsyncChunkIOOptimizedBridge() = default;

    // 禁止拷贝和移动
    AsyncChunkIOOptimizedBridge(const AsyncChunkIOOptimizedBridge&) = delete;
    AsyncChunkIOOptimizedBridge& operator=(const AsyncChunkIOOptimizedBridge&) = delete;

    /**
     * @brief 异步加载区块
     * @param env JNI环境
     * @param clazz Java类
     * @param worldId 世界ID
     * @param chunkX 区块X坐标
     * @param chunkZ 区块Z坐标
     * @param callback Java回调对象 (可选)
     * @return 区块加载是否成功
     */
    static jboolean loadChunkAsync(JNIEnv* env, jclass clazz,
                                   jint worldId, jint chunkX, jint chunkZ,
                                   jobject callback);

    /**
     * @brief 异步保存区块
     * @param env JNI环境
     * @param clazz Java类
     * @param worldId 世界ID
     * @param chunkX 区块X坐标
     * @param chunkZ 区块Z坐标
     * @param chunkData Java byte[] - NBT数据
     * @param callback Java回调对象 (可选)
     * @return 区块保存是否成功
     */
    static jboolean saveChunkAsync(JNIEnv* env, jclass clazz,
                                   jint worldId, jint chunkX, jint chunkZ,
                                   jbyteArray chunkData, jobject callback);

    /**
     * @brief 批量保存区块 - 关键优化点
     * @param env JNI环境
     * @param clazz Java类
     * @param worldIdArray Java int[] - 世界ID数组
     * @param chunkXArray Java int[] - X坐标数组
     * @param chunkZArray Java int[] - Z坐标数组
     * @param chunkDataArray Java byte[][] - NBT数据数组
     * @param count 数据数量
     * @return 批量操作结果
     */
    static jboolean saveChunksBatch(JNIEnv* env, jclass clazz,
                                    jintArray worldIdArray, jintArray chunkXArray,
                                    jintArray chunkZArray, jobjectArray chunkDataArray,
                                    jint count);

    /**
     * @brief 获取I/O性能统计
     * @param env JNI环境
     * @return 性能统计字符串
     */
    static jstring getIOStats(JNIEnv* env, jclass clazz);

private:
    // JNI辅助函数
    static void throwJNIException(JNIEnv* env, const char* exceptionClass, const char* message);
    static bool validateChunkCoordinates(JNIEnv* env, jint chunkX, jint chunkZ);
};

} // namespace io
} // namespace jni
} // namespace lattice
