#pragma once

#include <jni.h>
#include <string>
#include <vector>
#include <memory>

namespace lattice {
namespace jni {
namespace io {

/**
 * @brief NBT序列化JNI桥接类 - 正确的JNI架构
 * 
 * JNI层职责：
 * 1. 参数转换（Java -> C++）
 * 2. 调用core/entity中的优化函数
 * 3. 返回结果（C++ -> Java）
 * 
 * 注意：所有实际的优化算法都在core/io/中实现
 */
class NBTOptimizedJNIBridge {
public:
    NBTOptimizedJNIBridge() = default;
    ~NBTOptimizedJNIBridge() = default;

    // 禁止拷贝和移动
    NBTOptimizedJNIBridge(const NBTOptimizedJNIBridge&) = delete;
    NBTOptimizedJNIBridge& operator=(const NBTOptimizedJNIBridge&) = delete;
    NBTOptimizedJNIBridge(NBTOptimizedJNIBridge&&) = delete;
    NBTOptimizedJNIBridge& operator=(NBTOptimizedJNIBridge&&) = delete;

    /**
     * @brief 优化NBT数据序列化
     * @param env JNI环境
     * @param nbtData Java byte[] - NBT数据
     * @param size 数据大小
     * @return 优化的序列化数据
     */
    static jbyteArray serializeOptimized(JNIEnv* env, jclass clazz, 
                                         jbyteArray nbtData, jint size);

    /**
     * @brief 优化NBT数据反序列化  
     * @param env JNI环境
     * @param serializedData Java byte[] - 序列化数据
     * @param size 数据大小
     * @return NBT原始数据
     */
    static jbyteArray deserializeOptimized(JNIEnv* env, jclass clazz,
                                          jbyteArray serializedData, jint size);

    /**
     * @brief 批量优化NBT序列化操作
     * @param env JNI环境
     * @param nbtDataArray Java byte[][] - NBT数据数组
     * @param count 数据数量
     * @return 批量优化的序列化数据
     */
    static jbyteArrayArray batchSerializeOptimized(JNIEnv* env, jclass clazz,
                                                   jobjectArray nbtDataArray, jint count);

    /**
     * @brief 获取NBT序列化性能统计
     * @param env JNI环境
     * @return 性能统计字符串
     */
    static jstring getPerformanceStats(JNIEnv* env, jclass clazz);

private:
    // JNI辅助函数
    static jbyteArray createJavaByteArray(JNIEnv* env, const void* data, size_t size);
    static jbyteArrayArray createJavaByteArrayArray(JNIEnv* env, const std::vector<std::vector<uint8_t>>& data);
    static void throwJavaException(JNIEnv* env, const char* exceptionClass, const char* message);
};

} // namespace io
} // namespace jni  
} // namespace lattice
