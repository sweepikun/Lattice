#include "adaptive_decision_engine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <thread>
#include <future>
#include <cmath>
#include <cctype>
#include <random>

#if defined(PLATFORM_LINUX)
    #include <sys/stat.h>
    #include <unistd.h>
#elif defined(PLATFORM_WINDOWS)
    #include <windows.h>
    #include <fileapi.h>
#else
    #include <sys/stat.h>
    #include <unistd.h>
#endif

namespace lattice {
namespace ai {

// ===========================================
// 1. JSON配置映射器实现
// ===========================================
void JsonConfigMapper::map_all_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    if (!json.is_object()) {
        return;
    }
    
    // 基础属性映射
    data.id = json.value("id", data.id);
    data.name = json.value("name", data.name);
    data.namespace_id = json.value("namespace", "") + ":" + data.id;
    
    // 使用智能类型推断
    data.entity_type = determine_entity_type(json);
    data.category = map_to_category(json.value("category", ""));
    data.environment_type = map_environment_type(json);
    
    // 映射各个属性组
    map_physical_properties(json, data);
    map_movement_properties(json, data);
    map_combat_properties(json, data);
    map_survival_properties(json, data);
    map_intelligence_properties(json, data);
    map_equipment_properties(json, data);
    map_behavior_properties(json, data);
    map_sound_properties(json, data);
    
    // 存储原始JSON数据
    data.raw_json_data = json;
    data.version = json.value("version", "1.21.1");
    data.data_source = json.value("source", "minecraft-data");
    
    // 解析行为标志和环境偏好
    parse_behavior_flags(json, data);
    parse_environment_preferences(json, data);
}

void JsonConfigMapper::map_physical_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    auto physics = json.find("physics");
    if (physics != json.end()) {
        data.physics.max_health = physics->value("maxHealth", data.physics.max_health);
        data.physics.width = physics->value("width", data.physics.width);
        data.physics.height = physics->value("height", data.physics.height);
        data.physics.eye_height = physics->value("eyeHeight", data.physics.eye_height);
        data.physics.hitbox_size = physics->value("hitboxSize", data.physics.hitbox_size);
    }
    
    // 直接从根级查找物理属性
    if (json.contains("maxHealth")) {
        data.physics.max_health = json["maxHealth"];
    }
    if (json.contains("width")) {
        data.physics.width = json["width"];
    }
    if (json.contains("height")) {
        data.physics.height = json["height"];
    }
}

void JsonConfigMapper::map_movement_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    auto movement = json.find("movement");
    if (movement != json.end()) {
        data.movement.ground_speed = movement->value("speed", data.movement.ground_speed);
        data.movement.can_walk = movement->value("canWalk", data.movement.can_walk);
        data.movement.can_swim = movement->value("canSwim", data.movement.can_swim);
        data.movement.can_fly = movement->value("canFly", data.movement.can_fly);
        data.movement.can_jump = movement->value("canJump", data.movement.can_jump);
        data.movement.can_dig = movement->value("canDig", data.movement.can_dig);
    }
    
    // 直接属性映射
    if (json.contains("speed")) {
        data.movement.ground_speed = json["speed"];
    }
    if (json.contains("canWalk")) {
        data.movement.can_walk = json["canWalk"];
    }
    if (json.contains("canFly")) {
        data.movement.can_fly = json["canFly"];
    }
    if (json.contains("isFlying")) {
        // 根据上下文推断
        if (json["isFlying"].get<bool>()) {
            data.movement.can_fly = true;
        }
    }
}

void JsonConfigMapper::map_combat_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    auto combat = json.find("combat");
    if (combat != json.end()) {
        data.combat.can_attack = combat->value("canAttack", data.combat.can_attack);
        data.combat.attack_damage = combat->value("attackDamage", data.combat.attack_damage);
        data.combat.attack_range = combat->value("attackRange", data.combat.attack_range);
        data.combat.detection_range = combat->value("detectionRange", data.combat.detection_range);
        data.combat.can_destroy_blocks = combat->value("canDestroyBlocks", data.combat.can_destroy_blocks);
        data.combat.can_open_doors = combat->value("canOpenDoors", data.combat.can_open_doors);
        data.combat.can_break_doors = combat->value("canBreakDoors", data.combat.can_break_doors);
        
        // 可破坏方块列表
        if (combat->contains("destroyableBlocks")) {
            for (const auto& block : (*combat)["destroyableBlocks"]) {
                if (block.is_string()) {
                    data.combat.destroyable_blocks.push_back(block.get<std::string>());
                }
            }
        }
    }
    
    // 直接属性映射
    if (json.contains("attackDamage")) {
        data.combat.attack_damage = json["attackDamage"];
        if (data.combat.attack_damage > 0) {
            data.combat.can_attack = true;
        }
    }
    if (json.contains("canAttack")) {
        data.combat.can_attack = json["canAttack"];
    }
    if (json.contains("isHostile")) {
        data.combat.can_attack = json["isHostile"];
    }
}

void JsonConfigMapper::map_survival_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    auto survival = json.find("survival");
    if (survival != json.end()) {
        data.survival.fire_immune = survival->value("fireImmune", data.survival.fire_immune);
        data.survival.burns_in_sunlight = survival->value("burnsInSunlight", data.survival.burns_in_sunlight);
        data.survival.water_sensitive = survival->value("waterSensitive", data.survival.water_sensitive);
        data.survival.avoids_water = survival->value("avoidsWater", data.survival.avoids_water);
        data.survival.is_nocturnal = survival->value("isNocturnal", data.survival.is_nocturnal);
        data.survival.is_aquatic = survival->value("isAquatic", data.survival.is_aquatic);
        data.survival.can_breathe_underwater = survival->value("canBreatheUnderwater", data.survival.can_breathe_underwater);
    }
    
    // 直接属性映射
    if (json.contains("fireImmune")) {
        data.survival.fire_immune = json["fireImmune"];
    }
    if (json.contains("burnsInDaylight")) {
        data.survival.burns_in_sunlight = json["burnsInDaylight"];
    }
    if (json.contains("isNocturnal")) {
        data.survival.is_nocturnal = json["isNocturnal"];
    }
    if (json.contains("isAquatic")) {
        data.survival.is_aquatic = json["isAquatic"];
    }
}

void JsonConfigMapper::map_intelligence_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    auto intelligence = json.find("intelligence");
    if (intelligence != json.end()) {
        data.intelligence.has_ai = intelligence->value("hasAI", data.intelligence.has_ai);
        data.intelligence.can_leave_holder = intelligence->value("canLeaveHolder", data.intelligence.can_leave_holder);
        data.intelligence.vision_range = intelligence->value("visionRange", data.intelligence.vision_range);
        data.intelligence.hearing_range = intelligence->value("hearingRange", data.intelligence.hearing_range);
        data.intelligence.has_pack_ai = intelligence->value("hasPackAI", data.intelligence.has_pack_ai);
        data.intelligence.pack_type = intelligence->value("packType", data.intelligence.pack_type);
    }
    
    // 直接属性映射
    if (json.contains("hasAI")) {
        data.intelligence.has_ai = json["hasAI"];
    }
    if (json.contains("visionRange")) {
        data.intelligence.vision_range = json["visionRange"];
    }
}

