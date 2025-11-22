#ifndef LATTICE_REDSTONE_ENGINE_JNI_HPP
#define LATTICE_REDSTONE_ENGINE_JNI_HPP

#include <jni.h>
#include "../../core/redstone/redstone_engine.hpp"
#include "../../core/redstone/redstone_components.hpp"
#include "../jni_helper.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// ========== JNI OnLoad/OnUnload ==========

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

// ========== RedstoneEngine 主类 ==========

JNIEXPORT jlong JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeCreateEngine(JNIEnv* env, jclass clazz);

JNIEXPORT void JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeDestroyEngine(JNIEnv* env, jclass clazz, jlong enginePtr);

JNIEXPORT void JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeTick(JNIEnv* env, jclass clazz, jlong enginePtr);

// ========== 组件注册 ==========

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeRegisterWire(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeRegisterRepeater(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jint delay);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeRegisterComparator(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jboolean subtractMode);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeRegisterTorch(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jboolean powered);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeRegisterLever(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jboolean powered);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeRegisterButton(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jboolean wooden);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeRegisterPressurePlate(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jint plateType);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeRegisterObserver(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jint facing);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeRegisterPiston(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jboolean sticky, jint facing);

// ========== 组件操作 ==========

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeUpdateComponent(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jint inputSignal);

JNIEXPORT jint JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeGetComponentOutput(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeUnregisterComponent(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z);

// ========== 查询和统计 ==========

JNIEXPORT jintArray JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeQueryComponentsInRange(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint centerX, jint centerY, jint centerZ, jint radius);

JNIEXPORT jlong JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeGetCurrentTick(JNIEnv* env, jclass clazz, jlong enginePtr);

JNIEXPORT jobject JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeGetPerformanceStats(
    JNIEnv* env, jclass clazz, jlong enginePtr);

// ========== 行为验证和降级 ==========

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeValidateBehavior(
    JNIEnv* env, jclass clazz, jlong enginePtr, jintArray javaSignals, jintArray nativeSignals);

JNIEXPORT void JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeEnableGracefulDegradation(
    JNIEnv* env, jclass clazz, jlong enginePtr);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeIsGracefulDegradationEnabled(
    JNIEnv* env, jclass clazz, jlong enginePtr);

// ========== 配置和调试 ==========

JNIEXPORT void JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeResetPerformanceStats(
    JNIEnv* env, jclass clazz, jlong enginePtr);

JNIEXPORT void JNICALL Java_io_lattice_redstone_RedstoneEngine_nativeSetDebugMode(
    JNIEnv* env, jclass clazz, jlong enginePtr, jboolean enabled);

#ifdef __cplusplus
}
#endif

// ========== 工具函数 ==========

namespace lattice {
namespace redstone {
namespace jni {

    // 将 Java 数组转换为 C++ 向量
    std::vector<int> jintArrayToVector(JNIEnv* env, jintArray array);
    
    // 转换位置结构
    inline lattice::redstone::Position jniToPosition(JNIEnv* env, jint x, jint y, jint z) {
        return lattice::redstone::Position(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z));
    }
    
    // 创建性能统计对象
    jobject createPerformanceStatsObject(JNIEnv* env, const RedstoneEngine::PerformanceStats& stats);
    
    // 错误处理
    void throwRedstoneException(JNIEnv* env, const char* message);
    void logRedstoneError(const char* format, ...);
    
} // namespace jni
} // namespace redstone
} // namespace lattice

#endif // LATTICE_REDSTONE_ENGINE_JNI_HPP