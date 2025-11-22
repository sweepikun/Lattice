#ifndef ADVANCED_LIGHT_ENGINE_JNI_HPP
#define ADVANCED_LIGHT_ENGINE_JNI_HPP

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// AdvancedLightEngine JNI 桥接

/**
 * 初始化透光表 - 启动时调用一次
 * @param materialIds Material ID数组
 * @param opacities 对应的透光值数组 (0-15)
 */
JNIEXPORT void JNICALL 
Java_io_lattice_world_AdvancedLightEngine_initOpacityTable(
    JNIEnv* env, jclass, jintArray materialIds, jbyteArray opacities);

/**
 * 处理方块变化事件
 * @param x 方块X坐标
 * @param y 方块Y坐标  
 * @param z 方块Z坐标
 * @param materialId Material ID
 */
JNIEXPORT void JNICALL
Java_io_lattice_world_AdvancedLightEngine_onBlockChange(
    JNIEnv* env, jclass, jint x, jint y, jint z, jint materialId);

/**
 * 每tick调用 - 处理批量光照更新
 */
JNIEXPORT void JNICALL
Java_io_lattice_world_AdvancedLightEngine_tick(
    JNIEnv* env, jclass);

/**
 * 获取光照级别
 * @param x 方块X坐标
 * @param y 方块Y坐标
 * @param z 方块Z坐标  
 * @param isSky 是否为天空光 (true=天空光, false=方块光)
 * @return 光照级别 (0-15)
 */
JNIEXPORT jbyte JNICALL
Java_io_lattice_world_AdvancedLightEngine_getLightLevel(
    JNIEnv* env, jclass, jint x, jint y, jint z, jboolean isSky);

/**
 * 检查是否有待处理的光照更新
 * @return true如果有更新，false否则
 */
JNIEXPORT jboolean JNICALL
Java_io_lattice_world_AdvancedLightEngine_hasUpdates(
    JNIEnv* env, jclass);

/**
 * 清除指定区块的光照数据
 * @param chunkX 区块X坐标
 * @param chunkZ 区块Z坐标
 */
JNIEXPORT void JNICALL
Java_io_lattice_world_AdvancedLightEngine_clearChunk(
    JNIEnv* env, jclass, jint chunkX, jint chunkZ);

/**
 * 获取内存使用统计
 * @return 格式化的内存使用信息字符串
 */
JNIEXPORT jstring JNICALL
Java_io_lattice_world_AdvancedLightEngine_getMemoryStats(
    JNIEnv* env, jclass);

/**
 * 强制刷新所有待处理的光照更新 (用于测试)
 */
JNIEXPORT void JNICALL
Java_io_lattice_world_AdvancedLightEngine_forceFlush(
    JNIEnv* env, jclass);

#ifdef __cplusplus
}
#endif

#endif // ADVANCED_LIGHT_ENGINE_JNI_HPP