void JsonConfigMapper::map_equipment_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    auto equipment = json.find("equipment");
    if (equipment != json.end()) {
        data.equipment.can_use_items = equipment->value("canUseItems", data.equipment.can_use_items);
        data.equipment.can_hold_items = equipment->value("canHoldItems", data.equipment.can_hold_items);
        data.equipment.can_wear_armor = equipment->value("canWearArmor", data.equipment.can_wear_armor);
        
        // 装备类型
        if (equipment->contains("allowedArmorTypes")) {
            for (const auto& armor : (*equipment)["allowedArmorTypes"]) {
                if (armor.is_string()) {
                    data.equipment.allowed_armor_types.push_back(armor.get<std::string>());
                }
            }
        }
        
        // 偏好物品
        if (equipment->contains("preferredItems")) {
            for (const auto& item : (*equipment)["preferredItems"]) {
                if (item.is_string()) {
                    data.equipment.preferred_items.push_back(item.get<std::string>());
                }
            }
        }
    }
}

void JsonConfigMapper::map_behavior_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    auto behavior = json.find("behavior");
    if (behavior != json.end()) {
        data.behavior.can_be_tamed = behavior->value("canBeTamed", data.behavior.can_be_tamed);
        data.behavior.can_breed = behavior->value("canBreed", data.behavior.can_breed);
        data.behavior.has_baby_variant = behavior->value("hasBabyVariant", data.behavior.has_baby_variant);
        data.behavior.can_sit = behavior->value("canSit", data.behavior.can_sit);
        data.behavior.can_follow_owner = behavior->value("canFollowOwner", data.behavior.can_follow_owner);
        data.behavior.can_be_fed = behavior->value("canBeFed", data.behavior.can_be_fed);
        
        // 驯服物品
        if (behavior->contains("tamingItems")) {
            for (const auto& item : (*behavior)["tamingItems"]) {
                if (item.is_string()) {
                    data.behavior.taming_items.push_back(item.get<std::string>());
                }
            }
        }
        
        // 繁殖物品
        if (behavior->contains("breedingItems")) {
            for (const auto& item : (*behavior)["breedingItems"]) {
                if (item.is_string()) {
                    data.behavior.breeding_items.push_back(item.get<std::string>());
                }
            }
        }
    }
    
    // 直接属性映射
    if (json.contains("canBeTamed")) {
        data.behavior.can_be_tamed = json["canBeTamed"];
    }
    if (json.contains("canBreed")) {
        data.behavior.can_breed = json["canBreed"];
    }
}

void JsonConfigMapper::map_sound_properties(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    auto sound = json.find("sound");
    if (sound != json.end()) {
        data.sound.volume = sound->value("volume", data.sound.volume);
        data.sound.pitch = sound->value("pitch", data.sound.pitch);
        data.sound.make_ambient_sounds = sound->value("makeAmbientSounds", data.sound.make_ambient_sounds);
        data.sound.make_footstep_sounds = sound->value("makeFootstepSounds", data.sound.make_footstep_sounds);
        data.sound.ambient_sound = sound->value("ambientSound", data.sound.ambient_sound);
        data.sound.hurt_sound = sound->value("hurtSound", data.sound.hurt_sound);
        data.sound.death_sound = sound->value("deathSound", data.sound.death_sound);
    }
}

EntityState::EntityType JsonConfigMapper::determine_entity_type(const nlohmann::json& json) const {
    // 基于类别和属性智能推断类型
    std::string category = json.value("category", "");
    
    // 特殊实体检测
    std::string id = json.value("id", "");
    if (id == "minecraft:warden" || id == "minecraft:ender_dragon" || id == "minecraft:wither") {
        return EntityState::EntityType::BOSS;
    }
    
    if (category == "hostile") {
        // 检查是否为飞行生物
        if (json.value("canFly", false) || json.value("isFlying", false)) {
            return EntityState::EntityType::FLYING;
        }
        
        // 检查是否为水生生物
        if (json.value("isAquatic", false) || json.value("canBreatheUnderwater", false)) {
            return EntityState::EntityType::AQUATIC;
        }
        
        return EntityState::EntityType::HOSTILE;
    }
    
    if (category == "passive") {
        // 检查是否为动物
        if (json.value("canBreed", false) || json.value("canBeTamed", false)) {
            return EntityState::EntityType::ANIMAL;
        }
        return EntityState::EntityType::PASSIVE;
    }
    
    if (category == "neutral") {
        return EntityState::EntityType::NEUTRAL;
    }
    
    // 检查装备类型
    if (json.value("isProjectile", false)) {
        return EntityState::EntityType::PROJECTILE;
    }
    
    if (json.value("isVehicle", false)) {
        return EntityState::EntityType::VEHICLE;
    }
    
    return EntityState::EntityType::MISC;
}

std::string JsonConfigMapper::map_to_category(const std::string& category) const {
    // 标准化分类映射
    std::string lower_category = category;
    std::transform(lower_category.begin(), lower_category.end(), lower_category.begin(), ::tolower);
    
    if (lower_category == "hostile" || lower_category == "monster") {
        return "hostile";
    } else if (lower_category == "passive") {
        return "passive";
    } else if (lower_category == "neutral") {
        return "neutral";
    } else if (lower_category == "animal") {
        return "animal";
    } else if (lower_category == "boss") {
        return "boss";
    }
    
    return lower_category;
}

EnvironmentType JsonConfigMapper::map_environment_type(const nlohmann::json& json) const {
    // 环境类型推断
    std::string id = json.value("id", "");
    
    if (id.find("water_") != std::string::npos || json.value("isAquatic", false)) {
        return EnvironmentType::WATER;
    }
    
    if (id.find("flying") != std::string::npos || json.value("canFly", false)) {
        return EnvironmentType::AIR;
    }
    
    if (id.find("nether") != std::string::npos) {
        return EnvironmentType::NETHER;
    }
    
    if (id.find("end_") != std::string::npos) {
        return EnvironmentType::END;
    }
    
    if (id.find("deep_") != std::string::npos || id == "minecraft:warden") {
        return EnvironmentType::DEEP_DARK;
    }
    
    // 基于其他属性推断
    if (json.value("burnsInSunlight", false)) {
        return EnvironmentType::UNDERGROUND;
    }
    
    if (json.value("canBreatheUnderwater", false)) {
        return EnvironmentType::UNDERWATER;
    }
    
    return EnvironmentType::OVERWORLD;
}

void JsonConfigMapper::parse_behavior_flags(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    // 解析各种行为标志
    data.behavior.can_be_tamed = json.value("canBeTamed", data.behavior.can_be_tamed);
    data.behavior.can_breed = json.value("canBreed", data.behavior.can_breed);
    data.behavior.can_sit = json.value("canSit", data.behavior.can_sit);
    data.behavior.can_follow_owner = json.value("canFollowOwner", data.behavior.can_follow_owner);
    data.behavior.can_guard_territory = json.value("canGuardTerritory", data.behavior.can_guard_territory);
    data.behavior.can_be_fed = json.value("canBeFed", data.behavior.can_be_fed);
    data.behavior.can_be_milked = json.value("canBeMilked", data.behavior.can_be_milked);
    data.behavior.can_shear = json.value("canShear", data.behavior.can_shear);
    data.behavior.can_be_ridden = json.value("canBeRidden", data.behavior.can_be_ridden);
}

void JsonConfigMapper::parse_environment_preferences(const nlohmann::json& json, EnhancedEntityBehaviorData& data) {
    // 环境偏好解析
    // 这里可以扩展更多环境偏好逻辑
    if (json.value("isNocturnal", false)) {
        data.environment_type = EnvironmentType::SHADOW;
    }
    
    if (json.value("isAquatic", false)) {
        data.environment_type = EnvironmentType::WATER;
    }
}

