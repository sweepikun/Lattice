#pragma once

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <variant>
#include <cmath>
#include <cstdint>
#include "nlohmann/json.hpp"

namespace lattice::entity {

// ====== 实体状态和数据结构 ======

/**
 * @brief 实体行为数据 - 从minecraft-data加载的静态配置
 */
struct EntityBehaviorData {
    std::string id;
    std::string name;
    std::string category;
    std::string entityType; // 生物类型（如"living"、"block"等）
    
    // 基础属性
    float maxHealth = 20.0f;
    float attackDamage = 0.0f;
    float followRange = 16.0f;
    float pathfindingRange = 32.0f;
    float movementSpeed = 0.3f;
    
    // 物理属性
    float width = 0.6f;
    float height = 1.8f;
    float flyingSpeed = 0.4f;
    
    // 环境属性
    bool fireImmune = false;
    bool fireproof = false;
    
    // 行为标志
    bool canBreakDoors = false;
    bool burnsInSunlight = false;
    bool avoidsWater = false;
    bool canFly = false;
    bool canSwim = false;
    bool canDig = false;
    bool isAquatic = false;
    bool isNocturnal = false;
    bool isDiurnal = false;
    
    // 攻击行为
    bool canAttack = false;
    bool canAttackPlayers = false;
    bool canAttackMobs = false;
    bool canRangedAttack = false;
    bool canExplode = false;
    bool canTeleport = false;
    bool canBeTamed = false;
    bool canClimbWalls = false;
    bool canOpenDoors = false;
    bool canPassDoors = false;
    bool isFeline = false;
    bool isCanine = false;
    bool isMountable = false;
    float attackCooldown = 1.0f;
    float attackInterval = 0.8f;
    float rangedAttackRange = 16.0f;
    float explosionRadius = 3.0f;
    
    // 寻路参数
    float jumpHeight = 0.8f;
    bool canJump = true;
    // 移除重复的成员变量定义
    
    // 环境适应性
    std::string environmentType; // "land", "water", "air", "shadow"
    std::unordered_map<std::string, float> blockPreferences; // 方块类型 -> 偏好值
    std::unordered_map<std::string, float> biomePreferences;  // 生物群系 -> 偏好值
    std::unordered_map<std::string, float> lightPreferences;  // 光照强度 -> 偏好值
    
    // 装备槽位
    std::vector<std::string> equipmentSlots;
    
    // 目标优先级
    std::unordered_map<std::string, float> targetPriorities; // 实体类型 -> 优先级
};

/**
 * @brief 动态实体状态
 */
struct EntityState {
    uint64_t entityId;
    float x, y, z;
    float rotationYaw, rotationPitch;
    float velocityX, velocityY, velocityZ;
    float movementSpeed = 0.3f; // 默认移动速度
    float health, maxHealth;
    bool isAlive;
    bool isInWater, isInLava, isOnFire;
    float lightLevel;
    std::string currentBlockType;
    std::string currentBiome;
    uint64_t lastAttackTime;
    uint64_t targetId;
    float targetDistance;
    std::vector<uint64_t> threatEntities;
    
    // 行为状态
    enum class BehaviorState {
        IDLE, FOLLOWING, ATTACKING, FLEEING, EXPLORING, RESTING
    };
    BehaviorState behaviorState = BehaviorState::IDLE;
    
    // 传统比较运算符（C++17兼容）
    bool operator==(const EntityState& other) const {
        return entityId == other.entityId &&
               std::abs(x - other.x) < 0.001f &&
               std::abs(y - other.y) < 0.001f &&
               std::abs(z - other.z) < 0.001f &&
               std::abs(rotationYaw - other.rotationYaw) < 0.001f &&
               std::abs(rotationPitch - other.rotationPitch) < 0.001f &&
               health == other.health &&
               isAlive == other.isAlive;
    }
    
    bool operator!=(const EntityState& other) const {
        return !(*this == other);
    }
};

/**
 * @brief 世界视图 - 实体可感知的环境信息
 */
struct WorldView {
    struct BlockInfo {
        std::string type;
        bool solid;
        bool liquid;
        bool transparent;
        float hardness;
        std::string tool;
    };
    
    std::unordered_map<std::string, BlockInfo> nearbyBlocks;
    std::unordered_map<uint64_t, EntityState> nearbyEntities;
    float lightGrid[16][256][16]; // 本地区块光照
    std::string biomeGrid[16][16];
    uint64_t timestamp;
};

// ====== C++17 约束检查 ======

/**
 * @brief AI组件约束检查（使用SFINAE代替C++20 concepts）
 */
template<typename T>
struct IsAIComponent {
private:
    template<typename U>
    static auto test(int) -> decltype(
        std::declval<U>().update(std::declval<const EntityState&>(), std::declval<const WorldView&>()) == bool{},
        std::declval<U>().getPriority() == float{},
        std::declval<U>().getName() == std::string{},
        std::declval<U>().reset(),
        std::true_type{}
    );
    
