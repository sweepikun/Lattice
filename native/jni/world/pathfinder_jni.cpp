#include "pathfinder_jni.hpp"
#include "../../core/world/pathfinder.hpp"
#include <iostream>
#include <vector>
#include <string>

// 缓存常用的类和方法ID以提高性能
static jclass path_node_class = nullptr;
static jmethodID path_node_constructor = nullptr;
static jclass mob_params_class = nullptr;
static jfieldID step_height_field = nullptr;
static jfieldID max_drop_height_field = nullptr;
static jfieldID can_swim_field = nullptr;
static jfieldID can_fly_field = nullptr;
static jfieldID avoids_water_field = nullptr;
static jfieldID avoids_sun_field = nullptr;
static jfieldID speed_factor_field = nullptr;

// 日志输出宏
#define LOG_TAG "PathfinderJNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/**
 * 初始化JNI方法ID缓存
 */
void initialize_pathfinder_method_ids(JNIEnv* env) {
    if (path_node_class != nullptr) {
        return; // 已经初始化
    }
    
    // 获取PathNode类
    jclass local_path_node_class = env->FindClass("io/lattice/world/PathNode");
    if (local_path_node_class == nullptr) {
        LOGE("无法找到PathNode类");
        return;
    }
    
    // 创建全局引用
    path_node_class = (jclass)env->NewGlobalRef(local_path_node_class);
    env->DeleteLocalRef(local_path_node_class);
    if (path_node_class == nullptr) {
        LOGE("无法创建PathNode类的全局引用");
        return;
    }
    
    // 获取构造函数
    path_node_constructor = env->GetMethodID(path_node_class, "<init>", "(III)V");
    if (path_node_constructor == nullptr) {
        LOGE("无法找到PathNode构造函数");
        return;
    }
    
    // 获取MobPathfindingParams类
    jclass local_mob_params_class = env->FindClass("io/lattice/world/MobPathfindingParams");
    if (local_mob_params_class == nullptr) {
        LOGE("无法找到MobPathfindingParams类");
        return;
    }
    
    mob_params_class = (jclass)env->NewGlobalRef(local_mob_params_class);
    env->DeleteLocalRef(local_mob_params_class);
    if (mob_params_class == nullptr) {
        LOGE("无法创建MobPathfindingParams类的全局引用");
        return;
    }
    
    // 获取字段ID
    step_height_field = env->GetFieldID(mob_params_class, "stepHeight", "F");
    max_drop_height_field = env->GetFieldID(mob_params_class, "maxDropHeight", "F");
    can_swim_field = env->GetFieldID(mob_params_class, "canSwim", "Z");
    can_fly_field = env->GetFieldID(mob_params_class, "canFly", "Z");
    avoids_water_field = env->GetFieldID(mob_params_class, "avoidsWater", "Z");
    avoids_sun_field = env->GetFieldID(mob_params_class, "avoidsSun", "Z");
    speed_factor_field = env->GetFieldID(mob_params_class, "speedFactor", "F");
    
    // 检查字段ID是否有效
    if (!step_height_field || !max_drop_height_field || !can_swim_field || 
        !can_fly_field || !avoids_water_field || !avoids_sun_field || !speed_factor_field) {
        LOGE("无法找到MobPathfindingParams类的字段");
        return;
    }
}

/**
 * JNI_OnLoad - 初始化函数
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    return JNI_VERSION_1_8;
}

/**
 * JNI_OnUnload - 清理函数
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_8) != JNI_OK) {
        return;
    }
    
    // 删除全局引用
    if (path_node_class != nullptr) {
        env->DeleteGlobalRef(path_node_class);
        path_node_class = nullptr;
    }
    
    if (mob_params_class != nullptr) {
        env->DeleteGlobalRef(mob_params_class);
        mob_params_class = nullptr;
    }
}

/**
 * 优化寻路计算
 */