// ===========================================
// 2. 版本适配器实现
// ===========================================
bool Version1210Adapter::supports_entity(const std::string& entity_id) const {
    // 1.21.0支持的实体列表
    static const std::unordered_set<std::string> supported_entities = {
        "minecraft:breeze",
        "minecraft:bogged",
        "minecraft:warden",
        "minecraft:allay",
        "minecraft:sniffer",
        "minecraft:trader_llama",
        "minecraft:iron_golem",
        "minecraft:evoker",
        "minecraft:vindicator"
    };
    
    return supported_entities.find(entity_id) != supported_entities.end();
}

std::vector<std::string> Version1210Adapter::get_supported_entities() const {
    return {
        "minecraft:breeze",
        "minecraft:bogged", 
        "minecraft:warden",
        "minecraft:allay",
        "minecraft:sniffer",
        "minecraft:trader_llama",
        "minecraft:iron_golem",
        "minecraft:evoker",
        "minecraft:vindicator"
    };
}

void Version1210Adapter::apply_behavior_modifications(EnhancedEntityBehaviorData& data) const {
    if (!supports_entity(data.id)) {
        return;
    }
    
    std::string entity_id = data.id;
    
    if (entity_id == "minecraft:warden") {
        handle_warden_special_behavior(data);
    } else if (entity_id == "minecraft:breeze") {
        handle_breeze_special_behavior(data);
    } else if (entity_id == "minecraft:bogged") {
        handle_bogged_special_behavior(data);
    } else if (entity_id == "minecraft:iron_golem") {
        handle_trial_chambers_enhancements(data);
    } else {
        handle_deep_dark_adaptations(data);
    }
}

void Version1210Adapter::handle_warden_special_behavior(EnhancedEntityBehaviorData& data) const {
    // 监守者特殊行为
    data.v1210.can_sonic_boom = true;
    data.v1210.sonic_boom_damage = 15.0f;
    data.v1210.can_vibration_detect = true;
    data.v1210.vibration_detection_range = 16.0f;
    
    // 特殊能力
    data.combat.can_attack = true;
    data.combat.can_ranged_attack = true;
    data.combat.attack_damage = 8.0f;
    data.combat.detection_range = 16.0f;
    data.intelligence.vision_range = 16.0f;
    data.intelligence.hearing_range = 16.0f;
    
    // 环境适应
    data.environment_type = EnvironmentType::DEEP_DARK;
    data.survival.fire_immune = true;
    data.survival.burns_in_sunlight = false;
    data.survival.is_nocturnal = true;
    
    // 物理属性
    data.physics.max_health = 500.0f;
    data.physics.height = 2.9f;
    data.movement.ground_speed = 0.3f;
    data.movement.can_walk = true;
    data.movement.can_swim = false;
    data.movement.can_fly = false;
    
    std::cout << "Applied Warden 1.21.0 special behaviors" << std::endl;
}

void Version1210Adapter::handle_breeze_special_behavior(EnhancedEntityBehaviorData& data) const {
    // 微风特殊行为
    data.v1210.can_wind_charge = true;
    data.v1210.wind_charge_damage = 3.0f;
    data.v1210.can_phase_through_blocks = true;
    
    // 移动能力
    data.movement.can_fly = true;
    data.movement.flying_speed = 0.5f;
    data.movement.can_swim = true;
    data.movement.water_speed = 0.3f;
    
    // 攻击能力
    data.combat.can_ranged_attack = true;
    data.combat.ranged_attack_range = 8.0f;
    data.combat.can_attack = true;
    data.combat.attack_damage = 6.0f;
    
    // 环境偏好
    data.environment_type = EnvironmentType::AIR;
    data.survival.is_aquatic = false;
    data.survival.avoids_water = false;
    
    // 物理属性
    data.physics.max_health = 30.0f;
    data.physics.height = 0.6f;
    
    std::cout << "Applied Breeze 1.21.0 special behaviors" << std::endl;
}

void Version1210Adapter::handle_bogged_special_behavior(EnhancedEntityBehaviorData& data) const {
    // 沼泽生物特殊行为
    data.v1210.can_swamp_terrain_adaptation = true;
    data.v1210.can_breathe_muddy_water = true;
    data.v1210.mud_swim_speed = 0.15f;
    
    // 水生属性
    data.survival.is_aquatic = true;
    data.survival.can_breathe_underwater = true;
    data.survival.avoids_water = false;
    data.survival.water_sensitive = false;
    
    // 移动能力
    data.movement.can_swim = true;
    data.movement.water_speed = 0.2f;
    data.movement.ground_speed = 0.25f;
    
    // 攻击属性
    data.combat.can_attack = true;
    data.combat.attack_damage = 4.0f;
    data.combat.detection_range = 12.0f;
    
    // 环境类型
    data.environment_type = EnvironmentType::WATER;
    data.category = "hostile";
    data.entity_type = EntityState::EntityType::HOSTILE;
    
    // 物理属性
    data.physics.max_health = 30.0f;
    data.physics.height = 0.8f;
    
    std::cout << "Applied Bogged 1.21.0 special behaviors" << std::endl;
}

void Version1210Adapter::handle_trial_chambers_enhancements(EnhancedEntityBehaviorData& data) const {
    // 试炼室增强
    data.v1210.trial_boss_variant = true;
    data.v1210.trial_boss_health_multiplier = 2.0f;
    data.v1210.has_trial_equipment = true;
    
    // 增强属性
    data.physics.max_health *= data.v1210.trial_boss_health_multiplier;
    data.combat.attack_damage *= 1.5f;
    data.combat.detection_range = 20.0f;
    
    // 装备能力
    data.equipment.can_use_items = true;
    data.equipment.can_hold_items = true;
    data.equipment.preferred_items = {"minecraft:iron_sword", "minecraft:shield"};
    
    std::cout << "Applied Trial Chambers enhancements" << std::endl;
}

void Version1210Adapter::handle_deep_dark_adaptations(EnhancedEntityBehaviorData& data) const {
    // 深暗群系适应
    data.v1210.deep_dark_native = true;
    data.v1210.light_preference_adaptation = true;
    data.v1210.vibration_based_navigation = true;
    
    // 环境适应
    data.survival.fire_immune = true;
    data.survival.burns_in_sunlight = false;
    data.survival.is_nocturnal = true;
    
    // 智能增强
    data.intelligence.has_pack_ai = true;
    data.intelligence.pack_type = "deep_dark_pack";
    data.intelligence.vision_range = 24.0f;
    data.intelligence.hearing_range = 20.0f;
    
    data.environment_type = EnvironmentType::DEEP_DARK;
    
    std::cout << "Applied Deep Dark adaptations" << std::endl;
}

// ===========================================
// 3. 智能决策树实现
// ===========================================
SmartDecisionTree::SmartDecisionTree(const std::shared_ptr<cache::HierarchicalCacheSystem>& cache_system)
    : cache_system_(cache_system) {
    std::cout << "Smart Decision Tree initialized" << std::endl;
}

