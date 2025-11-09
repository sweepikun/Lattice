#include <jni.h>
#include "../../core/world/light_updater.hpp"
#include <memory>
#include <unordered_map>
#include "light_updater_jni.hpp"

// 线程本地存储的光照更新器实例
thread_local std::unique_ptr<lattice::world::LightUpdater> tl_lightUpdater = nullptr;

// 全局引用缓存
JavaVM* g_jvm = nullptr;
jobject g_worldObject = nullptr;
jclass g_blockPosClass = nullptr;
jmethodID g_blockPosConstructor = nullptr;
jfieldID g_blockPosXField = nullptr;
jfieldID g_blockPosYField = nullptr;
jfieldID g_blockPosZField = nullptr;
jclass g_worldClass = nullptr;
jmethodID g_getBlockStateMethod = nullptr;
jclass g_blockStateClass = nullptr;
jmethodID g_getBlockMethod = nullptr;
jclass g_blockClass = nullptr;
jfieldID g_blockMaterialField = nullptr;
jclass g_materialClass = nullptr;
jmethodID g_isTransparentMethod = nullptr;

extern "C" {

// JNI_OnLoad函数，在库加载时调用
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    
    // 缓存BlockPos类和字段ID
    jclass blockPosClass = env->FindClass("net/minecraft/core/BlockPos");
    if (blockPosClass != nullptr) {
        g_blockPosClass = static_cast<jclass>(env->NewGlobalRef(blockPosClass));
        g_blockPosConstructor = env->GetMethodID(blockPosClass, "<init>", "(III)V");
        g_blockPosXField = env->GetFieldID(blockPosClass, "x", "I");
        g_blockPosYField = env->GetFieldID(blockPosClass, "y", "I");
        g_blockPosZField = env->GetFieldID(blockPosClass, "z", "I");
    }
    
    // 缓存World类和方法ID
    jclass worldClass = env->FindClass("net/minecraft/world/level/BlockGetter");
    if (worldClass != nullptr) {
        g_worldClass = static_cast<jclass>(env->NewGlobalRef(worldClass));
        g_getBlockStateMethod = env->GetMethodID(worldClass, "getBlockState", "(Lnet/minecraft/core/BlockPos;)Lnet/minecraft/world/level/block/state/BlockState;");
    }
    
    // 缓存BlockState类和方法ID
    jclass blockStateClass = env->FindClass("net/minecraft/world/level/block/state/BlockState");
    if (blockStateClass != nullptr) {
        g_blockStateClass = static_cast<jclass>(env->NewGlobalRef(blockStateClass));
        g_getBlockMethod = env->GetMethodID(blockStateClass, "getBlock", "()Lnet/minecraft/world/level/block/Block;");
    }
    
    // 缓存Block类和字段ID
    jclass blockClass = env->FindClass("net/minecraft/world/level/block/Block");
    if (blockClass != nullptr) {
        g_blockClass = static_cast<jclass>(env->NewGlobalRef(blockClass));
        g_blockMaterialField = env->GetFieldID(blockClass, "material", "Lnet/minecraft/world/level/material/Material;");
    }
    
    // 缓存Material类和方法ID
    jclass materialClass = env->FindClass("net/minecraft/world/level/material/Material");
    if (materialClass != nullptr) {
        g_materialClass = static_cast<jclass>(env->NewGlobalRef(materialClass));
        g_isTransparentMethod = env->GetMethodID(materialClass, "isTransparent", "()Z");
    }
    
    return JNI_VERSION_1_6;
}

// JNI_OnUnload函数，在库卸载时调用
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return;
    }
    
    // 删除全局引用
    if (g_worldObject != nullptr) {
        env->DeleteGlobalRef(g_worldObject);
        g_worldObject = nullptr;
    }
    
    if (g_blockPosClass != nullptr) {
        env->DeleteGlobalRef(g_blockPosClass);
        g_blockPosClass = nullptr;
    }
    
    if (g_worldClass != nullptr) {
        env->DeleteGlobalRef(g_worldClass);
        g_worldClass = nullptr;
    }
    
    if (g_blockStateClass != nullptr) {
        env->DeleteGlobalRef(g_blockStateClass);
        g_blockStateClass = nullptr;
    }
    
    if (g_blockClass != nullptr) {
        env->DeleteGlobalRef(g_blockClass);
        g_blockClass = nullptr;
    }
    
    if (g_materialClass != nullptr) {
        env->DeleteGlobalRef(g_materialClass);
        g_materialClass = nullptr;
    }
    
    g_jvm = nullptr;
}

