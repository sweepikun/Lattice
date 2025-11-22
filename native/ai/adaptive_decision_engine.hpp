#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <optional>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "cache/hierarchical_cache_system.hpp"

namespace lattice {
namespace ai {

// ===========================================
// 1. 实体行为状态定义
// ===========================================
namespace EntityState {
    enum class BehaviorState : uint8_t {
        IDLE,               // 空闲状态
        WALKING,            // 行走
        RUNNING,            // 奔跑
        FLYING,             // 飞行
        SWIMMING,           // 游泳
        ATTACKING,          // 攻击
        DEFENDING,          // 防御
        FLEEING,            // 逃跑
        HUNTING,            // 狩猎
        GRAZING,            // 吃草
        PATROLLING,         // 巡逻
        HIDING,             // 隐藏
        ESCAPING_WATER,     // 逃离水域
        AERIAL_HUNTING,     // 空中狩猎
        AERIAL_PATROL,      // 空中巡逻
        SONIC_BOOM,         // 声波攻击(Warden)
        VIBRATION_DETECTION, // 振动检测
        BLAZING,            // 烈焰人燃烧状态
        DROWNING,           // 溺水状态
        BURNING,            // 燃烧状态
        FREEZING,           // 冰冻状态
        UNKNOWN             // 未知状态
    };
    
    enum class EntityType : uint8_t {
        HOSTILE,           // 敌对生物
        PASSIVE,           // 被动生物
        NEUTRAL,           // 中性生物
        ANIMAL,            // 动物
        BOSS,              // Boss
        VEHICLE,           // 载具
        PROJECTILE,        // 投射物
        MISC,              // 杂项
        FLYING,            // 飞行生物
        AQUATIC,           // 水生生物
        UNDEAD,            // 不死族
        ARTHROPOD,         // 节肢动物
        UNKNOWN            // 未知类型
    };
    
    enum class EnvironmentType : uint8_t {
        LAND,              // 陆地
        WATER,             // 水域
        AIR,               // 空中
        SHADOW,            // 黑暗环境
        NETHER,            // 下界
        END,               // 末地
        DEEP_DARK,         // 深暗群系
        OVERWORLD,         // 主世界
        UNDERGROUND,       // 地下
        UNDERWATER,        // 水下
        EXTREME_HOT,       // 极端炎热
        EXTREME_COLD,      // 极端寒冷
        UNKNOWN            // 未知环境
    };
}

// ===========================================
// 2. 增强的实体行为数据结构
// ===========================================
struct EnhancedEntityBehaviorData {
    // 基础标识
    std::string id;
    std::string name;
    std::string namespace_id;
    
    // 分类信息
    EntityState::EntityType entity_type{EntityState::EntityType::UNKNOWN};
    std::string category;
    EnvironmentType environment_type{EnvironmentType::UNKNOWN};
    
    // 物理属性
    struct PhysicalProperties {
        float max_health{20.0f};
        float current_health{20.0f};
        float width{0.6f};
        float height{1.8f};
        float eye_height{1.62f};
        float hitbox_size{1.0f};
        float gravity{0.98f};
        float drag{0.91f};
    } physics;
    
    // 移动属性
    struct MovementProperties {
        float ground_speed{0.3f};
        float water_speed{0.2f};
        float flying_speed{0.4f};
        float sprint_multiplier{1.3f};
        bool can_walk{true};
        bool can_swim{true};
        bool can_fly{false};
        bool can_climb{false};
        bool can_jump{false};
        bool can_dig{false};
    } movement;
    
    // 攻击属性
    struct CombatProperties {
        bool can_attack{false};
        bool can_ranged_attack{false};
        float attack_damage{0.0f};
        float attack_interval{0.8f};
        float attack_range{3.0f};
        float ranged_attack_range{16.0f};
        float detection_range{16.0f};
        int max_attack_targets{1};
        bool can_destroy_blocks{false};
        std::vector<std::string> destroyable_blocks;
        bool can_open_doors{false};
        bool can_break_doors{false};
    } combat;
    
    // 生存属性
    struct SurvivalProperties {
        bool fire_immune{false};
        bool burns_in_sunlight{false};
        bool water_sensitive{false};
        bool avoids_water{false};
        bool is_nocturnal{false};
        bool is_aquatic{false};
        bool can_breathe_underwater{false};
        bool needs_breathing_air{true};
        bool freeze_damage{false};
        bool fire_damage{false};
    } survival;
    