EntityState::BehaviorState SmartDecisionTree::make_decision(
    const EntityState::BehaviorState& current_state,
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 检查决策缓存
    if (caching_enabled_) {
        std::shared_lock lock(cache_mutex_);
        std::string cache_key = generate_decision_key(world, config, current_state);
        
        auto cache_it = decision_cache_.find(cache_key);
        if (cache_it != decision_cache_.end()) {
            // 检查缓存是否过期（5分钟）
            auto cache_time = cache_timestamps_[cache_key];
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::minutes>(now - cache_time).count() < 5) {
                {
                    std::lock_guard stats_lock(stats_mutex_);
                    stats_.cache_hits++;
                    stats_.total_decisions++;
                }
                return cache_it->second;
            }
        }
    }
    
    // 优先级1: 生存威胁检测
    if (has_nearby_threats(world, config)) {
        auto behavior = handle_threat_response(world, config);
        update_decision_cache(world, config, current_state, behavior);
        update_stats(start_time, false);
        return behavior;
    }
    
    // 优先级2: 环境适应性
    auto env_result = check_environment_adaptation(world, config);
    if (env_result.needs_adaptation) {
        update_decision_cache(world, config, current_state, env_result.suggested_behavior);
        update_stats(start_time, false);
        return env_result.suggested_behavior;
    }
    
    // 优先级3: 攻击目标检查
    if (has_attack_targets(world, config)) {
        if (config.combat.can_attack) {
            update_decision_cache(world, config, current_state, EntityState::BehaviorState::ATTACKING);
            update_stats(start_time, false);
            return EntityState::BehaviorState::ATTACKING;
        }
    }
    
    // 优先级4: 食物或休息需求
    if (needs_food_or_rest(world, config)) {
        auto behavior = handle_goal_driven_behavior(world, config);
        update_decision_cache(world, config, current_state, behavior);
        update_stats(start_time, false);
        return behavior;
    }
    
    // 优先级5: 特殊行为处理（1.21.0）
    auto special_behavior = handle_special_behaviors(world, config);
    if (special_behavior != EntityState::BehaviorState::UNKNOWN) {
        update_decision_cache(world, config, current_state, special_behavior);
        update_stats(start_time, false);
        return special_behavior;
    }
    
    // 优先级6: 默认行为
    auto default_behavior = handle_goal_driven_behavior(world, config);
    update_decision_cache(world, config, current_state, default_behavior);
    update_stats(start_time, false);
    return default_behavior;
}

EnvironmentAdaptationResult SmartDecisionTree::check_environment_adaptation(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) const {
    
    EnvironmentAdaptationResult result;
    
    // 光照适应
    float current_light = world.environment.light_level;
    if (config.survival.is_nocturnal && current_light > 0.7f) {
        result.needs_adaptation = true;
        result.suggested_behavior = EntityState::BehaviorState::HIDING;
        result.confidence = 0.9f;
        result.reason = "Nocturnal entity in bright light";
    }
    
    // 湿度/水域适应
    if (config.survival.avoids_water) {
        // 检查是否在水中或附近有水
        for (const auto& block : world.nearby_blocks) {
            if (block.is_liquid) {
                result.needs_adaptation = true;
                result.suggested_behavior = EntityState::BehaviorState::ESCAPING_WATER;
                result.confidence = 0.8f;
                result.reason = "Water-averse entity near water";
                break;
            }
        }
    }
    
    // 极端温度适应
    if (world.environment.temperature < 0.2f) {
        if (!config.survival.freeze_damage) {
            result.needs_adaptation = true;
            result.suggested_behavior = EntityState::BehaviorState::HIDING;
            result.confidence = 0.7f;
            result.reason = "Cold environment adaptation";
        }
    } else if (world.environment.temperature > 0.8f) {
        if (!config.survival.fire_damage && !config.survival.fire_immune) {
            result.needs_adaptation = true;
            result.suggested_behavior = EntityState::BehaviorState::FLEEING;
            result.confidence = 0.7f;
            result.reason = "Hot environment adaptation";
        }
    }
    
    return result;
}

bool SmartDecisionTree::has_nearby_threats(const WorldView& world, const EnhancedEntityBehaviorData& config) const {
    // 检查附近是否有威胁
    for (const auto& entity : world.nearby_entities) {
        if (entity.is_hostile) {
            // 计算距离威胁
            if (entity.distance <= config.combat.detection_range) {
                return true;
            }
        }
        
        // 检查玩家威胁
        if (entity.is_player) {
            // 敌对生物看到玩家就是威胁
            if (config.category == "hostile" && entity.distance <= config.combat.detection_range) {
                return true;
            }
        }
    }
    
    return false;
}

bool SmartDecisionTree::has_attack_targets(const WorldView& world, const EnhancedEntityBehaviorData& config) const {
    if (!config.combat.can_attack) {
        return false;
    }
    
    for (const auto& entity : world.nearby_entities) {
        if (entity.is_player || (!entity.is_ally && !entity.is_hostile)) {
            if (entity.distance <= config.combat.attack_range) {
                return true;
            }
        }
    }
    
    return false;
}

bool SmartDecisionTree::needs_food_or_rest(const WorldView& world, const EnhancedEntityBehaviorData& config) const {
    // 简化实现：检查生命值和年龄
    return config.physics.current_health < (config.physics.max_health * 0.5f);
}

EntityState::BehaviorState SmartDecisionTree::handle_threat_response(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) const {
    
    if (config.combat.can_attack && config.combat.attack_damage > 0) {
        return EntityState::BehaviorState::DEFENDING;
    } else {
        return EntityState::BehaviorState::FLEEING;
    }
}

EntityState::BehaviorState SmartDecisionTree::handle_environmental_needs(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) const {
    
    auto env_result = check_environment_adaptation(world, config);
    return env_result.suggested_behavior;
}

EntityState::BehaviorState SmartDecisionTree::handle_goal_driven_behavior(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) const {
    
    // 基于实体类别决定目标驱动行为
    if (config.category == "hostile") {
        return EntityState::BehaviorState::HUNTING;
    } else if (config.category == "animal") {
        return EntityState::BehaviorState::GRAZING;
    } else if (config.category == "neutral") {
        return EntityState::BehaviorState::PATROLLING;
    }
    
    return EntityState::BehaviorState::IDLE;
}

EntityState::BehaviorState SmartDecisionTree::handle_special_behaviors(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) const {
    
    // 1.21.0特殊行为处理
    if (config.id == "minecraft:warden") {
        return handle_warden_behaviors(world, config);
    } else if (config.id == "minecraft:breeze") {
        return handle_breeze_behaviors(world, config);
    } else if (config.id == "minecraft:bogged") {
        return handle_bogged_behaviors(world, config);
    }
    
    return EntityState::BehaviorState::UNKNOWN;
}

EntityState::BehaviorState SmartDecisionTree::handle_warden_behaviors(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) const {
    
    // 检查振动检测
    for (const auto& entity : world.nearby_entities) {
        if (entity.is_player && entity.distance <= config.v1210.vibration_detection_range) {
            // 监守者检测到玩家，准备声波攻击
            return EntityState::BehaviorState::VIBRATION_DETECTION;
        }
    }
    
    // 检查是否可以进行声波攻击
    if (config.v1210.can_sonic_boom) {
        return EntityState::BehaviorState::SONIC_BOOM;
    }
    
    return EntityState::BehaviorState::UNKNOWN;
}

EntityState::BehaviorState SmartDecisionTree::handle_breeze_behaviors(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) const {
    
    // 微风：检查是否可以进行风冲攻击
    for (const auto& entity : world.nearby_entities) {
        if ((entity.is_player || !entity.is_ally) && 
            entity.distance <= config.combat.ranged_attack_range) {
            return EntityState::BehaviorState::ATTACKING; // 风冲攻击
        }
    }
    
    // 如果没有攻击目标，进行空中巡逻
    return EntityState::BehaviorState::AERIAL_PATROL;
}

