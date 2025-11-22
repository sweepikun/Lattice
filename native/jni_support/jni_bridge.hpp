#pragma once
#include <jni.h>
#include <cstdint>

namespace lattice {
namespace jni {

// 简化的JNI包装器
struct JNIBridge {
    static void initialize() {
        // 初始化JNI环境
    }
    
    static jclass findClass(JNIEnv* env, const char* name) {
        return env->FindClass(name);
    }
    
    static jmethodID getMethodID(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
        return env->GetMethodID(clazz, name, sig);
    }
    
    static jfieldID getFieldID(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
        return env->GetFieldID(clazz, name, sig);
    }
};

} // namespace jni
} // namespace lattice