    // 智能属性
    struct IntelligenceProperties {
        bool has_ai{true};
        bool can_leave_holder{true};
        bool can_despawn{true};
        bool has_collision{true};
        bool can_be_pushed{true};
        bool can_push_entities{true};
        float vision_range{16.0f};
        float hearing_range{16.0f};
        int memory_capacity{8};
        bool has_pack_ai{false};
        std::string pack_type;
    } intelligence;
    
    // 1.21.10特殊属性
    struct V1210Properties {
        // Warden (监守者) 特殊属性
        float sonic_boom_damage{15.0f};
        float vibration_detection_range{16.0f};
        bool can_sonic_boom{false};
        bool can_vibration_detect{false};
        
        // Breeze (微风) 特殊属性
        bool can_wind_charge{false};
        float wind_charge_damage{3.0f};
        bool can_phase_through_blocks{false};
        
        // Bogged (沼泽生物) 特殊属性
        bool can_swamp_terrain_adaptation{false};
        bool can_breathe_muddy_water{false};
        float mud_swim_speed{0.15f};
        
        // 试炼室相关
        bool trial_boss_variant{false};
        float trial_boss_health_multiplier{2.0f};
        bool has_trial_equipment{false};
        
        // 深暗群系适应
        bool deep_dark_native{false};
        bool light_preference_adaptation{false};
        bool vibration_based_navigation{false};
    } v1210;
    
    // 装备和物品
    struct EquipmentProperties {
        bool can_use_items{false};
        bool can_hold_items{false};
        bool can_wear_armor{false};
        std::vector<std::string> allowed_armor_types;
        std::vector<std::string> preferred_items;
        bool has_custom_drops{false};
        std::vector<std::string> drop_items;
        std::vector<float> drop_chances;
    } equipment;
    
    // 行为标志
    struct BehaviorFlags {
        bool can_be_tamed{false};
        bool can_breed{false};
        bool has_baby_variant{false};
        bool can_sit{false};
        bool can_follow_owner{false};
        bool can_guard_territory{false};
        bool can_be_fed{false};
        bool can_be_milked{false};
        bool can_shear{false};
        bool can_be_ridden{false};
        std::vector<std::string> taming_items;
        std::vector<std::string> breeding_items;
        std::string baby_texture_variant;
    } behavior;
    
    // 音量和声音
    struct SoundProperties {
        float volume{1.0f};
        float pitch{1.0f};
        bool make_ambient_sounds{false};
        bool make_footstep_sounds{true};
        std::string ambient_sound;
        std::string hurt_sound;
        std::string death_sound;
        std::string step_sound;
    } sound;
    
    // JSON数据源
    nlohmann::json raw_json_data;
    std::string version{"1.21.1"};
    std::string data_source;
    
    // 性能统计
    struct PerformanceStats {
        std::chrono::microseconds parse_time{0};
        std::chrono::microseconds decision_time{0};
        uint64_t decision_count{0};
        std::chrono::system_clock::time_point last_updated;
        double cache_hit_ratio{0.0};
    } stats;
    
    // 自动适配功能
    void autoAdaptFromJSON(const nlohmann::json& json);
    void validateAndFillDefaults();
    bool isValid() const;
};

// ===========================================
// 3. 世界视图和环境感知
// ===========================================
struct WorldView {
    struct BlockInfo {
        std::string id;
        int x, y, z;
        float light_level{1.0f};
        bool is_solid{false};
        bool is_liquid{false};
        bool is_opaque{false};
        std::unordered_map<std::string, float> properties;
    };
    
    struct EntityInfo {
        std::string id;
        float x, y, z;
        float health{20.0f};
        bool is_hostile{false};
        bool is_player{false};
        bool is_ally{false};
        float distance{0.0f};
    };
    
    struct EnvironmentInfo {
        float temperature{0.5f}; // 0.0 (寒冷) to 1.0 (炎热)
        float humidity{0.5f};     // 0.0 (干燥) to 1.0 (湿润)
        float light_level{1.0f};
        bool is_daytime{true};
        bool is_raining{false};
        EnvironmentType biome_type{EnvironmentType::OVERWORLD};
        int player_count{0};
    };
    
