#ifndef LIGHT_UPDATER_JNI_HPP
#define LIGHT_UPDATER_JNI_HPP

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// JNI_OnLoad 和 JNI_OnUnload
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

// NativeLightUpdater 类的方法
JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_initLightUpdater
  (JNIEnv *, jclass);

JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_releaseLightUpdater
  (JNIEnv *, jclass);

JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_addLightSource
  (JNIEnv *, jclass, jobject, jint, jboolean);

JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_removeLightSource
  (JNIEnv *, jclass, jobject, jboolean);

JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_propagateLightUpdates
  (JNIEnv *, jclass);

JNIEXPORT jboolean JNICALL Java_io_lattice_world_NativeLightUpdater_hasUpdates
  (JNIEnv *, jclass);

JNIEXPORT jint JNICALL Java_io_lattice_world_NativeLightUpdater_getLightLevel
  (JNIEnv *, jclass, jobject, jboolean);

#ifdef __cplusplus
}
#endif

#endif // LIGHT_UPDATER_JNI_HPP