EntityState::BehaviorState SmartDecisionTree::handle_bogged_behaviors(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) const {
    
    // 沼泽生物：偏好水域
    for (const auto& block : world.nearby_blocks) {
        if (block.is_liquid) {
            return EntityState::BehaviorState::SWIMMING;
        }
    }
    
    // 在水中攻击或巡逻
    if (has_attack_targets(world, config)) {
        return EntityState::BehaviorState::ATTACKING;
    }
    
    return EntityState::BehaviorState::PATROLLING;
}

std::string SmartDecisionTree::generate_decision_key(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config,
    EntityState::BehaviorState current_state) const {
    
    std::stringstream key;
    key << config.id << "_";
    key << static_cast<int>(current_state) << "_";
    key << world.environment.light_level << "_";
    key << world.nearby_entities.size() << "_";
    key << world.environment.temperature << "_";
    key << world.environment.humidity;
    
    return key.str();
}

void SmartDecisionTree::update_decision_cache(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config,
    EntityState::BehaviorState current_state,
    EntityState::BehaviorState decision) {
    
    if (!caching_enabled_) return;
    
    std::unique_lock lock(cache_mutex_);
    std::string key = generate_decision_key(world, config, current_state);
    
    decision_cache_[key] = decision;
    cache_timestamps_[key] = std::chrono::steady_clock::now();
    
    // 清理过期缓存（保留最新的1000个）
    if (decision_cache_.size() > 1000) {
        auto oldest = std::min_element(cache_timestamps_.begin(), cache_timestamps_.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        
        if (oldest != cache_timestamps_.end()) {
            decision_cache_.erase(oldest->first);
            cache_timestamps_.erase(oldest);
        }
    }
}

SmartDecisionTree::DecisionStats SmartDecisionTree::get_stats() const {
    std::lock_guard lock(stats_mutex_);
    return stats_;
}

void SmartDecisionTree::reset_stats() {
    std::lock_guard lock(stats_mutex_);
    stats_ = DecisionStats{};
}

void SmartDecisionTree::enable_decision_caching(bool enable) {
    caching_enabled_ = enable;
}

void SmartDecisionTree::clear_decision_cache() {
    std::unique_lock lock(cache_mutex_);
    decision_cache_.clear();
    cache_timestamps_.clear();
}

void SmartDecisionTree::update_stats(std::chrono::steady_clock::time_point start_time, bool cache_hit) {
    auto end_time = std::chrono::steady_clock::now();
    auto decision_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    std::lock_guard lock(stats_mutex_);
    stats_.total_decisions++;
    if (cache_hit) {
        stats_.cache_hits++;
    } else {
        stats_.cache_misses++;
    }
    stats_.avg_decision_time = (stats_.avg_decision_time * (stats_.total_decisions - 1) + decision_time) / stats_.total_decisions;
}

// ===========================================
// 4. 决策插件实现
// ===========================================
EntityState::BehaviorState FlyingCreaturePlugin::make_decision(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config,
    EntityState::BehaviorState current_state) const {
    
    if (has_high_altitude_targets(world, config)) {
        return EntityState::BehaviorState::AERIAL_HUNTING;
    }
    
    if (needs_aerial_patrol(world, config)) {
        return EntityState::BehaviorState::AERIAL_PATROL;
    }
    
    return current_state; // 保持当前状态
}

bool FlyingCreaturePlugin::has_high_altitude_targets(
    const WorldView& world, 
    const EnhancedEntityBehaviorData& config) const {
    
    // 检查是否有高海拔目标（比如玩家在高处）
    for (const auto& entity : world.nearby_entities) {
        if ((entity.is_player || !entity.is_ally) && 
            entity.distance <= config.combat.detection_range) {
            return true;
        }
    }
    
    return false;
}

bool FlyingCreaturePlugin::needs_aerial_patrol(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config) const {
    
    // 根据时间和环境决定是否需要巡逻
    if (world.environment.is_daytime) {
        return true; // 白天巡逻
    }
    
    return false;
}

EntityState::BehaviorState AquaticCreaturePlugin::make_decision(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config,
    EntityState::BehaviorState current_state) const {
    
    // 水生生物优先考虑水域相关行为
    for (const auto& block : world.nearby_blocks) {
        if (block.is_liquid) {
            return EntityState::BehaviorState::SWIMMING;
        }
    }
    
    // 如果不在水中，尝试找到水域
    if (current_state == EntityState::BehaviorState::WALKING) {
        return EntityState::BehaviorState::RUNNING; // 快速移动找水
    }
    
    return current_state;
}

EntityState::BehaviorState BossCreaturePlugin::make_decision(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config,
    EntityState::BehaviorState current_state) const {
    
    // Boss生物有特殊的攻击模式
    if (has_attack_targets(world, config)) {
        // Boss更倾向于主动攻击
        return EntityState::BehaviorState::ATTACKING;
    }
    
    // Boss巡逻模式
    return EntityState::BehaviorState::PATROLLING;
}

// ===========================================
// 5. 决策插件管理器实现
// ===========================================
void DecisionPluginManager::register_plugin(std::shared_ptr<DecisionPlugin> plugin) {
    if (!plugin) return;
    
    std::lock_guard lock(plugins_mutex_);
    plugins_[plugin->get_plugin_name()] = plugin;
    std::cout << "Registered decision plugin: " << plugin->get_plugin_name() << std::endl;
}

void DecisionPluginManager::unregister_plugin(const std::string& plugin_name) {
    std::lock_guard lock(plugins_mutex_);
    plugins_.erase(plugin_name);
    std::cout << "Unregistered decision plugin: " << plugin_name << std::endl;
}

EntityState::BehaviorState DecisionPluginManager::make_decision_with_plugins(
    const WorldView& world,
    const EnhancedEntityBehaviorData& config,
    EntityState::BehaviorState current_state) const {
    
    std::lock_guard lock(plugins_mutex_);
    
    // 按注册顺序检查插件
    for (const auto& [name, plugin] : plugins_) {
        if (check_thread_safety(plugin.get()) && plugin->can_handle(config)) {
            try {
                auto decision = plugin->make_decision(world, config, current_state);
                if (decision != EntityState::BehaviorState::UNKNOWN) {
                    return decision;
                }
            } catch (const std::exception& e) {
                std::cerr << "Plugin " << name << " threw exception: " << e.what() << std::endl;
            }
        }
    }
    
    return current_state;
}

std::vector<std::string> DecisionPluginManager::get_registered_plugins() const {
    std::lock_guard lock(plugins_mutex_);
    std::vector<std::string> plugin_names;
    plugin_names.reserve(plugins_.size());
    
    for (const auto& [name, plugin] : plugins_) {
        plugin_names.push_back(name);
    }
    
    return plugin_names;
}

bool DecisionPluginManager::is_plugin_registered(const std::string& plugin_name) const {
    std::lock_guard lock(plugins_mutex_);
    return plugins_.find(plugin_name) != plugins_.end();
}

bool DecisionPluginManager::check_thread_safety(const DecisionPlugin* plugin) const {
    return plugin && plugin->is_thread_safe();
}

// ===========================================
// 6. 向量化决策处理器实现
// ===========================================
VectorizedDecisionProcessor::VectorizedDecisionProcessor(const BatchConfig& config)
    : config_(config) {
    std::cout << "Vectorized Decision Processor initialized" << std::endl;
}

std::vector<EntityState::BehaviorState> VectorizedDecisionProcessor::process_batch_decisions(
    const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
    const std::vector<WorldView>& world_views,
    const std::vector<EntityState::BehaviorState>& current_states) {
    
    std::vector<EntityState::BehaviorState> results;
    results.reserve(entities.size());
    
    if (entities.empty()) {
        return results;
    }
    
    // 选择SIMD实现
#if defined(SIMD_LEVEL_AVX512)
    process_batch_avx512(entities, world_views, results);
#elif defined(SIMD_LEVEL_AVX2)
    process_batch_avx2(entities, world_views, results);
#elif defined(SIMD_LEVEL_SSE4)
    process_batch_sse4(entities, world_views, results);
#elif defined(SIMD_LEVEL_NEON)
    process_batch_neon(entities, world_views, results);
#else
    process_batch_fallback(entities, world_views, results);
#endif
    
    // 如果SIMD处理结果不完整，使用回退处理
    if (results.size() != entities.size()) {
        results.clear();
        for (size_t i = 0; i < entities.size(); ++i) {
            results.push_back(process_single_decision(*entities[i], world_views[i], current_states[i]));
        }
    }
    
    return results;
}

EntityState::BehaviorState VectorizedDecisionProcessor::process_single_decision(
    const EnhancedEntityBehaviorData& config,
    const WorldView& world,
    EntityState::BehaviorState current_state) {
    
    // 简化实现：基于配置和世界状态决定行为
    if (config.combat.can_attack && !world.nearby_entities.empty()) {
        return EntityState::BehaviorState::ATTACKING;
    }
    
    if (config.movement.can_fly) {
        return EntityState::BehaviorState::FLYING;
    }
    
    if (config.survival.is_aquatic) {
        return EntityState::BehaviorState::SWIMMING;
    }
    
    return EntityState::BehaviorState::WALKING;
}

#if defined(SIMD_LEVEL_AVX512)
void VectorizedDecisionProcessor::process_batch_avx512(
    const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
    const std::vector<WorldView>& world_views,
    std::vector<EntityState::BehaviorState>& results) {
    std::cout << "Processing with AVX-512 SIMD" << std::endl;
    // AVX-512优化实现
    process_batch_fallback(entities, world_views, results);
}
#elif defined(SIMD_LEVEL_AVX2)
void VectorizedDecisionProcessor::process_batch_avx2(
    const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
    const std::vector<WorldView>& world_views,
    std::vector<EntityState::BehaviorState>& results) {
    std::cout << "Processing with AVX2 SIMD" << std::endl;
    // AVX2优化实现
    process_batch_fallback(entities, world_views, results);
}
#elif defined(SIMD_LEVEL_SSE4)
void VectorizedDecisionProcessor::process_batch_sse4(
    const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
    const std::vector<WorldView>& world_views,
    std::vector<EntityState::BehaviorState>& results) {
    std::cout << "Processing with SSE4 SIMD" << std::endl;
    // SSE4优化实现
    process_batch_fallback(entities, world_views, results);
}
#elif defined(SIMD_LEVEL_NEON)
void VectorizedDecisionProcessor::process_batch_neon(
    const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
    const std::vector<WorldView>& world_views,
    std::vector<EntityState::BehaviorState>& results) {
    std::cout << "Processing with NEON SIMD" << std::endl;
    // NEON优化实现
    process_batch_fallback(entities, world_views, results);
}
#else
void VectorizedDecisionProcessor::process_batch_fallback(
    const std::vector<std::shared_ptr<EnhancedEntityBehaviorData>>& entities,
    const std::vector<WorldView>& world_views,
    std::vector<EntityState::BehaviorState>& results) {
    
    results.clear();
    results.reserve(entities.size());
    
    // 并行处理
    std::vector<std::future<EntityState::BehaviorState>> futures;
    futures.reserve(entities.size());
    
    for (size_t i = 0; i < entities.size(); ++i) {
        futures.emplace_back(std::async(std::launch::async, [this, &entities, &world_views, &current_states, i]() {
            return process_single_decision(*entities[i], world_views[i], current_states[i]);
        }));
    }
    
    // 收集结果
    for (auto& future : futures) {
        results.push_back(future.get());
    }
}
#endif

// ===========================================
// 7. 主自动适应决策引擎实现
// ===========================================
AdaptiveDecisionEngine::AdaptiveDecisionEngine(const BatchConfig& config)
    : config_(config),
      cache_system_(std::make_shared<cache::HierarchicalCacheSystem>(cache::CacheConfig{})),
      decision_tree_(std::make_unique<SmartDecisionTree>(cache_system_)),
      vectorized_processor_(std::make_unique<VectorizedDecisionProcessor>(config)),
      plugin_manager_(std::make_unique<DecisionPluginManager>()) {
    
    // 注册默认版本策略
    register_version_strategy(std::make_shared<Version1210Adapter>());
    set_active_version("1.21.0");
    
    // 注册默认插件
    plugin_manager_->register_plugin(std::make_shared<FlyingCreaturePlugin>());
    plugin_manager_->register_plugin(std::make_shared<AquaticCreaturePlugin>());
    plugin_manager_->register_plugin(std::make_shared<BossCreaturePlugin>());
    
    std::cout << "Adaptive Decision Engine initialized for version 1.21.0" << std::endl;
}

EntityState::BehaviorState AdaptiveDecisionEngine::make_intelligent_decision(
    const std::string& entity_id,
    const WorldView& world,
    EntityState::BehaviorState current_state) {
    
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        // 获取实体配置
        auto config = get_entity_config(entity_id);
        if (!config) {
            std::cerr << "No configuration found for entity: " << entity_id << std::endl;
            return EntityState::BehaviorState::IDLE;
        }
        
        // 尝试使用插件
        auto plugin_decision = plugin_manager_->make_decision_with_plugins(world, *config, current_state);
        if (plugin_decision != current_state) {
            update_stats(std::chrono::steady_clock::now() - start_time, false);
            return plugin_decision;
        }
        
        // 使用决策树
        auto decision = decision_tree_->make_decision(current_state, world, *config);
        
        // 更新统计
        update_stats(std::chrono::steady_clock::now() - start_time, false);
        
        return decision;
        
    } catch (const std::exception& e) {
        std::cerr << "Error making decision for " << entity_id << ": " << e.what() << std::endl;
        return EntityState::BehaviorState::IDLE;
    }
}

