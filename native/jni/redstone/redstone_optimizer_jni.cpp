#include "redstone_optimizer_jni.hpp"
#include "../../core/redstone/redstone_optimizer.hpp"
#include <iostream>
#include <jni.h>
#include <stdexcept>
#include <mutex>

// 用于保存JavaVM实例
static JavaVM* g_vm = nullptr;
static std::mutex g_vm_mutex;

/**
 * JNI_OnLoad - 初始化函数
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    std::lock_guard<std::mutex> lock(g_vm_mutex);
    g_vm = vm;
    
    // 初始化JNI环境
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        return JNI_ERR;
    }
    
    return JNI_VERSION_1_8;
}

/**
 * JNI_OnUnload - 清理函数
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    std::lock_guard<std::mutex> lock(g_vm_mutex);
    // 清理全局引用和资源
    g_vm = nullptr;
}

/**
 * 获取红石功率
 */
JNIEXPORT jint JNICALL Java_io_lattice_redstone_NativeRedstoneOptimizer_nativeGetRedstonePower
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z) {
    if (!env) {
        return 0;
    }
    
    try {
        // 添加参数验证
        if (env->ExceptionCheck()) {
            return 0;
        }
        
        return lattice::redstone::RedstoneOptimizer::get_instance().get_redstone_power(x, y, z);
    } catch (const std::exception& ex) {
        // 记录异常信息
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return 0;
    } catch (...) {
        // 未知异常
        env->ThrowNew(env->FindClass("java/lang/Exception"), "Unknown exception in native code");
        return 0;
    }
}

/**
 * 计算网络功率
 */
JNIEXPORT jint JNICALL Java_io_lattice_redstone_NativeRedstoneOptimizer_nativeCalculateNetworkPower
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z, jint max_distance) {
    if (!env) {
        return 0;
    }
    
    try {
        // 添加参数验证
        if (env->ExceptionCheck()) {
            return 0;
        }
        
        // 确保max_distance为非负值
        int clamped_distance = max_distance < 0 ? 0 : max_distance;
        
        return lattice::redstone::RedstoneOptimizer::get_instance().calculate_network_power(x, y, z, clamped_distance);
    } catch (const std::exception& ex) {
        // 记录异常信息
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
        return 0;
    } catch (...) {
        // 未知异常
        env->ThrowNew(env->FindClass("java/lang/Exception"), "Unknown exception in native code");
        return 0;
    }
}

/**
 * 使网络缓存失效
 */
JNIEXPORT void JNICALL Java_io_lattice_redstone_NativeRedstoneOptimizer_nativeInvalidateNetworkCache
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z) {
    if (!env || env->ExceptionCheck()) {
        return;
    }
    
    try {
        lattice::redstone::RedstoneOptimizer::get_instance().invalidate_network_cache(x, y, z);
    } catch (const std::exception& ex) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), ex.what());
    } catch (...) {
        env->ThrowNew(env->FindClass("java/lang/Exception"), "Unknown exception in native code");
    }
}

/**
 * 检查指定坐标是否为充能方块
 */
JNIEXPORT jboolean JNICALL Java_io_lattice_redstone_NativeRedstoneOptimizer_nativeIsPoweredBlock
  (JNIEnv *env, jclass clazz, jint x, jint y, jint z) {
    try {
        return lattice::redstone::RedstoneOptimizer::get_instance().is_powered_block(x, y, z) ? JNI_TRUE : JNI_FALSE;
    } catch (...) {
        // 发生异常时返回false
        return JNI_FALSE;
    }
}