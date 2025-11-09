#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// 异步压缩相关函数
JNIEXPORT void JNICALL Java_io_lattice_network_AsyncCompression_initializeAsyncCompressor
  (JNIEnv *, jclass, jint);

JNIEXPORT void JNICALL Java_io_lattice_network_AsyncCompression_setWorkerCount
  (JNIEnv *, jclass, jint);

JNIEXPORT void JNICALL Java_io_lattice_network_AsyncCompression_compressAsync
  (JNIEnv *, jclass, jobject, jint, jobject, jint, jobject);

JNIEXPORT jint JNICALL Java_io_lattice_network_DynamicCompression_suggestCompressionLevel
  (JNIEnv *, jclass, jint, jdouble, jdouble, jint, jint, jint);

#ifdef __cplusplus
}
#endif