    std::vector<BlockInfo> nearby_blocks;
    std::vector<EntityInfo> nearby_entities;
    EnvironmentInfo environment;
    
    // 查询功能
    std::optional<BlockInfo> getBlockAt(int x, int y, int z) const;
    std::vector<EntityInfo> getEntitiesInRange(float x, float z, float range) const;
    float getLightLevel(int x, int y, int z) const;
    bool isInWater(int x, int y, int z) const;
    bool isSolidBlock(int x, int y, int z) const;
};

// ===========================================
// 4. 环境适应结果
// ===========================================
struct EnvironmentAdaptationResult {
    bool needs_adaptation{false};
    EntityState::BehaviorState suggested_behavior{EntityState::BehaviorState::IDLE};
    float confidence{0.0f};
    std::string reason;
    std::chrono::milliseconds adaptation_time{0};
};

// ===========================================
// 5. 版本适配策略基类
// ===========================================
class VersionStrategy {
public:
    virtual ~VersionStrategy() = default;
    virtual std::string get_version() const = 0;
    virtual void apply_behavior_modifications(EnhancedEntityBehaviorData& data) const = 0;
    virtual bool supports_entity(const std::string& entity_id) const = 0;
    virtual std::vector<std::string> get_supported_entities() const = 0;
};

// ===========================================
// 6. 1.21.0版本适配器
// ===========================================
class Version1210Adapter : public VersionStrategy {
public:
    std::string get_version() const override {
        return "1.21.0";
    }
    
    void apply_behavior_modifications(EnhancedEntityBehaviorData& data) const override;
    
    bool supports_entity(const std::string& entity_id) const override;
    
    std::vector<std::string> get_supported_entities() const override;
    
private:
    void handle_warden_special_behavior(EnhancedEntityBehaviorData& data) const;
    void handle_breeze_special_behavior(EnhancedEntityBehaviorData& data) const;
    void handle_bogged_special_behavior(EnhancedEntityBehaviorData& data) const;
    void handle_trial_chambers_enhancements(EnhancedEntityBehaviorData& data) const;
    void handle_deep_dark_adaptations(EnhancedEntityBehaviorData& data) const;
};

// ===========================================
// 7. JSON配置映射器
// ===========================================
class JsonConfigMapper {
public:
    void map_all_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    
    // 特定类型映射
    void map_physical_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    void map_movement_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    void map_combat_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    void map_survival_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    void map_intelligence_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    void map_equipment_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    void map_behavior_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    void map_sound_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    
    // 智能类型推断
    EntityState::EntityType determine_entity_type(const nlohmann::json& json) const;
    std::string map_to_category(const std::string& category) const;
    EnvironmentType map_environment_type(const nlohmann::json& json) const;
    
private:
    void parse_behavior_flags(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
    void parse_environment_preferences(const nlohmann::json& json, EnhancedEntityBehaviorData& data);
};

// ===========================================
// 8. 智能决策树
// ===========================================
class SmartDecisionTree {
public:
    explicit SmartDecisionTree(const std::shared_ptr<cache::HierarchicalCacheSystem>& cache_system);
    ~SmartDecisionTree() = default;
    
    EntityState::BehaviorState make_decision(const EntityState::BehaviorState& current_state,
                                            const WorldView& world,
                                            const EnhancedEntityBehaviorData& config);
    
    // 决策缓存
    void enable_decision_caching(bool enable);
    void clear_decision_cache();
    
    // 性能统计
    struct DecisionStats {
        uint64_t total_decisions{0};
        uint64_t cache_hits{0};
        uint64_t cache_misses{0};
        std::chrono::microseconds avg_decision_time{0};
        double cache_hit_ratio() const {
            return (total_decisions > 0) ? static_cast<double>(cache_hits) / total_decisions : 0.0;
        }
    };
    
    DecisionStats get_stats() const;
    void reset_stats();
    
private:
    std::shared_ptr<cache::HierarchicalCacheSystem> cache_system_;
    mutable std::mutex stats_mutex_;
    DecisionStats stats_;
    
    // 决策缓存
    std::unordered_map<std::string, EntityState::BehaviorState> decision_cache_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> cache_timestamps_;
    mutable std::shared_mutex cache_mutex_;
    bool caching_enabled_{true};
    
    // 决策逻辑
    EnvironmentAdaptationResult check_environment_adaptation(const WorldView& world,
                                                            const EnhancedEntityBehaviorData& config) const;
    
