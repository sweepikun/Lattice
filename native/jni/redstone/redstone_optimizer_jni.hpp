#ifndef REDSTONE_OPTIMIZER_JNI_HPP
#define REDSTONE_OPTIMIZER_JNI_HPP

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// JNI_OnLoad 和 JNI_OnUnload
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

// NativeRedstoneOptimizer 类的方法
JNIEXPORT jint JNICALL Java_io_lattice_redstone_NativeRedstoneOptimizer_nativeGetRedstonePower
  (JNIEnv *, jclass, jint, jint, jint);

JNIEXPORT jint JNICALL Java_io_lattice_redstone_NativeRedstoneOptimizer_nativeCalculateNetworkPower
  (JNIEnv *, jclass, jint, jint, jint, jint);

JNIEXPORT void JNICALL Java_io_lattice_redstone_NativeRedstoneOptimizer_nativeInvalidateNetworkCache
  (JNIEnv *, jclass, jint, jint, jint);

#ifdef __cplusplus
}
#endif

#endif // REDSTONE_OPTIMIZER_JNI_HPP