    template<typename>
    static std::false_type test(...);
    
public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template<typename T>
inline constexpr bool IsAIComponent_v = IsAIComponent<T>::value;

/**
 * @brief 行为节点约束检查
 */
template<typename T>
struct IsBehaviorNode {
private:
    template<typename U>
    static auto test(int) -> decltype(
        std::declval<U>().tick(std::declval<EntityState&>(), std::declval<const WorldView&>()) == bool{},
        std::declval<U>().reset(),
        std::declval<U>().getNodeType() == std::string{},
        std::true_type{}
    );
    
    template<typename>
    static std::false_type test(...);
    
public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template<typename T>
inline constexpr bool IsBehaviorNode_v = IsBehaviorNode<T>::value;

// ====== 行为节点基类 ======

/**
 * @brief 行为树节点基类
 */
class BehaviorNodeBase {
public:
    virtual ~BehaviorNodeBase() = default;
    
    virtual bool tick(EntityState& state, const WorldView& world) = 0;
    virtual void reset() = 0;
    virtual std::string getNodeType() const = 0;
    virtual float getPriority() const { return 1.0f; }
    
    // 实用工具方法（public）
    static inline float clamp(float value, float min, float max) {
        return std::max(min, std::min(max, value));
    }
    
    static inline float distance3D(float x1, float y1, float z1, float x2, float y2, float z2) {
        float dx = x2 - x1;
        float dy = y2 - y1;
        float dz = z2 - z1;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    
protected:
};

// ====== 版本策略模式 ======

/**
 * @brief 版本策略接口 - 支持不同Minecraft版本的实体行为差异
 */
class VersionStrategy {
public:
    virtual ~VersionStrategy() = default;
    virtual std::string getVersion() const = 0;
    virtual void applyBehaviorModifications(EntityBehaviorData& data) const = 0;
    virtual float getBehaviorMultiplier(const std::string& behavior) const = 0;
};

/**
 * @brief Minecraft 1.20.4 版本策略
 */
class Version1_20_4 : public VersionStrategy {
public:
    std::string getVersion() const override { return "1.20.4"; }
    
    void applyBehaviorModifications(EntityBehaviorData& data) const override {
        // 1.20.4 特殊行为调整
        if (data.id == "minecraft:warden") {
            data.attackCooldown = 1.5f; // 监守者攻击间隔延长
        } else if (data.id == "minecraft:allay") {
            data.followRange = 8.0f; // 悦灵跟随范围缩小
        }
    }
    
    float getBehaviorMultiplier(const std::string& behavior) const override {
        if (behavior == "warden_attack") return 1.0f;
        if (behavior == "allay_follow") return 0.8f;
        if (behavior == "axolotl_swim") return 1.2f;
        return 1.0f;
    }
};

/**
 * @brief Minecraft 1.19.4 版本策略
 */
class Version1_19_4 : public VersionStrategy {
public:
    std::string getVersion() const override { return "1.19.4"; }
    
    void applyBehaviorModifications(EntityBehaviorData& data) const override {
        // 1.19.4 行为调整
        if (data.id == "minecraft:axolotl") {
            data.canSwim = true;
            data.isAquatic = true;
        }
    }
    
    float getBehaviorMultiplier(const std::string& behavior) const override {
        if (behavior == "axolotl_regeneration") return 1.5f;
        return 1.0f;
    }
};

// ====== 工厂模式 ======

/**
 * @brief 策略工厂 - 创建版本特定的行为策略
 */
class StrategyFactory {
public:
    static std::unique_ptr<VersionStrategy> createStrategy(const std::string& version) {
        if (version == "1.20.4" || version == "1.20.3" || version == "1.20.2") {
            return std::make_unique<Version1_20_4>();
        } else if (version == "1.19.4" || version == "1.19.3" || version == "1.19.2") {
            return std::make_unique<Version1_19_4>();
        }
        
        // 默认使用最新版本
        return std::make_unique<Version1_20_4>();
    }
};

// ====== 数据加载器 ======

/**
 * @brief 数据模块 - 从minecraft-data加载实体配置
 */
class DataModule {
public:
    DataModule(const std::string& minecraftDataPath = "", bool useGitHub = false);
    ~DataModule() = default;
    
    // 加载实体行为数据
    bool loadEntityData();
    bool loadEntityDataForVersion(const std::string& version);
    
    // 获取实体数据
    EntityBehaviorData* getEntityData(const std::string& entityId);
    std::vector<std::string> getAllEntityIds() const;
    std::vector<EntityBehaviorData> getEntitiesByCategory(const std::string& category) const;
    