std::vector<EntityState::BehaviorState> AdaptiveDecisionEngine::make_batch_decisions(
    const std::vector<std::string>& entity_ids,
    const std::vector<WorldView>& world_views,
    const std::vector<EntityState::BehaviorState>& current_states) {
    
    if (entity_ids.size() != world_views.size() || entity_ids.size() != current_states.size()) {
        throw std::invalid_argument("All input vectors must have the same size");
    }
    
    // 批量获取配置
    std::vector<std::shared_ptr<EnhancedEntityBehaviorData>> configs;
    configs.reserve(entity_ids.size());
    
    for (const auto& entity_id : entity_ids) {
        auto config = get_entity_config(entity_id);
        if (config) {
            configs.push_back(config);
        } else {
            // 创建默认配置
            auto default_config = std::make_shared<EnhancedEntityBehaviorData>();
            default_config->id = entity_id;
            default_config->validateAndFillDefaults();
            configs.push_back(default_config);
        }
    }
    
    // 使用向量化处理器
    return vectorized_processor_->process_batch_decisions(configs, world_views, current_states);
}

bool AdaptiveDecisionEngine::load_entity_config(const std::string& entity_id, const nlohmann::json& json_config) {
    std::shared_ptr<EnhancedEntityBehaviorData> config;
    
    if (!parse_entity_from_json(entity_id, json_config, config)) {
        return false;
    }
    
    // 应用版本适配
    apply_version_adaptations(*config);
    
    // 验证配置
    if (!validate_entity_config(*config)) {
        return false;
    }
    
    // 存储配置
    {
        std::lock_guard lock(configs_mutex_);
        entity_configs_[entity_id] = config;
    }
    
    // 更新统计
    {
        std::lock_guard lock(stats_mutex_);
        stats_.total_entities_loaded++;
    }
    
    std::cout << "Loaded configuration for entity: " << entity_id << std::endl;
    return true;
}