    bool has_nearby_threats(const WorldView& world, const EnhancedEntityBehaviorData& config) const;
    bool has_attack_targets(const WorldView& world, const EnhancedEntityBehaviorData& config) const;
    bool needs_food_or_rest(const WorldView& world, const EnhancedEntityBehaviorData& config) const;
    
    EntityState::BehaviorState handle_threat_response(const WorldView& world,
                                                     const EnhancedEntityBehaviorData& config) const;
    
    EntityState::BehaviorState handle_environmental_needs(const WorldView& world,
                                                         const EnhancedEntityBehaviorData& config) const;
    
    EntityState::BehaviorState handle_goal_driven_behavior(const WorldView& world,
                                                          const EnhancedEntityBehaviorData& config) const;
    
    EntityState::BehaviorState handle_special_behaviors(const WorldView& world,
                                                       const EnhancedEntityBehaviorData& config) const;
    
    // 缓存键生成
    std::string generate_decision_key(const WorldView& world,
                                     const EnhancedEntityBehaviorData& config,
                                     EntityState::BehaviorState current_state) const;
    
    // 1.21.10特殊行为处理
    EntityState::BehaviorState handle_warden_behaviors(const WorldView& world,
                                                      const EnhancedEntityBehaviorData& config) const;
    EntityState::BehaviorState handle_breeze_behaviors(const WorldView& world,
                                                      const EnhancedEntityBehaviorData& config) const;
    EntityState::BehaviorState handle_bogged_behaviors(const WorldView& world,
                                                      const EnhancedEntityBehaviorData& config) const;
};

// ===========================================
// 9. 决策插件系统
// ===========================================
class DecisionPlugin {
public:
    virtual ~DecisionPlugin() = default;
    virtual std::string get_plugin_name() const = 0;
    virtual bool can_handle(const EnhancedEntityBehaviorData& config) const = 0;
    virtual EntityState::BehaviorState make_decision(const WorldView& world,
                                                    const EnhancedEntityBehaviorData& config,
                                                    EntityState::BehaviorState current_state) const = 0;
    virtual bool is_thread_safe() const { return true; }
};

// 专用插件示例
class FlyingCreaturePlugin : public DecisionPlugin {
public:
    std::string get_plugin_name() const override {
        return "FlyingCreatures";
    }
    
    bool can_handle(const EnhancedEntityBehaviorData& config) const override {
        return config.movement.can_fly;
    }
    
    EntityState::BehaviorState make_decision(const WorldView& world,
                                           const EnhancedEntityBehaviorData& config,
                                           EntityState::BehaviorState current_state) const override;
    
private:
    bool has_high_altitude_targets(const WorldView& world, 
                                  const EnhancedEntityBehaviorData& config) const;
    bool needs_aerial_patrol(const WorldView& world,
                           const EnhancedEntityBehaviorData& config) const;
};

class AquaticCreaturePlugin : public DecisionPlugin {
public:
    std::string get_plugin_name() const override {
        return "AquaticCreatures";
    }
    
    bool can_handle(const EnhancedEntityBehaviorData& config) const override {
        return config.survival.is_aquatic;
    }
    
    EntityState::BehaviorState make_decision(const WorldView& world,
                                           const EnhancedEntityBehaviorData& config,
                                           EntityState::BehaviorState current_state) const override;
};

class BossCreaturePlugin : public DecisionPlugin {
public:
    std::string get_plugin_name() const override {
        return "BossCreatures";
    }
    
    bool can_handle(const EnhancedEntityBehaviorData& config) const override {
        return config.entity_type == EntityState::EntityType::BOSS;
    }
    
    EntityState::BehaviorState make_decision(const WorldView& world,
                                           const EnhancedEntityBehaviorData& config,
                                           EntityState::BehaviorState current_state) const override;
};

// ===========================================
// 10. 决策插件管理器
// ===========================================
class DecisionPluginManager {
public:
    DecisionPluginManager() = default;
    ~DecisionPluginManager() = default;
    
    void register_plugin(std::shared_ptr<DecisionPlugin> plugin);
    void unregister_plugin(const std::string& plugin_name);
    
    EntityState::BehaviorState make_decision_with_plugins(const WorldView& world,
                                                         const EnhancedEntityBehaviorData& config,
                                                         EntityState::BehaviorState current_state) const;
    
