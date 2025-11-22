#ifndef LATTICE_PAPER_COMPATIBLE_REDSTONE_JNI_HPP
#define LATTICE_PAPER_COMPATIBLE_REDSTONE_JNI_HPP

#include <jni.h>
#include "../core/redstone/paper_compatible_redstone_engine.hpp"
#include "../jni/jni_helper.hpp"

#ifdef __cplusplus
extern "C" {
#endif

// ========== JNI OnLoad/OnUnload ==========

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

// ========== Paper兼容的RedstoneEngine ==========

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_isNativeEngineLoaded(
    JNIEnv* env, jclass clazz);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativeIsPowered(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z);

JNIEXPORT jint JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativeGetPower(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z);

JNIEXPORT void JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativeUpdatePower(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jint power);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativeRegisterWire(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativeRegisterRepeater(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jint delay);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativeRegisterComparator(
    JNIEnv* env, jclass clazz, jlong enginePtr, jint x, jint y, jint z, jboolean subtractMode);

JNIEXPORT void JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativeTick(
    JNIEnv* env, jclass clazz, jlong enginePtr);

// ========== 性能监控 ==========

JNIEXPORT jobject JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativeGetPerformanceStats(
    JNIEnv* env, jclass clazz, jlong enginePtr);

JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativePing(
    JNIEnv* env, jclass clazz, jlong enginePtr);

JNIEXPORT void JNICALL Java_io_lattice_redstone_nativebridge_RedstoneJNI_nativeRestart(
    JNIEnv* env, jclass clazz, jlong enginePtr);

#ifdef __cplusplus
}
#endif

#endif // LATTICE_PAPER_COMPATIBLE_REDSTONE_JNI_HPP