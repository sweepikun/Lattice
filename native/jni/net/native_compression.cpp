#include "native_compression.hpp"
#include "../../core/net/native_compressor.hpp"
#include "../../core/net/async_compressor.hpp"
#include <jni.h>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <memory>

// JNI_OnLoad - 初始化函数
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    return JNI_VERSION_1_8;
}

// JNI_OnUnload - 清理函数
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    // 清理资源
}

// 压缩数据 (Zlib格式)
JNIEXPORT jlong JNICALL Java_io_lattice_network_NativeCompression_nativeCompressZlib
  (JNIEnv *env, jclass clazz, jobject srcBuffer, jlong srcLen, jobject dstBuffer, jlong dstCapacity, jint level) {
    try {
        // 获取DirectBuffer地址
        char* src = static_cast<char*>(env->GetDirectBufferAddress(srcBuffer));
        char* dst = static_cast<char*>(env->GetDirectBufferAddress(dstBuffer));
        
        if (!src || !dst) {
            return -1; // 错误：无法获取buffer地址
        }
        
        // 获取线程本地的压缩器实例
        auto* compressor = lattice::net::NativeCompressor::forThread(level);
        if (!compressor) {
            return -2; // 错误：无法创建压缩器
        }
        
        // 执行压缩
        size_t result = compressor->compressZlib(src, srcLen, dst, dstCapacity);
        return static_cast<jlong>(result);
    } catch (...) {
        // 发生异常时返回-3
        return -3;
    }
}

// 解压缩数据 (Zlib格式)
JNIEXPORT jlong JNICALL Java_io_lattice_network_NativeCompression_nativeDecompressZlib
  (JNIEnv *env, jclass clazz, jobject srcBuffer, jlong srcLen, jobject dstBuffer, jlong dstCapacity) {
    try {
        // 获取DirectBuffer地址
        char* src = static_cast<char*>(env->GetDirectBufferAddress(srcBuffer));
        char* dst = static_cast<char*>(env->GetDirectBufferAddress(dstBuffer));
        
        if (!src || !dst) {
            return -1; // 错误：无法获取buffer地址
        }
        
        // 获取线程本地的压缩器实例（解压缩不需要特定级别）
        auto* compressor = lattice::net::NativeCompressor::forThread(6);
        if (!compressor) {
            return -2; // 错误：无法创建压缩器
        }
        
        // 执行解压缩
        size_t result = compressor->decompressZlib(src, srcLen, dst, dstCapacity);
        return static_cast<jlong>(result);
    } catch (...) {
        // 发生异常时返回-3
        return -3;
    }
}