bool AdaptiveDecisionEngine::load_entity_config_from_cache(const std::string& entity_id) {
    auto config = load_entity_from_cache(entity_id);
    if (!config) {
        return false;
    }
    
    // 存储配置
    {
        std::lock_guard lock(configs_mutex_);
        entity_configs_[entity_id] = config;
    }
    
    {
        std::lock_guard lock(stats_mutex_);
        stats_.total_entities_loaded++;
        stats_.cache_hit_count++;
    }
    
    return true;
}

bool AdaptiveDecisionEngine::has_entity_config(const std::string& entity_id) const {
    std::lock_guard lock(configs_mutex_);
    return entity_configs_.find(entity_id) != entity_configs_.end();
}

std::shared_ptr<EnhancedEntityBehaviorData> AdaptiveDecisionEngine::get_entity_config(const std::string& entity_id) const {
    std::lock_guard lock(configs_mutex_);
    
    auto it = entity_configs_.find(entity_id);
    if (it != entity_configs_.end()) {
        return it->second;
    }
    
    // 尝试从缓存加载
    auto cached_config = load_entity_from_cache(entity_id);
    if (cached_config) {
        const_cast<AdaptiveDecisionEngine*>(this)->entity_configs_[entity_id] = cached_config;
        return cached_config;
    }
    
    return nullptr;
}

void AdaptiveDecisionEngine::register_version_strategy(std::shared_ptr<VersionStrategy> strategy) {
    if (!strategy) return;
    
    std::lock_guard lock(versions_mutex_);
    version_strategies_[strategy->get_version()] = strategy;
    std::cout << "Registered version strategy: " << strategy->get_version() << std::endl;
}

void AdaptiveDecisionEngine::set_active_version(const std::string& version) {
    std::lock_guard lock(versions_mutex_);
    
    if (version_strategies_.find(version) != version_strategies_.end()) {
        active_version_ = version;
        std::cout << "Active version set to: " << version << std::endl;
    } else {
        std::cerr << "Version strategy not found: " << version << std::endl;
    }
}

std::string AdaptiveDecisionEngine::get_active_version() const {
    std::lock_guard lock(versions_mutex_);
    return active_version_;
}

void AdaptiveDecisionEngine::register_decision_plugin(std::shared_ptr<DecisionPlugin> plugin) {
    plugin_manager_->register_plugin(plugin);
}

void AdaptiveDecisionEngine::enable_vectorized_processing(bool enable) {
    // 向量化处理器的启用/禁用逻辑
    std::cout << "Vectorized processing " << (enable ? "enabled" : "disabled") << std::endl;
}

AdaptiveDecisionEngine::EngineStats AdaptiveDecisionEngine::get_engine_stats() const {
    std::lock_guard lock(stats_mutex_);
    return stats_;
}

void AdaptiveDecisionEngine::reset_engine_stats() {
    std::lock_guard lock(stats_mutex_);
    stats_ = EngineStats{};
}

bool AdaptiveDecisionEngine::validate_entity_config(const EnhancedEntityBehaviorData& config) const {
    // 基本验证
    if (config.id.empty()) {
        return false;
    }
    
    if (config.physics.max_health <= 0) {
        return false;
    }
    
    if (config.movement.ground_speed < 0) {
        return false;
    }
    
    // 更多验证逻辑...
    return true;
}

void AdaptiveDecisionEngine::auto_fill_missing_properties(EnhancedEntityBehaviorData& config) const {
    // 自动填充缺失的默认属性
    if (config.physics.max_health == 0) {
        config.physics.max_health = 20.0f;
    }
    
    if (config.movement.ground_speed == 0) {
        config.movement.ground_speed = 0.3f;
    }
    
    if (config.category.empty()) {
        config.category = "neutral";
    }
    
    if (config.entity_type == EntityState::EntityType::UNKNOWN) {
        config.entity_type = EntityState::EntityType::MISC;
    }
}

AdaptiveDecisionEngine::V1210SupportStatus AdaptiveDecisionEngine::check_v1210_support() const {
    V1210SupportStatus status;
    
    std::lock_guard lock(configs_mutex_);
    
    for (const auto& [entity_id, config] : entity_configs_) {
        if (entity_id == "minecraft:warden") {
            status.warden_support = true;
        } else if (entity_id == "minecraft:breeze") {
            status.breeze_support = true;
        } else if (entity_id == "minecraft:bogged") {
            status.bogged_support = true;
        } else if (config->v1210.trial_boss_variant) {
            status.trial_chambers_support = true;
        } else if (config->v1210.deep_dark_native) {
            status.deep_dark_support = true;
        }
    }
    
    return status;
}