    std::vector<std::string> get_registered_plugins() const;
    bool is_plugin_registered(const std::string& plugin_name) const;
    
private:
    mutable std::mutex plugins_mutex_;
    std::unordered_map<std::string, std::shared_ptr<DecisionPlugin>> plugins_;
    
    // 线程安全检查
    bool check_thread_safety(const DecisionPlugin* plugin) const;
};

// ===========================================
// 11. 向量化决策处理器
// ===========================================
class VectorizedDecisionProcessor {
public:
    explicit VectorizedDecisionProcessor(const BatchConfig& config);
    ~VectorizedDecisionProcessor() = default;
    
    // 批量决策处理（SIMD优化）
    std::vector<EntityState::BehaviorState> process_batch_decisions(
        const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
        const std::vector<WorldView>& world_views,
        const std::vector<EntityState::BehaviorState>& current_states);
    
    // 单个实体决策（回退）
    EntityState::BehaviorState process_single_decision(
        const EnhancedEntityBehaviorData& config,
        const WorldView& world,
        EntityState::BehaviorState current_state);
    
private:
    BatchConfig config_;
    
#if defined(SIMD_LEVEL_AVX512)
    void process_batch_avx512(const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
                             const std::vector<WorldView>& world_views,
                             std::vector<EntityState::BehaviorState>& results);
#elif defined(SIMD_LEVEL_AVX2)
    void process_batch_avx2(const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
                           const std::vector<WorldView>& world_views,
                           std::vector<EntityState::BehaviorState>& results);
#elif defined(SIMD_LEVEL_SSE4)
    void process_batch_sse4(const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
                           const std::vector<WorldView>& world_views,
                           std::vector<EntityState::BehaviorState>& results);
#elif defined(SIMD_LEVEL_NEON)
    void process_batch_neon(const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
                          const std::vector<WorldView>& world_views,
                          std::vector<EntityState::BehaviorState>& results);
#else
    void process_batch_fallback(const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
                               const std::vector<WorldView>& world_views,
                               std::vector<EntityState::BehaviorState>& results);
#endif
};

// ===========================================
// 12. 主自动适应决策引擎
// ===========================================
class AdaptiveDecisionEngine {
public:
    explicit AdaptiveDecisionEngine(const BatchConfig& config = BatchConfig{});
    ~AdaptiveDecisionEngine() = default;
    
    // 核心决策接口
    EntityState::BehaviorState make_intelligent_decision(
        const std::string& entity_id,
        const WorldView& world,
        EntityState::BehaviorState current_state);
    
    // 批量决策处理
    std::vector<EntityState::BehaviorState> make_batch_decisions(
        const std::vector<std::string>& entity_ids,
        const std::vector<WorldView>& world_views,
        const std::vector<EntityState::BehaviorState>& current_states);
    
    // 配置管理
    bool load_entity_config(const std::string& entity_id, const nlohmann::json& json_config);
    bool load_entity_config_from_cache(const std::string& entity_id);
    bool has_entity_config(const std::string& entity_id) const;
    std::shared_ptr<EnhancedEntityBehaviorData> get_entity_config(const std::string& entity_id) const;
    
    // 版本管理
    void register_version_strategy(std::shared_ptr<VersionStrategy> strategy);
    void set_active_version(const std::string& version);
    std::string get_active_version() const;
    
    // 插件管理
    void register_decision_plugin(std::shared_ptr<DecisionPlugin> plugin);
    void enable_vectorized_processing(bool enable);
    
    // 性能监控
    struct EngineStats {
        uint64_t total_entities_loaded{0};
        uint64_t total_decisions_made{0};
        std::chrono::milliseconds avg_decision_time{0};
        size_t cache_hit_count{0};
        size_t cache_miss_count{0};
        double cache_hit_ratio() const {
            size_t total = cache_hit_count + cache_miss_count;
            return (total > 0) ? static_cast<double>(cache_hit_count) / total : 0.0;
        }
    };
    
    EngineStats get_engine_stats() const;
    void reset_engine_stats();
    
    // 配置验证
    bool validate_entity_config(const EnhancedEntityBehaviorData& config) const;
    void auto_fill_missing_properties(EnhancedEntityBehaviorData& config) const;
    