JNIEXPORT jobjectArray JNICALL Java_io_lattice_world_NativePathfinder_nativeOptimizePathfinding
  (JNIEnv *env, jclass clazz, jint start_x, jint start_y, jint start_z, 
   jint end_x, jint end_y, jint end_z, jint mob_type) {
    
    // 初始化方法ID（如果尚未初始化）
    initialize_pathfinder_method_ids(env);
    
    try {
        // 转换生物类型
        lattice::world::MobType mobType = static_cast<lattice::world::MobType>(mob_type);
        
        // 使用Lattice原生寻路优化器计算路径
        auto path = lattice::world::PathfinderOptimizer::get_instance().optimize_pathfinding(
            start_x, start_y, start_z, end_x, end_y, end_z, mobType);
        
        if (path.empty()) {
            LOGI("未找到路径");
            return nullptr; // 未找到路径
        }
        
        // 创建Java PathNode数组
        jsize arrayLength = static_cast<jsize>(path.size());
        jobjectArray pathArray = env->NewObjectArray(arrayLength, path_node_class, nullptr);
        if (pathArray == nullptr) {
            LOGE("无法分配PathNode数组");
            return nullptr; // 内存分配失败
        }
        
        // 填充数组
        for (jsize i = 0; i < arrayLength; ++i) {
            const auto& node = path[i];
            jobject pathNode = env->NewObject(path_node_class, path_node_constructor, 
                                            node->x, node->y, node->z);
            if (pathNode != nullptr) {
                env->SetObjectArrayElement(pathArray, i, pathNode);
                env->DeleteLocalRef(pathNode);
            }
        }
        
        return pathArray;
    } catch (const std::exception& e) {
        LOGE("发生C++异常: %s", e.what());
        // 抛出Java异常
        env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
        return nullptr;
    } catch (...) {
        LOGE("发生未知C++异常");
        // 抛出Java异常
        env->ThrowNew(env->FindClass("java/lang/Exception"), "Unknown C++ exception");
        return nullptr;
    }
}

/**
 * 获取生物寻路参数
 */
JNIEXPORT jobject JNICALL Java_io_lattice_world_NativePathfinder_nativeGetMobPathfindingParams
  (JNIEnv *env, jclass clazz, jint mob_type) {
    
    // 初始化方法ID（如果尚未初始化）
    initialize_pathfinder_method_ids(env);
    
    try {
        // 转换生物类型
        lattice::world::MobType mobType = static_cast<lattice::world::MobType>(mob_type);
        
        // 获取生物寻路参数
        const auto& params = lattice::world::PathfinderOptimizer::get_instance().get_mob_params(mobType);
        
        // 创建MobPathfindingParams对象
        jobject mobParams = env->AllocObject(mob_params_class);
        if (mobParams == nullptr) {
            LOGE("无法分配MobPathfindingParams对象");
            return nullptr; // 内存分配失败
        }
        
        // 设置字段值
        env->SetFloatField(mobParams, step_height_field, params.step_height);
        env->SetFloatField(mobParams, max_drop_height_field, params.max_drop_height);
        env->SetBooleanField(mobParams, can_swim_field, params.can_swim);
        env->SetBooleanField(mobParams, can_fly_field, params.can_fly);
        env->SetBooleanField(mobParams, avoids_water_field, params.avoids_water);
        env->SetBooleanField(mobParams, avoids_sun_field, params.avoids_sun);
        env->SetFloatField(mobParams, speed_factor_field, params.speed_factor);
        
        return mobParams;
    } catch (const std::exception& e) {
        LOGE("发生C++异常: %s", e.what());
        // 抛出Java异常
        env->ThrowNew(env->FindClass("java/lang/Exception"), e.what());
        return nullptr;
    } catch (...) {
        LOGE("发生未知C++异常");
        // 抛出Java异常
        env->ThrowNew(env->FindClass("java/lang/Exception"), "Unknown C++ exception");
        return nullptr;
    }
}