#include "async_compression_jni.hpp"
#include "../../core/net/async_compressor.hpp"
#include "../jni_helper.hpp"
#include <vector>
#include <memory>
#include <functional>

// 全局引用用于回调
static jclass callbackClass = nullptr;
static jmethodID callbackMethod = nullptr;
static JavaVM* jvm = nullptr;

extern "C" {

// JNI_OnLoad函数，在库加载时调用
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    jvm = vm;
    return JNI_VERSION_1_8;
}

// 初始化异步压缩器
JNIEXPORT void JNICALL Java_io_lattice_network_AsyncCompression_initializeAsyncCompressor
  (JNIEnv *env, jclass clazz, jint workerCount) {
    try {
        // 获取回调类和方法的引用
        if (callbackClass == nullptr) {
            jclass localCallbackClass = env->FindClass("io/lattice/network/AsyncCompression$CompressionCallback");
            if (localCallbackClass != nullptr) {
                callbackClass = (jclass)env->NewGlobalRef(localCallbackClass);
                callbackMethod = env->GetMethodID(callbackClass, "onComplete", "(ZI)V");
                env->DeleteLocalRef(localCallbackClass);
            }
        }
        
        // 初始化异步压缩器
        lattice::net::AsyncCompressor::getInstance().setWorkerCount(workerCount);
    } catch (const std::exception& e) {
        JniHelper::throwJavaException(env, "java/lang/RuntimeException", e.what());
    }
}

// 设置工作线程数
JNIEXPORT void JNICALL Java_io_lattice_network_AsyncCompression_setWorkerCount
  (JNIEnv *env, jclass clazz, jint count) {
    try {
        lattice::net::AsyncCompressor::getInstance().setWorkerCount(count);
    } catch (const std::exception& e) {
        JniHelper::throwJavaException(env, "java/lang/RuntimeException", e.what());
    }
}

// 异步压缩
JNIEXPORT void JNICALL Java_io_lattice_network_AsyncCompression_compressAsync
  (JNIEnv *env, jclass clazz, jobject inputBuffer, jint inputLength, 
   jobject outputBuffer, jint outputCapacity, jobject callback) {
    try {
        // 获取直接缓冲区地址
        void* inputPtr = env->GetDirectBufferAddress(inputBuffer);
        void* outputPtr = env->GetDirectBufferAddress(outputBuffer);
        
        if (!inputPtr || !outputPtr) {
            JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "Buffers must be direct");
            return;
        }
        
        // 创建输入数据的共享指针
        auto inputData = std::make_shared<std::vector<char>>(static_cast<char*>(inputPtr), static_cast<char*>(inputPtr) + inputLength);
        auto outputData = std::make_shared<std::vector<char>>(outputCapacity);
        
        // 创建全局引用以在回调中使用
        jobject globalCallback = env->NewGlobalRef(callback);
        
        // 提交异步压缩任务
        lattice::net::AsyncCompressor::getInstance().compressAsync(
            inputData,
            outputData,
            6, // 默认压缩级别，实际应该从配置中获取
            [globalCallback, outputData](bool success, size_t outputSize) {
                // 在工作线程中执行回调
                
                // 获取JNIEnv
                JNIEnv* callbackEnv;
                bool needDetach = false;
                
                if (jvm->GetEnv((void**)&callbackEnv, JNI_VERSION_1_8) == JNI_EDETACHED) {
                    if (jvm->AttachCurrentThread((void**)&callbackEnv, nullptr) == JNI_OK) {
                        needDetach = true;
                    } else {
                        // 无法附加到线程，直接返回
                        return;
                    }
                } else if (callbackEnv == nullptr) {
                    // 无法获取JNIEnv，直接返回
                    return;
                }
                
                // 调用Java回调
                if (callbackClass != nullptr && callbackMethod != nullptr) {
                    callbackEnv->CallVoidMethod(globalCallback, callbackMethod, success, static_cast<jint>(outputSize));
                    
                    // 检查是否有异常
                    if (callbackEnv->ExceptionCheck()) {
                        callbackEnv->ExceptionClear();
                    }
                }
                
                // 删除全局引用
                callbackEnv->DeleteGlobalRef(globalCallback);
                
                // 如果需要，从JVM分离线程
                if (needDetach) {
                    jvm->DetachCurrentThread();
                }
            }
        );
    } catch (const std::exception& e) {
        JniHelper::throwJavaException(env, "java/lang/RuntimeException", e.what());
    }
}

// 动态压缩级别建议
JNIEXPORT jint JNICALL Java_io_lattice_network_DynamicCompression_suggestCompressionLevel
  (JNIEnv *env, jclass clazz, jint baseLevel, jdouble cpuUsage, jdouble rttMs, 
   jint playerCount, jint minLevel, jint maxLevel) {
    try {
        lattice::net::DynamicCompression::Stats stats;
        stats.bytesSent = 0; // 这个值在Java层计算
        stats.cpuUsage = cpuUsage;
        stats.rttMs = rttMs;
        stats.playerCount = playerCount;
        
        return lattice::net::DynamicCompression::suggestLevel(stats, baseLevel, minLevel, maxLevel);
    } catch (const std::exception& e) {
        JniHelper::throwJavaException(env, "java/lang/RuntimeException", e.what());
        return baseLevel;
    }
}

}