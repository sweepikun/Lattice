#include "biological_ai.hpp"
#include "behavior_nodes.hpp"
#include <jni.h>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
#include <format>
#ifdef USE_CUSTOM_FMT
#include "fmt_wrapper.hpp"
#else
#include <fmt/chrono.h>
#endif

// ====== JNI全局变量 ======

namespace lattice {
namespace entity {
namespace jni {

// 全局AI引擎实例
static std::unique_ptr<AIEngine> g_aiEngine;
static std::string g_minecraftDataPath;
static std::string g_gameVersion;

// 线程局部存储
thread_local static jclass g_entityAIConfigClass = nullptr;
thread_local static jmethodID g_entityAIConfigConstructor = nullptr;

// 获取当前AI引擎实例
static AIEngine* getAIEngine() {
    if (!g_aiEngine) {
        std::cerr << "[Lattice AI] AI Engine not initialized!" << std::endl;
        return nullptr;
    }
    return g_aiEngine.get();
}

// ====== JNI辅助函数 ======

/**
 * @brief 异常处理包装器
 */
template<typename Func>
jobject executeWithExceptionHandling(JNIEnv* env, Func&& func) {
    try {
        return std::forward<Func>(func)();
    } catch (const std::exception& e) {
        fmt::print(stderr, "JNI Exception: {}\n", e.what());
        
        // 抛出Java异常
        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        if (exceptionClass) {
            env->ThrowNew(exceptionClass, e.what());
        }
        return nullptr;
    } catch (...) {
        fmt::print(stderr, "JNI Exception: Unknown error\n");
        
        jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
        if (exceptionClass) {
            env->ThrowNew(exceptionClass, "Unknown native error");
        }
        return nullptr;
    }
}

/**
 * @brief 字符串转换工具
 */
std::string jstringToString(JNIEnv* env, jstring jstr) {
    if (!jstr) return "";
    const char* str = env->GetStringUTFChars(jstr, nullptr);
    std::string result(str);
    env->ReleaseStringUTFChars(jstr, str);
    return result;
}

jstring stringToJstring(JNIEnv* env, const std::string& str) {
    return env->NewStringUTF(str.c_str());
}

/**
 * @brief 实体状态转换工具
 */
EntityState javaToEntityState(JNIEnv* env, jobject javaState) {
    EntityState state{};
    
    // 获取Java EntityState类
    jclass stateClass = env->GetObjectClass(javaState);
    
    // 获取各个字段
    jfieldID idField = env->GetFieldID(stateClass, "entityId", "J");
    jfieldID xField = env->GetFieldID(stateClass, "x", "F");
    jfieldID yField = env->GetFieldID(stateClass, "y", "F");
    jfieldID zField = env->GetFieldID(stateClass, "z", "F");
    jfieldID yawField = env->GetFieldID(stateClass, "rotationYaw", "F");
    jfieldID pitchField = env->GetFieldID(stateClass, "rotationPitch", "F");
    jfieldID velXField = env->GetFieldID(stateClass, "velocityX", "F");
    jfieldID velYField = env->GetFieldID(stateClass, "velocityY", "F");
    jfieldID velZField = env->GetFieldID(stateClass, "velocityZ", "F");
    jfieldID healthField = env->GetFieldID(stateClass, "health", "F");
    jfieldID maxHealthField = env->GetFieldID(stateClass, "maxHealth", "F");
    jfieldID aliveField = env->GetFieldID(stateClass, "isAlive", "Z");
    jfieldID inWaterField = env->GetFieldID(stateClass, "isInWater", "Z");
    jfieldID inLavaField = env->GetFieldID(stateClass, "isInLava", "Z");
    jfieldID onFireField = env->GetFieldID(stateClass, "isOnFire", "Z");
    jfieldID lightLevelField = env->GetFieldID(stateClass, "lightLevel", "F");
    
    // 读取值
    state.entityId = env->GetLongField(javaState, idField);
    state.x = env->GetFloatField(javaState, xField);
    state.y = env->GetFloatField(javaState, yField);
    state.z = env->GetFloatField(javaState, zField);
    state.rotationYaw = env->GetFloatField(javaState, yawField);
    state.rotationPitch = env->GetFloatField(javaState, pitchField);
    state.velocityX = env->GetFloatField(javaState, velXField);
    state.velocityY = env->GetFloatField(javaState, velYField);
    state.velocityZ = env->GetFloatField(javaState, velZField);
    state.health = env->GetFloatField(javaState, healthField);
    state.maxHealth = env->GetFloatField(javaState, maxHealthField);
    state.isAlive = env->GetBooleanField(javaState, aliveField);
    state.isInWater = env->GetBooleanField(javaState, inWaterField);
    state.isInLava = env->GetBooleanField(javaState, inLavaField);
    state.isOnFire = env->GetBooleanField(javaState, onFireField);
    state.lightLevel = env->GetFloatField(javaState, lightLevelField);
    
    return state;
}

jobject entityStateToJava(JNIEnv* env, const EntityState& state) {
    // 这里应该创建Java EntityState对象
    // 简化实现，返回null
    return nullptr;
}

/**
 * @brief 世界视图转换工具
 */
WorldView javaToWorldView(JNIEnv* env, jobject javaWorld) {
    WorldView world{};
    // 简化实现，实际需要转换Java WorldView对象
    world.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
    return world;
}

} // namespace lattice::jni

// ====== JNI函数实现 ======

using namespace lattice::entity;
using namespace lattice::jni;

/**
 * JNI版本：Java_com_lattice_ai_AIEngine_initialize
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_lattice_ai_AIEngine_initializeNative(
    JNIEnv* env, jclass clazz, jstring minecraftDataPath, jstring gameVersion) 
{
    return executeWithExceptionHandling(env, [&]() -> jboolean {
        std::string dataPath = jstringToString(env, minecraftDataPath);
        std::string version = jstringToString(env, gameVersion);
        
        // 全局AI引擎实例（应该使用单例模式）
        static std::unique_ptr<AIEngine> aiEngine;
        if (!aiEngine) {
            aiEngine = std::make_unique<AIEngine>();
        }
        
        bool success = aiEngine->initialize(dataPath, version);
        if (success) {
            fmt::print("AI Engine initialized successfully\n");
        } else {
            fmt::print(stderr, "AI Engine initialization failed\n");
        }
        
        return success ? JNI_TRUE : JNI_FALSE;
    });
}

/**
 * JNI版本：Java_com_lattice_ai_AIEngine_registerEntity
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_com_lattice_ai_AIEngine_registerEntityNative(
    JNIEnv* env, jclass clazz, jlong entityId, jstring entityType) 
{
    return executeWithExceptionHandling(env, [&]() -> jboolean {
        // 获取AI引擎实例
        static std::unique_ptr<AIEngine> aiEngine = std::make_unique<AIEngine>();
        
        std::string type = jstringToString(env, entityType);
        bool success = aiEngine->registerEntity(entityId, type);
        
        return success ? JNI_TRUE : JNI_FALSE;
    });
}

/**
 * JNI版本：Java_com_lattice_ai_AIEngine_unregisterEntity
 */
extern "C" JNIEXPORT void JNICALL
Java_com_lattice_ai_AIEngine_unregisterEntityNative(
    JNIEnv* env, jclass clazz, jlong entityId) 
{
    executeWithExceptionHandling(env, [&]() {
        static std::unique_ptr<AIEngine> aiEngine = std::make_unique<AIEngine>();
        aiEngine->unregisterEntity(entityId);
    });
}

/**
 * JNI版本：Java_com_lattice_ai_AIEngine_updateEntityState
 */
extern "C" JNIEXPORT void JNICALL
Java_com_lattice_ai_AIEngine_updateEntityStateNative(
    JNIEnv* env, jclass clazz, jlong entityId, jobject javaState) 
{
    executeWithExceptionHandling(env, [&]() {
        static std::unique_ptr<AIEngine> aiEngine = std::make_unique<AIEngine>();
        
        EntityState state = javaToEntityState(env, javaState);
        aiEngine->updateEntityState(entityId, state);
    });
}

/**
 * JNI版本：Java_com_lattice_ai_AIEngine_updateWorldView
 */
extern "C" JNIEXPORT void JNICALL
Java_com_lattice_ai_AIEngine_updateWorldViewNative(
    JNIEnv* env, jclass clazz, jlong entityId, jobject javaWorld) 
{
    executeWithExceptionHandling(env, [&]() {
        static std::unique_ptr<AIEngine> aiEngine = std::make_unique<AIEngine>();
        
        WorldView world = javaToWorldView(env, javaWorld);
        aiEngine->updateWorldView(entityId, world);
    });
}

/**
 * JNI版本：Java_com_lattice_ai_AIEngine_tick
 */
extern "C" JNIEXPORT void JNICALL
Java_com_lattice_ai_AIEngine_tickNative(
    JNIEnv* env, jclass clazz, jlong deltaTime) 
{
    executeWithExceptionHandling(env, [&]() {
        static std::unique_ptr<AIEngine> aiEngine = std::make_unique<AIEngine>();
        aiEngine->tick();
    });
}

/**
 * JNI版本：Java_com_lattice_ai_AIEngine_getPerformanceStats
 */
extern "C" JNIEXPORT jobject JNICALL
Java_com_lattice_ai_AIEngine_getPerformanceStatsNative(
    JNIEnv* env, jclass clazz) 
{
    return executeWithExceptionHandling(env, [&]() -> jobject {
        static std::unique_ptr<AIEngine> aiEngine = std::make_unique<AIEngine>();
        auto stats = aiEngine->getPerformanceStats();
        
        // 创建Java PerformanceStats对象
        jclass statsClass = env->FindClass("com/lattice/ai/PerformanceStats");
        if (!statsClass) return nullptr;
        
        jmethodID constructor = env->GetMethodID(statsClass, "<init>", 
                                                "(JJJJJ)V");
        if (!constructor) return nullptr;
        
        return env->NewObject(statsClass, constructor,
                             static_cast<jlong>(stats.totalTicks),
                             static_cast<jlong>(stats.avgTickTime),
                             static_cast<jlong>(stats.maxTickTime),
                             static_cast<jlong>(stats.entitiesProcessed),
                             static_cast<jlong>(stats.memoryUsage.load()));
    });
}

/**
 * JNI版本：Java_com_lattice_ai_AIEngine_resetPerformanceStats
 */
extern "C" JNIEXPORT void JNICALL
Java_com_lattice_ai_AIEngine_resetPerformanceStatsNative(
    JNIEnv* env, jclass clazz) 
{
    executeWithExceptionHandling(env, [&]() {
        static std::unique_ptr<AIEngine> aiEngine = std::make_unique<AIEngine>();
        aiEngine->resetPerformanceStats();
    });
}

/**
 * JNI版本：Java_com_lattice_ai_AIEngine_shutdown
 */
extern "C" JNIEXPORT void JNICALL
Java_com_lattice_ai_AIEngine_shutdownNative(
    JNIEnv* env, jclass clazz) 
{
    executeWithExceptionHandling(env, [&]() {
        static std::unique_ptr<AIEngine> aiEngine = std::make_unique<AIEngine>();
        aiEngine->shutdown();
        fmt::print("AI Engine shutdown\n");
    });
}

// ====== 行为节点JNI接口 ======

/**
 * JNI版本：Java_com_lattice_ai_BehaviorNodes_createLookAtPlayerNode
 */
extern "C" JNIEXPORT jlong JNICALL
Java_com_lattice_ai_BehaviorNodes_createLookAtPlayerNode(
    JNIEnv* env, jclass clazz) 
{
    return executeWithExceptionHandling(env, [&]() -> jlong {
        auto* node = new LookAtPlayerNode();
        return reinterpret_cast<jlong>(node);
    });
}

/**
 * JNI版本：Java_com_lattice_ai_BehaviorNodes_createMeleeAttackNode
 */
extern "C" JNIEXPORT jlong JNICALL
Java_com_lattice_ai_BehaviorNodes_createMeleeAttackNode(
    JNIEnv* env, jclass clazz, jfloat attackRange, jfloat attackDamage) 
{
    return executeWithExceptionHandling(env, [&]() -> jlong {
        auto* node = new MeleeAttackNode(attackRange, attackDamage);
        return reinterpret_cast<jlong>(node);
    });
}

/**
 * JNI版本：Java_com_lattice_ai_BehaviorNodes_createAvoidEntityNode
 */
extern "C" JNIEXPORT jlong JNICALL
Java_com_lattice_ai_BehaviorNodes_createAvoidEntityNode(
    JNIEnv* env, jclass clazz, jobjectArray threatTypes) 
{
    return executeWithExceptionHandling(env, [&]() -> jlong {
        std::unordered_set<std::string> threats;
        
        if (threatTypes) {
            jsize length = env->GetArrayLength(threatTypes);
            for (jsize i = 0; i < length; i++) {
                jstring threatType = static_cast<jstring>(
                    env->GetObjectArrayElement(threatTypes, i));
                if (threatType) {
                    threats.insert(jstringToString(env, threatType));
                    env->DeleteLocalRef(threatType);
                }
            }
        }
        
        auto* node = new AvoidEntityNode(threats);
        return reinterpret_cast<jlong>(node);
    });
}

/**
 * JNI版本：Java_com_lattice_ai_BehaviorNodes_createWaterNavigationNode
 */
extern "C" JNIEXPORT jlong JNICALL
Java_com_lattice_ai_BehaviorNodes_createWaterNavigationNode(
    JNIEnv* env, jclass clazz, jfloat swimSpeed) 
{
    return executeWithExceptionHandling(env, [&]() -> jlong {
        auto* node = new WaterNavigationNode(swimSpeed);
        return reinterpret_cast<jlong>(node);
    });
}

/**
 * JNI版本：Java_com_lattice_ai_BehaviorNodes_createFlyingBehaviorNode
 */
extern "C" JNIEXPORT jlong JNICALL
Java_com_lattice_ai_BehaviorNodes_createFlyingBehaviorNode(
    JNIEnv* env, jclass clazz, jfloat flightSpeed, jfloat maxAltitude) 
{
    return executeWithExceptionHandling(env, [&]() -> jlong {
        auto* node = new FlyingBehaviorNode(flightSpeed, maxAltitude);
        return reinterpret_cast<jlong>(node);
    });
}

/**
 * JNI版本：Java_com_lattice_ai_BehaviorNodes_deleteNode
 */
extern "C" JNIEXPORT void JNICALL
Java_com_lattice_ai_BehaviorNodes_deleteNode(
    JNIEnv* env, jclass clazz, jlong nodePtr) 
{
    executeWithExceptionHandling(env, [&]() {
        if (nodePtr) {
            auto* node = reinterpret_cast<BehaviorNodeBase*>(nodePtr);
            delete node;
        }
    });
}

// ====== JNI加载和卸载 ======

/**
 * JNI_OnLoad - 当JNI库被加载时调用
 */
extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    fmt::print("Loading Lattice AI Native Library\n");
    
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK) {
        return JNI_ERR;
    }
    