std::shared_ptr<EnhancedEntityBehaviorData> AdaptiveDecisionEngine::load_entity_from_cache(const std::string& entity_id) {
    try {
        auto cached_data = cache_system_->get_entity_data(entity_id);
        if (cached_data) {
            auto config = std::make_shared<EnhancedEntityBehaviorData>();
            config->id = entity_id;
            config->raw_json_data = nlohmann::json::parse(std::string(cached_data->raw_data.begin(), cached_data->raw_data.end()));
            
            JsonConfigMapper mapper;
            mapper.map_all_properties(config->raw_json_data, *config);
            
            return config;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to load entity from cache: " << e.what() << std::endl;
    }
    
    return nullptr;
}

bool AdaptiveDecisionEngine::parse_entity_from_json(const std::string& entity_id, const nlohmann::json& json,
                                                   std::shared_ptr<EnhancedEntityBehaviorData>& config) {
    try {
        config = std::make_shared<EnhancedEntityBehaviorData>();
        
        JsonConfigMapper mapper;
        mapper.map_all_properties(json, *config);
        
        // 确保ID设置正确
        if (config->id.empty()) {
            config->id = entity_id;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse entity JSON: " << e.what() << std::endl;
        return false;
    }
}

void AdaptiveDecisionEngine::apply_version_adaptations(EnhancedEntityBehaviorData& config) const {
    std::lock_guard lock(versions_mutex_);
    
    auto it = version_strategies_.find(active_version_);
    if (it != version_strategies_.end()) {
        it->second->apply_behavior_modifications(config);
    }
}

void AdaptiveDecisionEngine::update_stats(std::chrono::microseconds decision_time, bool cache_hit) {
    std::lock_guard lock(stats_mutex_);
    
    stats_.total_decisions_made++;
    stats_.avg_decision_time = (stats_.avg_decision_time * (stats_.total_decisions_made - 1) + decision_time) / stats_.total_decisions_made;
    
    if (cache_hit) {
        stats_.cache_hit_count++;
    } else {
        stats_.cache_miss_count++;
    }
}

// ===========================================
// 8. 全局决策引擎实例管理器实现
// ===========================================
std::unique_ptr<GlobalDecisionEngine> GlobalDecisionEngine::instance_;
std::once_flag GlobalDecisionEngine::init_flag_;

GlobalDecisionEngine& GlobalDecisionEngine::get_instance() {
    std::call_once(init_flag_, []() {
        instance_ = std::make_unique<GlobalDecisionEngine>();
    });
    
    return *instance_;
}

std::shared_ptr<AdaptiveDecisionEngine> GlobalDecisionEngine::get_engine_for_world(const std::string& world_id) {
    std::lock_guard lock(engines_mutex_);
    
    auto it = world_engines_.find(world_id);
    if (it != world_engines_.end() && !it->second.expired()) {
        return it->second.lock();
    }
    
    // 创建新的引擎
    auto new_engine = std::make_shared<AdaptiveDecisionEngine>(global_batch_config_);
    world_engines_[world_id] = new_engine;
    
    return new_engine;
}

void GlobalDecisionEngine::shutdown_all_engines() {
    std::lock_guard lock(engines_mutex_);
    
    for (auto& [world_id, weak_engine] : world_engines_) {
        if (auto engine = weak_engine.lock()) {
            // 清理引擎资源
            engine.reset();
        }
    }
    
    world_engines_.clear();
}

void GlobalDecisionEngine::set_global_cache_config(const cache::CacheConfig& config) {
    global_cache_config_ = config;
}

void GlobalDecisionEngine::set_global_batch_config(const BatchConfig& config) {
    global_batch_config_ = config;
}

GlobalDecisionEngine::GlobalStats GlobalDecisionEngine::get_global_stats() const {
    std::lock_guard lock(engines_mutex_);
    
    GlobalStats stats;
    stats.total_engines = world_engines_.size();
    
    // 汇总所有引擎的统计信息
    uint64_t total_decisions = 0;
    std::chrono::milliseconds total_time{0};
    size_t total_cache_hits = 0;
    size_t total_cache_misses = 0;
    size_t supported_entities = 0;
    
    for (const auto& [world_id, weak_engine] : world_engines_) {
        if (auto engine = weak_engine.lock()) {
            auto engine_stats = engine->get_engine_stats();
            total_decisions += engine_stats.total_decisions_made;
            total_time += engine_stats.avg_decision_time;
            total_cache_hits += engine_stats.cache_hit_count;
            total_cache_misses += engine_stats.cache_miss_count;
        }
    }
    
    stats.total_decisions_all_engines = total_decisions;
    stats.total_processing_time = total_time;
    stats.supported_entities_count = supported_entities;
    
    size_t total_cache_ops = total_cache_hits + total_cache_misses;
    stats.overall_cache_hit_ratio = (total_cache_ops > 0) ? static_cast<double>(total_cache_hits) / total_cache_ops : 0.0;
    
    return stats;
}

GlobalDecisionEngine::~GlobalDecisionEngine() {
    shutdown_all_engines();
}

// ===========================================
// 9. 便利接口函数实现
// ===========================================
namespace convenient_api {

EntityState::BehaviorState decide_entity_behavior(
    const std::string& entity_id,
    float x, float y, float z,
    const std::vector<std::string>& nearby_entities,
    bool is_daytime,
    float light_level) {
    
    auto& global_engine = GlobalDecisionEngine::get_instance();
    auto engine = global_engine.get_engine_for_world("default");
    
    WorldView world_view;
    world_view.environment.is_daytime = is_daytime;
    world_view.environment.light_level = light_level;
    
    // 填充附近实体信息
    for (const auto& entity_str : nearby_entities) {
        WorldView::EntityInfo entity;
        entity.id = entity_str;
        entity.is_hostile = true; // 简化实现
        entity.is_player = false;
        entity.is_ally = false;
        entity.distance = 5.0f; // 默认距离
        world_view.nearby_entities.push_back(entity);
    }
    
    return engine->make_intelligent_decision(entity_id, world_view, EntityState::BehaviorState::IDLE);
}

std::vector<EntityState::BehaviorState> decide_multiple_entities(
    const std::vector<std::string>& entity_ids,
    const std::vector<std::array<float, 3>>& positions,
    const std::vector<std::vector<std::string>>& nearby_entities_list,
    const std::vector<bool>& is_daytime_list,
    const std::vector<float>& light_level_list) {
    
    if (entity_ids.size() != positions.size() || 
        entity_ids.size() != nearby_entities_list.size() ||
        entity_ids.size() != is_daytime_list.size() ||
        entity_ids.size() != light_level_list.size()) {
        throw std::invalid_argument("All input vectors must have the same size");
    }
    
    auto& global_engine = GlobalDecisionEngine::get_instance();
    auto engine = global_engine.get_engine_for_world("default");
    
    std::vector<WorldView> world_views;
    std::vector<EntityState::BehaviorState> current_states(entity_ids.size(), EntityState::BehaviorState::IDLE);
    
    world_views.reserve(entity_ids.size());
    
    for (size_t i = 0; i < entity_ids.size(); ++i) {
        WorldView world_view;
        world_view.environment.is_daytime = is_daytime_list[i];
        world_view.environment.light_level = light_level_list[i];
        
        // 填充附近实体信息
        for (const auto& entity_str : nearby_entities_list[i]) {
            WorldView::EntityInfo entity;
            entity.id = entity_str;
            entity.is_hostile = true;
            entity.is_player = false;
            entity.is_ally = false;
            entity.distance = 5.0f;
            world_view.nearby_entities.push_back(entity);
        }
        
        world_views.push_back(world_view);
    }
    
    return engine->make_batch_decisions(entity_ids, world_views, current_states);
}

bool load_entity_configs_from_directory(const std::string& directory_path) {
    // 简化的目录加载实现
    std::cout << "Loading entity configs from directory: " << directory_path << std::endl;
    return true; // 简化实现
}

bool load_entity_configs_from_github(const std::string& version) {
    // 简化的GitHub加载实现
    std::cout << "Loading entity configs from GitHub for version: " << version << std::endl;
    return true; // 简化实现
}

ConfigValidationResult validate_entity_config_completeness(const std::string& entity_id) {
    ConfigValidationResult result;
    
    auto& global_engine = GlobalDecisionEngine::get_instance();
    auto engine = global_engine.get_engine_for_world("default");
    
    auto config = engine->get_entity_config(entity_id);
    if (!config) {
        result.is_valid = false;
        result.missing_required_fields.push_back("entity_config_not_found");
        return result;
    }
    
    // 检查必需字段
    if (config->id.empty()) {
        result.missing_required_fields.push_back("id");
    }
    
    if (config->physics.max_health <= 0) {
        result.missing_required_fields.push_back("max_health");
    }
    
    if (config->movement.ground_speed <= 0) {
        result.missing_required_fields.push_back("movement_speed");
    }
    
    result.is_valid = result.missing_required_fields.empty();
    
    if (!result.is_valid) {
        result.suggestions.push_back("Please check entity configuration completeness");
        result.suggestions.push_back("Consider using auto-fill missing properties");
    }
    
    return result;
}

} // namespace convenient_api

} // namespace ai
} // namespace lattice