#pragma once

#include <jni.h>
#include <memory>
#include <vector>

namespace lattice {
namespace jni {
namespace net {

/**
 * @brief 内存Arena JNI桥接类 - 正确的JNI架构
 * 
 * JNI职责：
 * 1. 参数验证
 * 2. Java -> C++ 数据转换
 * 3. 调用core中的优化函数
 * 4. 结果转换 (C++ -> Java)
 * 5. JNI错误处理
 */
class MemoryArenaOptimizedBridge {
public:
    MemoryArenaOptimizedBridge() = default;
    ~MemoryArenaOptimizedBridge() = default;

    // 禁止拷贝和移动
    MemoryArenaOptimizedBridge(const MemoryArenaOptimizedBridge&) = delete;
    MemoryArenaOptimizedBridge& operator=(const MemoryArenaOptimizedBridge&) = delete;

    /**
     * @brief 获取线程局部内存分配器
     * @param env JNI环境
     * @param clazz Java类
     * @return 内存分配器指针
     */
    static jlong getThreadLocalArena(JNIEnv* env, jclass clazz);

    /**
     * @brief 内存分配
     * @param env JNI环境
     * @param arenaPtr 内存分配器指针
     * @param size 分配大小
     * @return 分配的内存指针
     */
    static jlong allocateMemory(JNIEnv* env, jclass clazz, jlong arenaPtr, jlong size);

    /**
     * @brief 安全内存分配（带验证）
     * @param env JNI环境
     * @param arenaPtr 内存分配器指针
     * @param size 分配大小
     * @return 分配的内存指针或0（失败）
     */
    static jlong safeAllocate(JNIEnv* env, jclass clazz, jlong arenaPtr, jlong size);

    /**
     * @brief 批量内存分配
     * @param env JNI环境
     * @param arenaPtr 内存分配器指针
     * @param sizeArray 分配大小数组
     * @return 分配的内存指针数组
     */
    static jlongArray batchAllocate(JNIEnv* env, jclass clazz, 
                                    jlong arenaPtr, jintArray sizeArray);

    /**
     * @brief 获取内存使用统计
     * @param env JNI环境
     * @param arenaPtr 内存分配器指针
     * @return 内存统计字符串
     */
    static jstring getMemoryStats(JNIEnv* env, jclass clazz, jlong arenaPtr);

    /**
     * @brief 清理当前线程的Arena
     * @param env JNI环境
     */
    static void clearThreadArena(JNIEnv* env, jclass clazz);

private:
    // JNI辅助函数
    static void throwJNIException(JNIEnv* env, const char* exceptionClass, const char* message);
    static bool validateArenaPointer(JNIEnv* env, jlong arenaPtr);
    static bool validateAllocationSize(JNIEnv* env, jlong size);
};

} // namespace net
} // namespace jni
} // namespace lattice