    // 1.21.10完整支持检查
    struct V1210SupportStatus {
        bool warden_support{false};
        bool breeze_support{false};
        bool bogged_support{false};
        bool trial_chambers_support{false};
        bool deep_dark_support{false};
        std::vector<std::string> unsupported_entities;
        bool is_fully_compatible() const {
            return warden_support && breeze_support && bogged_support;
        }
    };
    
    V1210SupportStatus check_v1210_support() const;
    
private:
    BatchConfig config_;
    std::shared_ptr<cache::HierarchicalCacheSystem> cache_system_;
    std::unique_ptr<SmartDecisionTree> decision_tree_;
    std::unique_ptr<VectorizedDecisionProcessor> vectorized_processor_;
    std::unique_ptr<DecisionPluginManager> plugin_manager_;
    
    // 缓存管理
    mutable std::mutex configs_mutex_;
    std::unordered_map<std::string, std::shared_ptr<EnhancedEntityBehaviorData>> entity_configs_;
    
    // 版本策略
    mutable std::mutex versions_mutex_;
    std::unordered_map<std::string, std::shared_ptr<VersionStrategy>> version_strategies_;
    std::string active_version_;
    
    // 性能统计
    mutable std::mutex stats_mutex_;
    EngineStats stats_;
    
    // 内部工具方法
    std::shared_ptr<EnhancedEntityBehaviorData> load_entity_from_cache(const std::string& entity_id);
    EntityState::BehaviorState process_entity_decision(const std::string& entity_id,
                                                     const WorldView& world,
                                                     EntityState::BehaviorState current_state);
    
    // 配置加载和验证
    bool parse_entity_from_json(const std::string& entity_id, const nlohmann::json& json,
                               std::shared_ptr<EnhancedEntityBehaviorData>& config);
    
    void apply_version_adaptations(EnhancedEntityBehaviorData& config) const;
    
    // 更新统计信息
    void update_stats(std::chrono::microseconds decision_time, bool cache_hit);
};

// ===========================================
// 13. 全局决策引擎实例管理器
// ===========================================
class GlobalDecisionEngine {
public:
    static GlobalDecisionEngine& get_instance();
    
    std::shared_ptr<AdaptiveDecisionEngine> get_engine_for_world(const std::string& world_id);
    void shutdown_all_engines();
    
    // 全局配置
    void set_global_cache_config(const cache::CacheConfig& config);
    void set_global_batch_config(const BatchConfig& config);
    
    // 性能汇总
    struct GlobalStats {
        size_t total_engines{0};
        uint64_t total_decisions_all_engines{0};
        std::chrono::milliseconds total_processing_time{0};
        double overall_cache_hit_ratio{0.0};
        size_t supported_entities_count{0};
    };
    
    GlobalStats get_global_stats() const;
    
private:
    GlobalDecisionEngine() = default;
    ~GlobalDecisionEngine();
    
    std::mutex engines_mutex_;
    std::unordered_map<std::string, std::weak_ptr<AdaptiveDecisionEngine>> world_engines_;
    
    cache::CacheConfig global_cache_config_;
    BatchConfig global_batch_config_;
    
    static std::unique_ptr<GlobalDecisionEngine> instance_;
    static std::once_flag init_flag_;
};

// ===========================================
// 14. 便利接口函数
// ===========================================
namespace convenient_api {

// 简化的决策接口
EntityState::BehaviorState decide_entity_behavior(
    const std::string& entity_id,
    float x, float y, float z,
    const std::vector<std::string>& nearby_entities,
    bool is_daytime,
    float light_level);

// 批量决策接口
std::vector<EntityState::BehaviorState> decide_multiple_entities(
    const std::vector<std::string>& entity_ids,
    const std::vector<std::array<float, 3>>& positions,
    const std::vector<std::vector<std::string>>& nearby_entities_list,
    const std::vector<bool>& is_daytime_list,
    const std::vector<float>& light_level_list);

// 配置文件加载接口
bool load_entity_configs_from_directory(const std::string& directory_path);
bool load_entity_configs_from_github(const std::string& version = "1.21.1");

// 验证配置完整性
struct ConfigValidationResult {
    bool is_valid{false};
    std::vector<std::string> missing_required_fields;
    std::vector<std::string> warnings;
    std::vector<std::string> suggestions;
};

ConfigValidationResult validate_entity_config_completeness(const std::string& entity_id);

} // namespace convenient_api

} // namespace ai
} // namespace lattice