    // 初始化线程局部存储
    initializeThreadLocalContext();
    
    fmt::print("Lattice AI Native Library loaded successfully\n");
    return JNI_VERSION_1_8;
}

/**
 * JNI_OnUnload - 当JNI库被卸载时调用
 */
extern "C" JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM* vm, void* reserved) {
    fmt::print("Unloading Lattice AI Native Library\n");
    
    // 清理资源
    static std::unique_ptr<AIEngine> aiEngine;
    if (aiEngine) {
        aiEngine->shutdown();
        aiEngine.reset();
    }
    
    fmt::print("Lattice AI Native Library unloaded\n");
}

// ====== 错误处理和调试 ======

/**
 * 设置本地调试模式
 */
extern "C" JNIEXPORT void JNICALL
Java_com_lattice_ai_AIEngine_setDebugMode(
    JNIEnv* env, jclass clazz, jboolean enabled) 
{
    static std::atomic<bool> debugMode{false};
    debugMode = enabled;
    
    fmt::print("AI Engine debug mode {}\n", enabled ? "enabled" : "disabled");
}

/**
 * 获取Native库版本信息
 */
extern "C" JNIEXPORT jstring JNICALL
Java_com_lattice_ai_AIEngine_getNativeVersion(
    JNIEnv* env, jclass clazz) 
{
    std::string version = "Lattice AI Native Library v1.0.0";
    return stringToJstring(env, version);
}

/**
 * 执行内存压力测试
 */
extern "C" JNIEXPORT jlong JNICALL
Java_com_lattice_ai_AIEngine_executeMemoryStressTest(
    JNIEnv* env, jclass clazz, jint entityCount) 
{
    return executeWithExceptionHandling(env, [&]() -> jlong {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 创建指定数量的实体用于测试
        static std::unique_ptr<AIEngine> aiEngine = std::make_unique<AIEngine>();
        
        for (int i = 0; i < entityCount; i++) {
            std::string entityType = "minecraft:cow";
            aiEngine->registerEntity(i, entityType);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        fmt::print("Memory stress test completed: {} entities in {} μs\n", 
                  entityCount, duration.count());
        
        return static_cast<jlong>(duration.count());
    });
}