// 异步压缩数据
JNIEXPORT void JNICALL Java_io_lattice_network_NativeCompression_nativeCompressAsync
  (JNIEnv *env, jclass clazz, jobject srcBuffer, jlong srcLen, jobject dstBuffer, jlong dstCapacity, 
   jint level, jobject callback) {
    try {
        // 获取DirectBuffer地址
        char* src = static_cast<char*>(env->GetDirectBufferAddress(srcBuffer));
        char* dst = static_cast<char*>(env->GetDirectBufferAddress(dstBuffer));
        
        if (!src || !dst) {
            // 调用回调函数报告错误
            jclass callbackClass = env->GetObjectClass(callback);
            jmethodID onErrorMethod = env->GetMethodID(callbackClass, "onError", "(I)V");
            if (onErrorMethod) {
                env->CallVoidMethod(callback, onErrorMethod, -1);
            }
            return;
        }
        
        // 创建输入和输出数据的shared_ptr副本
        auto inputData = std::make_shared<std::vector<char>>(src, src + srcLen);
        auto outputBuffer = std::make_shared<std::vector<char>>(dstCapacity);
        
        // 创建Java VM和回调对象的全局引用，以便在线程中使用
        JavaVM* jvm;
        env->GetJavaVM(&jvm);
        jobject callbackGlobalRef = env->NewGlobalRef(callback);
        
        // 提交异步压缩任务
        lattice::net::AsyncCompressor::getInstance().compressAsync(
            inputData, 
            outputBuffer, 
            level,
            [jvm, callbackGlobalRef, dst](bool success, size_t outputSize) {
                // 在回调中获取JNIEnv
                JNIEnv* callbackEnv;
                bool needDetach = false;
                
                // 尝试获取当前线程的JNIEnv
                jint result = jvm->GetEnv((void**)&callbackEnv, JNI_VERSION_1_8);
                if (result == JNI_EDETACHED) {
                    // 如果当前线程未附加到JVM，需要附加
                    if (jvm->AttachCurrentThread((void**)&callbackEnv, nullptr) == JNI_OK) {
                        needDetach = true;
                    } else {
                        // 附加失败，无法调用Java方法
                        jvm->DeleteGlobalRef(callbackGlobalRef);
                        return;
                    }
                } else if (result != JNI_OK) {
                    // 获取JNIEnv失败
                    jvm->DeleteGlobalRef(callbackGlobalRef);
                    return;
                }
                
                // 获取回调类和方法
                jclass callbackClass = callbackEnv->GetObjectClass(callbackGlobalRef);
                
                if (success) {
                    // 复制压缩结果到目标缓冲区
                    if (outputSize <= (size_t)callbackEnv->GetDirectBufferCapacity(callbackEnv->NewDirectByteBuffer(dst, outputSize))) {
                        memcpy(dst, outputBuffer->data(), outputSize);
                    }
                    
                    // 调用onSuccess方法
                    jmethodID onSuccessMethod = callbackEnv->GetMethodID(callbackClass, "onSuccess", "(J)V");
                    if (onSuccessMethod) {
                        callbackEnv->CallVoidMethod(callbackGlobalRef, onSuccessMethod, (jlong)outputSize);
                    }
                } else {
                    // 调用onError方法
                    jmethodID onErrorMethod = callbackEnv->GetMethodID(callbackClass, "onError", "(I)V");
                    if (onErrorMethod) {
                        callbackEnv->CallVoidMethod(callbackGlobalRef, onErrorMethod, -4); // -4表示压缩失败
                    }
                }
                
                // 清理全局引用
                callbackEnv->DeleteGlobalRef(callbackGlobalRef);
                
                // 如果之前附加了线程，现在需要分离
                if (needDetach) {
                    jvm->DetachCurrentThread();
                }
            }
        );
    } catch (...) {
        // 异常处理
        jclass callbackClass = env->GetObjectClass(callback);
        jmethodID onErrorMethod = env->GetMethodID(callbackClass, "onError", "(I)V");
        if (onErrorMethod) {
            env->CallVoidMethod(callback, onErrorMethod, -3); // -3表示异常
        }
    }
}

// 设置异步压缩工作线程数
JNIEXPORT void JNICALL Java_io_lattice_network_NativeCompression_nativeSetWorkerCount
  (JNIEnv *env, jclass clazz, jint workerCount) {
    try {
        lattice::net::AsyncCompressor::getInstance().setWorkerCount(workerCount);
    } catch (...) {
        // 忽略异常
    }
}

// 获取建议的压缩级别
JNIEXPORT jint JNICALL Java_io_lattice_network_NativeCompression_nativeSuggestCompressionLevel
  (JNIEnv *env, jclass clazz, jlong bytesSent, jdouble rttMs, jint playerCount, jdouble cpuUsage,
   jint baseLevel, jint minLevel, jint maxLevel) {
    try {
        lattice::net::DynamicCompression::Stats stats;
        stats.bytesSent = bytesSent;
        stats.rttMs = rttMs;
        stats.playerCount = playerCount;
        stats.cpuUsage = cpuUsage;
        
        return lattice::net::DynamicCompression::suggestLevel(stats, baseLevel, minLevel, maxLevel);
    } catch (...) {
        // 发生异常时返回基础级别
        return baseLevel;
    }
}

// 获取建议的工作线程数
JNIEXPORT jint JNICALL Java_io_lattice_network_NativeCompression_nativeSuggestWorkerCount
  (JNIEnv *env, jclass clazz) {
    try {
        return lattice::net::DynamicCompression::suggestWorkerCount();
    } catch (...) {
        // 发生异常时返回默认值
        return 4;
    }
}