    // 数据验证
    bool validateEntityData(const EntityBehaviorData& data) const;
    
private:
    std::string minecraftDataPath_;
    bool useGitHub_;
    std::unordered_map<std::string, EntityBehaviorData> entityData_;
    std::unordered_map<std::string, std::string> entityDataByName_; // 按名称索引
    std::unordered_map<std::string, std::vector<std::string>> categoryIndex_;
    mutable std::shared_mutex dataMutex_;
    
    // 解析minecraft-data JSON文件
    bool parseEntityJSON(const std::string& filePath);
    
    // 使用nlohmann::json进行完整JSON解析
    EntityBehaviorData parseEntityFromJson(const nlohmann::json& entityJson);
    
    // 向后兼容的简化字符串解析（已废弃）
    EntityBehaviorData parseEntityObject(const std::string& jsonStr);
    
    std::string extractJsonField(const std::string& json, const std::string& field, const std::string& defaultValue);
    std::string mapCategory(const std::string& minecraftCategory);
    std::string determineCategoryFromId(const std::string& id);
    float getHealthFromId(const std::string& id);
    float getAttackDamageFromId(const std::string& id);
    void setupEntitySpecificProperties(EntityBehaviorData& data);
    void populateCategoryIndex();
};

// ====== AI引擎主类 ======

/**
 * @brief AI引擎主类 - 管理所有实体的AI行为
 */
class AIEngine {
public:
    AIEngine();
    ~AIEngine() = default;
    
    // 初始化和配置
    bool initialize(const std::string& minecraftDataPath, const std::string& gameVersion, bool useGitHub = false);
    void shutdown();
    
    // 实体管理
    bool registerEntity(uint64_t entityId, const std::string& entityType);
    void unregisterEntity(uint64_t entityId);
    void updateEntityState(uint64_t entityId, const EntityState& state);
    
    // 主循环
    void tick();
    void tickEntity(uint64_t entityId, uint64_t deltaTime);
    
    // 世界视图更新
    void updateWorldView(uint64_t entityId, const WorldView& world);
    
    // 配置获取
    EntityBehaviorData* getEntityConfig(uint64_t entityId);
    
    // 数据获取辅助方法
    std::vector<std::string> getAllEntityIds() const;
    std::vector<EntityBehaviorData> getEntitiesByCategory(const std::string& category) const;
    
    // 性能监控
    struct PerformanceStats {
        uint64_t totalTicks{0};
        uint64_t avgTickTime{0};
        uint64_t maxTickTime{0};
        uint64_t entitiesProcessed{0};
        std::atomic<uint64_t> memoryUsage{0};
    };
    
    const PerformanceStats& getPerformanceStats() const {
        return performanceStats_;
    }
    
    void resetPerformanceStats() {
        performanceStats_.totalTicks = 0;
        performanceStats_.avgTickTime = 0;
        performanceStats_.maxTickTime = 0;
        performanceStats_.entitiesProcessed = 0;
        performanceStats_.memoryUsage = 0;
    }
    
private:
    std::unique_ptr<DataModule> dataModule_;
    std::unique_ptr<VersionStrategy> versionStrategy_;
    
    // 实体状态管理
    std::unordered_map<uint64_t, EntityState> entityStates_;
    std::unordered_map<uint64_t, std::string> entityTypes_;
    std::unordered_map<uint64_t, std::unique_ptr<WorldView>> entityWorldViews_;
    
    // 并发控制
    mutable std::shared_mutex entityMutex_;
    
    // 性能统计
    PerformanceStats performanceStats_;
    mutable std::mutex statsMutex_;
    
    // 内部方法
    void processEntityBehavior(uint64_t entityId, const EntityState& state, const WorldView& world);
    void updateWorldViewInternal(uint64_t entityId, const WorldView& world);
};

// ====== 线程局部存储管理 ======

/**
 * @brief 线程局部AI上下文
 */
struct ThreadLocalContext {
    EntityState* currentEntity{nullptr};
    const WorldView* currentWorld{nullptr};
    uint64_t threadId{0};
    std::chrono::high_resolution_clock::time_point tickStart;
};

inline thread_local ThreadLocalContext g_threadContext;

// ====== 异常安全包装器 ======

/**
 * @brief 异常安全的AI操作包装器
 */
template<typename Func>
auto safeExecute(Func&& func) -> decltype(func()) {
    try {
        return std::forward<Func>(func)();
    } catch (const std::exception& e) {
        // 记录错误但不崩溃
        fprintf(stderr, "AI Engine Error: %s\n", e.what());
        throw; // 重新抛出，让上层决定如何处理
    } catch (...) {
        fprintf(stderr, "AI Engine Error: Unknown exception\n");
        throw;
    }
}

} // namespace lattice::entity