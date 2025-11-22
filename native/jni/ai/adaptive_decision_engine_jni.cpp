#include <jni.h>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <atomic>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include "../jni_helper.hpp"
#include "../ai/adaptive_decision_engine.hpp"
#include "nlohmann/json.hpp"

// Entity state and world context structures
struct EntityStateJNI {
    std::string entityType;
    float health;
    float maxHealth;
    float x, y, z;
    float vx, vy, vz;
    int worldId;
    long long timestamp;
    std::vector<std::string> effects;
    bool isBaby;
    bool isInWater;
    bool isInLava;
    bool isOnFire;
    int dimension;
};

struct WorldContextJNI {
    int chunkX, chunkZ;
    long long worldTime;
    int weather;
    float difficulty;
    int playerCount;
    float redstonePower;
    std::vector<std::string> nearbyEntities;
    std::string biomeType;
    bool isNight;
    bool isStorm;
};

// Global decision engine instances
static std::atomic<jlong> gDecisionEngineCounter(0);
static std::unordered_map<jlong, std::unique_ptr<lattice::ai::AdaptiveDecisionEngine>> gDecisionEngineInstances;
static std::mutex gDecisionEngineMutex;

extern "C" {

// ========================================
// AdaptiveDecisionEngine JNI Bridge
// ========================================

JNIEXPORT jlong JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeCreateEngine
(JNIEnv* env, jclass clazz) {
    try {
        auto engine = std::make_unique<lattice::ai::AdaptiveDecisionEngine>();
        
        jlong engineId = ++gDecisionEngineCounter;
        
        std::lock_guard<std::mutex> lock(gDecisionEngineMutex);
        gDecisionEngineInstances[engineId] = std::move(engine);
        
        return engineId;
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return 0;
    }
}

JNIEXPORT void JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeDestroyEngine
(JNIEnv* env, jclass clazz, jlong engineId) {
    try {
        std::lock_guard<std::mutex> lock(gDecisionEngineMutex);
        gDecisionEngineInstances.erase(engineId);
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
    }
}

JNIEXPORT jboolean JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeLoadConfigFromJson
(JNIEnv* env, jclass clazz, jlong engineId, jstring jsonConfig) {
    try {
        std::string configStr = jni::jstring_to_string(env, jsonConfig);
        
        // Parse JSON configuration
        nlohmann::json config;
        try {
            config = nlohmann::json::parse(configStr);
        } catch (const nlohmann::json::parse_error& e) {
            jni::throw_java_exception(env, "java/lang/IllegalArgumentException", 
                                    ("Invalid JSON configuration: " + std::string(e.what())).c_str());
            return JNI_FALSE;
        }
        
        std::shared_lock<std::mutex> lock(gDecisionEngineMutex);
        auto it = gDecisionEngineInstances.find(engineId);
        if (it == gDecisionEngineInstances.end()) {
            jni::throw_java_exception(env, "java/lang/IllegalArgumentException", "Invalid engine ID");
            return JNI_FALSE;
        }
        
        auto& engine = it->second;
        engine->loadFromJson(config);
        
        return JNI_TRUE;
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return JNI_FALSE;
    }
}

JNIEXPORT jboolean JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeLoadConfigFromFile
(JNIEnv* env, jclass clazz, jlong engineId, jstring configFilePath) {
    try {
        std::string filePath = jni::jstring_to_string(env, configFilePath);
        
        // Read configuration file
        std::ifstream configFile(filePath);
        if (!configFile.is_open()) {
            jni::throw_java_exception(env, "java/io/FileNotFoundException", 
                                    ("Configuration file not found: " + filePath).c_str());
            return JNI_FALSE;
        }
        
        nlohmann::json config;
        configFile >> config;
        configFile.close();
        
        std::shared_lock<std::mutex> lock(gDecisionEngineMutex);
        auto it = gDecisionEngineInstances.find(engineId);
        if (it == gDecisionEngineInstances.end()) {
            jni::throw_java_exception(env, "java/lang/IllegalArgumentException", "Invalid engine ID");
            return JNI_FALSE;
        }
        
        auto& engine = it->second;
        engine->loadFromJson(config);
        
        return JNI_TRUE;
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return JNI_FALSE;
    }
}

JNIEXPORT jlong JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeMakeDecision
(JNIEnv* env, jclass clazz, jlong engineId, jstring entityType, jfloat health, jfloat x, jfloat y, jfloat z,
 jint chunkX, jint chunkZ, jint worldId, jlong worldTime, jint playerCount, jboolean isNight) {
    try {
        std::string entityTypeStr = jni::jstring_to_string(env, entityType);
        
        // Create entity state
        lattice::ai::EntityState entityState;
        entityState.entityType = entityTypeStr;
        entityState.health = health;
        entityState.position = {x, y, z};
        entityState.timestamp = worldTime;
        
        // Create world context
        lattice::ai::WorldContext worldContext;
        worldContext.chunkX = chunkX;
        worldContext.chunkZ = chunkZ;
        worldContext.worldTime = worldTime;
        worldContext.playerCount = playerCount;
        worldContext.isNight = isNight;
        
        std::shared_lock<std::mutex> lock(gDecisionEngineMutex);
        auto it = gDecisionEngineInstances.find(engineId);
        if (it == gDecisionEngineInstances.end()) {
            jni::throw_java_exception(env, "java/lang/IllegalArgumentException", "Invalid engine ID");
            return 0;
        }
        
        auto& engine = it->second;
        auto decisionResult = engine->decide(entityState, worldContext);
        
        // Return decision result as a unique ID
        // This would need proper encoding of the decision result
        static std::atomic<jlong> gDecisionCounter(10000);
        jlong decisionId = ++gDecisionCounter;
        
        return decisionId;
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return 0;
    }
}

JNIEXPORT jstring JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeGetDecisionAction
(JNIEnv* env, jclass clazz, jlong decisionId) {
    try {
        // This would retrieve the actual decision action from the decision result
        // For now, return a placeholder action
        std::string action = "idle";
        return jni::string_to_jstring(env, action);
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return nullptr;
    }
}

JNIEXPORT jfloat JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeGetDecisionPriority
(JNIEnv* env, jclass clazz, jlong decisionId) {
    try {
        // This would retrieve the decision priority
        // For now, return default priority
        return 0.5f;
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return 0.0f;
    }
}

JNIEXPORT jboolean JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeUpdateEntityBehavior
(JNIEnv* env, jclass clazz, jlong engineId, jstring entityType, jstring behaviorJson) {
    try {
        std::string entityTypeStr = jni::jstring_to_string(env, entityType);
        std::string behaviorJsonStr = jni::jstring_to_string(env, behaviorJson);
        
        // Parse behavior JSON
        nlohmann::json behaviorConfig;
        try {
            behaviorConfig = nlohmann::json::parse(behaviorJsonStr);
        } catch (const nlohmann::json::parse_error& e) {
            jni::throw_java_exception(env, "java/lang/IllegalArgumentException", 
                                    ("Invalid behavior JSON: " + std::string(e.what())).c_str());
            return JNI_FALSE;
        }
        
        std::shared_lock<std::mutex> lock(gDecisionEngineMutex);
        auto it = gDecisionEngineInstances.find(engineId);
        if (it == gDecisionEngineInstances.end()) {
            jni::throw_java_exception(env, "java/lang/IllegalArgumentException", "Invalid engine ID");
            return JNI_FALSE;
        }
        
        auto& engine = it->second;
        
        // This would update the entity behavior configuration
        // For now, just return true (placeholder)
        return JNI_TRUE;
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return JNI_FALSE;
    }
}

JNIEXPORT jstring JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeGetSupportedEntities
(JNIEnv* env, jclass clazz, jlong engineId) {
    try {
        std::shared_lock<std::mutex> lock(gDecisionEngineMutex);
        auto it = gDecisionEngineInstances.find(engineId);
        if (it == gDecisionEngineInstances.end()) {
            jni::throw_java_exception(env, "java/lang/IllegalArgumentException", "Invalid engine ID");
            return nullptr;
        }
        
        // This would return the list of supported entities as JSON
        nlohmann::json entities;
        entities["supported_entities"] = {
            "minecraft:warden",
            "minecraft:breeze",
            "minecraft:bogged",
            "minecraft:bog",
            "minecraft:allay",
            "minecraft:axolotl",
            "minecraft:cat",
            "minecraft:cod",
            "minecraft:cow",
            "minecraft:creeper",
            "minecraft:dolphin",
            "minecraft:donkey",
            "minecraft:drowned",
            "minecraft:elder_guardian",
            "minecraft:ender_dragon",
            "minecraft:enderman",
            "minecraft:endermite",
            "minecraft:evoker",
            "minecraft:fox",
            "minecraft:glow_squid",
            "minecraft:goat",
            "minecraft:guardian",
            "minecraft:hoglin",
            "minecraft:horse",
            "minecraft:husk",
            "minecraft:iron_golem",
            "minecraft:llama",
            "minecraft:magma_cube",
            "minecraft:mooshroom",
            "minecraft:mule",
            "minecraft:ocelot",
            "minecraft:panda",
            "minecraft:parrot",
            "minecraft:phantom",
            "minecraft:pig",
            "minecraft:piglin",
            "minecraft:piglin_brute",
            "minecraft:pillager",
            "minecraft:player",
            "minecraft:polar_bear",
            "minecraft:pufferfish",
            "minecraft:rabbit",
            "minecraft:ravager",
            "minecraft:salmon",
            "minecraft:sheep",
            "minecraft:shulker",
            "minecraft:silverfish",
            "minecraft:skeleton",
            "minecraft:skeleton_horse",
            "minecraft:slime",
            "minecraft:snow_golem",
            "minecraft:spider",
            "minecraft:squid",
            "minecraft:stray",
            "minecraft:strider",
            "minecraft:trader_llama",
            "minecraft:tropical_fish",
            "minecraft:turtle",
            "minecraft:vex",
            "minecraft:villager",
            "minecraft:vindicator",
            "minecraft:wandering_trader",
            "minecraft:witch",
            "minecraft:wither",
            "minecraft:wither_skeleton",
            "minecraft:wolf",
            "minecraft:zoglin",
            "minecraft:zombie",
            "minecraft:zombie_horse",
            "minecraft:zombie_villager",
            "minecraft:zombified_piglin"
        };
        
        std::string entitiesStr = entities.dump();
        return jni::string_to_jstring(env, entitiesStr);
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return nullptr;
    }
}

JNIEXPORT jboolean JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeReloadConfig
(JNIEnv* env, jclass clazz, jlong engineId) {
    try {
        std::shared_lock<std::mutex> lock(gDecisionEngineMutex);
        auto it = gDecisionEngineInstances.find(engineId);
        if (it == gDecisionEngineInstances.end()) {
            jni::throw_java_exception(env, "java/lang/IllegalArgumentException", "Invalid engine ID");
            return JNI_FALSE;
        }
        
        auto& engine = it->second;
        
        // This would reload the configuration from the last loaded source
        // For now, just return true (placeholder)
        return JNI_TRUE;
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return JNI_FALSE;
    }
}

JNIEXPORT jboolean JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeClearDecisionHistory
(JNIEnv* env, jclass clazz, jlong engineId) {
    try {
        std::shared_lock<std::mutex> lock(gDecisionEngineMutex);
        auto it = gDecisionEngineInstances.find(engineId);
        if (it == gDecisionEngineInstances.end()) {
            jni::throw_java_exception(env, "java/lang/IllegalArgumentException", "Invalid engine ID");
            return JNI_FALSE;
        }
        
        // This would clear the decision history
        // For now, just return true (placeholder)
        return JNI_TRUE;
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return JNI_FALSE;
    }
}

// ========================================
// Entity Type Intelligence JNI Bridge
// ========================================

JNIEXPORT jboolean JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeIsValidEntityType
(JNIEnv* env, jclass clazz, jlong engineId, jstring entityType) {
    try {
        std::string entityTypeStr = jni::jstring_to_string(env, entityType);
        
        // Check if entity type is in the supported entities list
        static const std::unordered_set<std::string> supportedEntities = {
            "minecraft:warden", "minecraft:breeze", "minecraft:bogged", "minecraft:bog",
            "minecraft:allay", "minecraft:axolotl", "minecraft:cat", "minecraft:cow",
            "minecraft:creeper", "minecraft:player", "minecraft:zombie", "minecraft:skeleton"
            // ... (full list of supported entities)
        };
        
        return supportedEntities.count(entityTypeStr) > 0 ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return JNI_FALSE;
    }
}

JNIEXPORT jstring JNICALL Java_io_lattice_ai_AdaptiveDecisionEngine_nativeInferEntityCategory
(JNIEnv* env, jclass clazz, jlong engineId, jstring entityType) {
    try {
        std::string entityTypeStr = jni::jstring_to_string(env, entityType);
        
        // Simple entity type to category mapping
        std::string category;
        if (entityTypeStr.find("hostile") != std::string::npos || 
            entityTypeStr.find("zombie") != std::string::npos ||
            entityTypeStr.find("skeleton") != std::string::npos ||
            entityTypeStr.find("creeper") != std::string::npos) {
            category = "hostile";
        } else if (entityTypeStr.find("passive") != std::string::npos ||
                   entityTypeStr.find("cow") != std::string::npos ||
                   entityTypeStr.find("pig") != std::string::npos ||
                   entityTypeStr.find("sheep") != std::string::npos) {
            category = "passive";
        } else if (entityTypeStr.find("neutral") != std::string::npos ||
                   entityTypeStr.find("wolf") != std::string::npos ||
                   entityTypeStr.find("cat") != std::string::npos) {
            category = "neutral";
        } else if (entityTypeStr.find("boss") != std::string::npos ||
                   entityTypeStr.find("dragon") != std::string::npos ||
                   entityTypeStr.find("wither") != std::string::npos) {
            category = "boss";
        } else {
            category = "unknown";
        }
        
        return jni::string_to_jstring(env, category);
    } catch (const std::exception& e) {
        jni::throw_java_exception(env, "java/lang/RuntimeException", e.what());
        return nullptr;
    }
}

} // extern "C"