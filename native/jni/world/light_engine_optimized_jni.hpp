#pragma once

#include <jni.h>
#include <memory>
#include <vector>

namespace lattice {
namespace jni {
namespace world {

/**
 * @brief 光照引擎 JNI桥接类 - 正确的JNI架构
 * 
 * JNI职责：
 * 1. 参数验证
 * 2. Java -> C++ 数据转换
 * 3. 调用core中的优化函数
 * 4. 结果转换 (C++ -> Java)
 * 5. JNI错误处理
 */
class LightEngineOptimizedBridge {
public:
    LightEngineOptimizedBridge() = default;
    ~LightEngineOptimizedBridge() = default;

    // 禁止拷贝和移动
    LightEngineOptimizedBridge(const LightEngineOptimizedBridge&) = delete;
    LightEngineOptimizedBridge& operator=(const LightEngineOptimizedBridge&) = delete;

    /**
     * @brief 添加光源
     * @param env JNI环境
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @param level 光照级别 (0-15)
     * @param lightType 光照类型 (0=方块光, 1=天空光)
     * @return 操作是否成功
     */
    static jboolean addLightSource(JNIEnv* env, jclass clazz,
                                   jint x, jint y, jint z, jint level, jint lightType);

    /**
     * @brief 移除光源
     * @param env JNI环境
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @param lightType 光照类型 (0=方块光, 1=天空光)
     * @return 操作是否成功
     */
    static jboolean removeLightSource(JNIEnv* env, jclass clazz,
                                      jint x, jint y, jint z, jint lightType);

    /**
     * @brief 执行光照传播 - 关键优化点
     * @param env JNI环境
     * @return 传播是否成功
     */
    static jboolean propagateLighting(JNIEnv* env, jclass clazz);

    /**
     * @brief 批量光照更新
     * @param env JNI环境
     * @param lightDataArray Java long[] - 光照数据数组（编码坐标和级别）
     * @param count 数据数量
     * @return 批量操作结果
     */
    static jboolean updateLightingBatch(JNIEnv* env, jclass clazz,
                                        jlongArray lightDataArray, jint count);

    /**
     * @brief 获取光照级别
     * @param env JNI环境
     * @param x X坐标
     * @param y Y坐标
     * @param z Z坐标
     * @param lightType 光照类型 (0=方块光, 1=天空光)
     * @return 光照级别
     */
    static jbyte getLightLevel(JNIEnv* env, jclass clazz,
                               jint x, jint y, jint z, jint lightType);

    /**
     * @brief 获取光照引擎统计
     * @param env JNI环境
     * @return 统计字符串
     */
    static jstring getLightingStats(JNIEnv* env, jclass clazz);

private:
    // JNI辅助函数
    static void throwJNIException(JNIEnv* env, const char* exceptionClass, const char* message);
    static bool validateCoordinates(JNIEnv* env, jint x, jint y, jint z);
    static bool validateLightLevel(JNIEnv* env, jint level);
    static bool validateLightType(JNIEnv* env, jint lightType);
    
    // 编码/解码光照数据
    static int64_t encodeLightData(jint x, jint y, jint z, jint level);
    static void decodeLightData(int64_t encodedData, jint& x, jint& y, jint& z, jint& level);
};

} // namespace world
} // namespace jni
} // namespace lattice
