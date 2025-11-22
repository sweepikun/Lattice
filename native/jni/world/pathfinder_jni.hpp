#ifndef PATHFINDER_JNI_HPP
#define PATHFINDER_JNI_HPP

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// JNI_OnLoad 和 JNI_OnUnload
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

// NativePathfinder 类的方法
JNIEXPORT jobjectArray JNICALL Java_io_lattice_world_NativePathfinder_nativeOptimizePathfinding
  (JNIEnv *, jclass, jint, jint, jint, jint, jint, jint, jint);

JNIEXPORT jobject JNICALL Java_io_lattice_world_NativePathfinder_nativeGetMobPathfindingParams
  (JNIEnv *, jclass, jint);

#ifdef __cplusplus
}
#endif

#endif // PATHFINDER_JNI_HPP