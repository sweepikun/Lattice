#pragma once

#include <jni.h>
#include <memory>
#include <vector>

namespace lattice {
namespace jni {
namespace world {

/**
 * @brief 寻路算法 JNI桥接类 - 正确的JNI架构
 * 
 * JNI职责：
 * 1. 参数验证
 * 2. Java -> C++ 数据转换
 * 3. 调用core中的优化函数
 * 4. 结果转换 (C++ -> Java)
 * 5. JNI错误处理
 */
class PathfinderOptimizedBridge {
public:
    PathfinderOptimizedBridge() = default;
    ~PathfinderOptimizedBridge() = default;

    // 禁止拷贝和移动
    PathfinderOptimizedBridge(const PathfinderOptimizedBridge&) = delete;
    PathfinderOptimizedBridge& operator=(const PathfinderOptimizedBridge&) = delete;

    /**
     * @brief 查找路径 - 核心A*算法
     * @param env JNI环境
     * @param startX 起始X坐标
     * @param startY 起始Y坐标
     * @param startZ 起始Z坐标
     * @param targetX 目标X坐标
     * @param targetY 目标Y坐标
     * @param targetZ 目标Z坐标
     * @param mobType 生物类型 (0=被动, 1=中立, 2=敌对, 3=水生, 4=飞行)
     * @return 路径点数组 [x1,y1,z1,x2,y2,z2,...]
     */
    static jintArray findPath(JNIEnv* env, jclass clazz,
                              jint startX, jint startY, jint startZ,
                              jint targetX, jint targetY, jint targetZ,
                              jint mobType);

    /**
     * @brief 批量路径查找
     * @param env JNI环境
     * @param pathRequestsArray Java long[] - 路径请求数组
     * @param count 请求数量
     * @return 批量路径结果
     */
    static jlongArray findPathsBatch(JNIEnv* env, jclass clazz,
                                     jlongArray pathRequestsArray, jint count);

    /**
     * @brief 路径平滑化
     * @param env JNI环境
     * @param pathArray Java int[] - 原始路径
     * @param count 路径点数量
     * @return 平滑化后的路径
     */
    static jintArray smoothPath(JNIEnv* env, jclass clazz,
                               jintArray pathArray, jint count);

    /**
     * @brief 获取寻路统计
     * @param env JNI环境
     * @return 统计字符串
     */
    static jstring getPathfindingStats(JNIEnv* env, jclass clazz);

private:
    // JNI辅助函数
    static void throwJNIException(JNIEnv* env, const char* exceptionClass, const char* message);
    static bool validateCoordinates(JNIEnv* env, jint x, jint y, jint z);
    static bool validateMobType(JNIEnv* env, jint mobType);
    
    // 编码/解码路径数据
    static int64_t encodePathRequest(jint startX, jint startY, jint startZ,
                                    jint targetX, jint targetY, jint targetZ, jint mobType);
    static void decodePathRequest(int64_t encodedRequest, 
                                 jint& startX, jint& startY, jint& startZ,
                                 jint& targetX, jint& targetY, jint& targetZ, jint& mobType);
};

} // namespace world
} // namespace jni
} // namespace lattice