// 设置世界对象引用
JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_setWorldReference
  (JNIEnv* env, jclass clazz, jobject world) {
    // 删除旧的世界对象引用（如果存在）
    if (g_worldObject != nullptr) {
        env->DeleteGlobalRef(g_worldObject);
        g_worldObject = nullptr;
    }
    
    // 创建新的全局引用
    if (world != nullptr) {
        g_worldObject = env->NewGlobalRef(world);
    }
}

// 初始化线程本地光照更新器
JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_initLightUpdater
  (JNIEnv* env, jclass clazz) {
    if (tl_lightUpdater) {
        tl_lightUpdater.reset();
    }
    tl_lightUpdater = std::make_unique<lattice::world::LightUpdater>();
}

// 释放线程本地光照更新器
JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_releaseLightUpdater
  (JNIEnv* env, jclass clazz) {
    if (tl_lightUpdater) {
        tl_lightUpdater.reset();
    }
}

// 将Java BlockPos转换为C++ BlockPos
lattice::world::BlockPos convertBlockPos(JNIEnv* env, jobject javaPos) {
    if (javaPos == nullptr || g_blockPosClass == nullptr) {
        return lattice::world::BlockPos(0, 0, 0);
    }
    
    jint x = env->GetIntField(javaPos, g_blockPosXField);
    jint y = env->GetIntField(javaPos, g_blockPosYField);
    jint z = env->GetIntField(javaPos, g_blockPosZField);
    
    return lattice::world::BlockPos(x, y, z);
}

// 添加光照源
JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_addLightSource
  (JNIEnv* env, jclass clazz, jobject pos, jint level, jboolean isSkyLight) {
    if (!tl_lightUpdater) {
        tl_lightUpdater = std::make_unique<lattice::world::LightUpdater>();
    }
    
    lattice::world::BlockPos cppPos = convertBlockPos(env, pos);
    lattice::world::LightType type = isSkyLight ? 
        lattice::world::LightType::SKY : lattice::world::LightType::BLOCK;
    
    tl_lightUpdater->addLightSource(cppPos, static_cast<uint8_t>(level), type);
}

// 移除光照源
JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_removeLightSource
  (JNIEnv* env, jclass clazz, jobject pos, jboolean isSkyLight) {
    if (!tl_lightUpdater) {
        tl_lightUpdater = std::make_unique<lattice::world::LightUpdater>();
    }
    
    lattice::world::BlockPos cppPos = convertBlockPos(env, pos);
    lattice::world::LightType type = isSkyLight ? 
        lattice::world::LightType::SKY : lattice::world::LightType::BLOCK;
    
    tl_lightUpdater->removeLightSource(cppPos, type);
}

// 执行光照更新
JNIEXPORT void JNICALL Java_io_lattice_world_NativeLightUpdater_propagateLightUpdates
  (JNIEnv* env, jclass clazz) {
    if (!tl_lightUpdater) {
        tl_lightUpdater = std::make_unique<lattice::world::LightUpdater>();
    }
    
    tl_lightUpdater->propagateLightUpdates();
}

// 检查是否有待处理的更新
JNIEXPORT jboolean JNICALL Java_io_lattice_world_NativeLightUpdater_hasUpdates
  (JNIEnv* env, jclass clazz) {
    if (!tl_lightUpdater) {
        return JNI_FALSE;
    }
    
    return tl_lightUpdater->hasUpdates() ? JNI_TRUE : JNI_FALSE;
}

// 获取指定位置的光照级别
JNIEXPORT jint JNICALL Java_io_lattice_world_NativeLightUpdater_getLightLevel
  (JNIEnv* env, jclass clazz, jobject pos, jboolean isSkyLight) {
    if (!tl_lightUpdater) {
        tl_lightUpdater = std::make_unique<lattice::world::LightUpdater>();
    }
    
    lattice::world::BlockPos cppPos = convertBlockPos(env, pos);
    lattice::world::LightType type = isSkyLight ? 
        lattice::world::LightType::SKY : lattice::world::LightType::BLOCK;
    
    uint8_t level = tl_lightUpdater->getLightLevel(cppPos, type);
    return static_cast<jint>(level);
}

}