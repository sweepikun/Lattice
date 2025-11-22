#pragma once

#include <jni.h>
#include <string>
#include <vector>

namespace lattice {
namespace jni {
namespace compression {

// JNI包装函数声明
extern "C" {

/**
 * @brief 初始化原生压缩引擎
 */
JNIEXPORT void JNICALL
Java_com_lattice_compression_NativeCompressionEngine_initialize(JNIEnv* env, jclass clazz, jint compressionLevel);

/**
 * @brief 压缩数据
 */
JNIEXPORT jbyteArray JNICALL
Java_com_lattice_compression_NativeCompressionEngine_compress(JNIEnv* env, jclass clazz, jbyteArray data);

/**
 * @brief 解压数据
 */
JNIEXPORT jbyteArray JNICALL
Java_com_lattice_compression_NativeCompressionEngine_decompress(JNIEnv* env, jclass clazz, jbyteArray compressedData);

/**
 * @brief 关闭压缩引擎
 */
JNIEXPORT void JNICALL
Java_com_lattice_compression_NativeCompressionEngine_shutdown(JNIEnv* env, jclass clazz);

} // extern "C"

} // namespace compression
} // namespace jni
} // namespace lattice