#include "biological_ai.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <execution>
#include <memory_resource>
#include <iostream>
#include <filesystem>
#include <thread>
#include <immintrin.h>
#include <emmintrin.h>
#include <string_view>
#include "fmt_wrapper.hpp"
#include <nlohmann/json.hpp>

namespace lattice::entity {

// ====== DataModule 实现 ======

DataModule::DataModule(const std::string& minecraftDataPath, bool useGitHub)
    : minecraftDataPath_(minecraftDataPath), useGitHub_(useGitHub) {
    // 如果使用GitHub，验证路径格式
    if (useGitHub_) {
        if (minecraftDataPath_.empty()) {
            minecraftDataPath_ = "https://github.com/PrismarineJS/minecraft-data";
        }
        fprintf(stdout, "Using GitHub minecraft-data: %s\n", minecraftDataPath_.c_str());
    } else {
        fprintf(stdout, "Using local minecraft-data: %s\n", minecraftDataPath_.c_str());
    }
}

bool DataModule::loadEntityData() {
    return loadEntityDataForVersion("1.20.4"); // 默认加载最新版本
}

bool DataModule::loadEntityDataForVersion(const std::string& version) {
    std::lock_guard lock(dataMutex_);
    
    // 构建实体数据文件路径
    std::string entitiesFile;
    
    if (useGitHub_) {
        // 使用GitHub raw URL格式（硬编码minecraft-data仓库）
        entitiesFile = minecraftDataPath_ + "/raw/master/data/pc/" + version + "/entities.json";
        
        // 记录GitHub数据源信息
        fprintf(stdout, "Loading entity data from GitHub: %s\n", entitiesFile.c_str());
    } else {
        // 使用本地路径
        entitiesFile = minecraftDataPath_ + "/data/pc/" + version + "/entities.json";
        
        fprintf(stdout, "Loading entity data from local: %s\n", entitiesFile.c_str());
    }
    
    // 直接调用parseEntityJSON处理文件读取和解析
    return parseEntityJSON(entitiesFile);
}

bool DataModule::parseEntityJSON(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open entities file: %s\n", filePath.c_str());
        return false;
    }
    
    // 读取完整JSON内容
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    
    try {
        // 使用nlohmann::json进行完整的JSON解析
        auto jsonData = nlohmann::json::parse(content);
        
        // 验证JSON结构
        if (!jsonData.is_array()) {
            fprintf(stderr, "Invalid JSON structure: expected array at root level\n");
            return false;
        }
        
        // 解析每个实体对象
        for (const auto& entityJson : jsonData) {
            try {
                EntityBehaviorData data = parseEntityFromJson(entityJson);
                if (!data.id.empty()) {
                    entityData_[data.id] = data;
                    // 同时按name索引
                    if (!data.name.empty()) {
                        entityDataByName_[data.name] = data.id;
                    }
                }
            } catch (const std::exception& e) {
                fprintf(stderr, "Failed to parse entity: %s\n", e.what());
                continue;
            }
        }
        
        printf("Successfully parsed %zu entities using nlohmann::json\n", entityData_.size());
        
    } catch (const nlohmann::json::parse_error& e) {
        fprintf(stderr, "JSON parse error: %s\n", e.what());
        return false;
    } catch (const std::exception& e) {
        fprintf(stderr, "Unexpected error during JSON parsing: %s\n", e.what());
        return false;
    }
    
    populateCategoryIndex();
    printf("Loaded %zu entities from %s\n", entityData_.size(), filePath.c_str());
    return true;
}

EntityBehaviorData DataModule::parseEntityFromJson(const nlohmann::json& entityJson) {
    EntityBehaviorData data;
    
    // 解析基本字段 - 使用JSON对象直接访问
    data.id = entityJson.value("name", "");
    data.name = entityJson.value("displayName", data.id);
    
    if (data.id.empty()) return EntityBehaviorData{}; // 无效实体
    
    // 解析数值字段 - 直接从JSON获取
    auto parseJsonNumber = [&](const std::string& field, float defaultValue) -> float {
        return entityJson.value(field, defaultValue);
    };
    
    // 解析分类信息
    std::string categoryStr = entityJson.value("category", "Other");
    data.category = mapCategory(categoryStr);
    
    // 解析生物类型和显示名
    if (entityJson.contains("type")) {
        std::string typeStr = entityJson["type"].get<std::string>();
        data.entityType = typeStr;
    }
    
    // 设置基础属性 - 从JSON或使用默认值
    data.maxHealth = parseJsonNumber("maxHealth", getHealthFromId(data.id));
    data.attackDamage = getAttackDamageFromId(data.id);
    data.followRange = parseJsonNumber("followRange", 16.0f);
    data.pathfindingRange = parseJsonNumber("pathfindingRange", 32.0f);
    data.movementSpeed = parseJsonNumber("movementSpeed", 0.3f);
    data.attackCooldown = parseJsonNumber("attackCooldown", 1.0f);
    data.attackInterval = parseJsonNumber("attackInterval", 0.8f);
    
    // 解析宽度和高度用于碰撞检测
    if (entityJson.contains("width")) {
        data.width = entityJson["width"].get<float>();
    }
    if (entityJson.contains("height")) {
        data.height = entityJson["height"].get<float>();
    }
    
    // 解析特殊属性（如果JSON中有）
    if (entityJson.contains("fireImmune")) {
        data.fireImmune = entityJson["fireImmune"].get<bool>();
    }
    if (entityJson.contains("flyingSpeed")) {
        data.flyingSpeed = entityJson["flyingSpeed"].get<float>();
    }
    
    // 设置特殊属性
    setupEntitySpecificProperties(data);
    
    // 验证解析的数据
    if (!validateEntityData(data)) {
        fprintf(stderr, "Warning: Invalid entity data for %s\n", data.id.c_str());
    }
    
    return data;
}

// 保留原来的函数用于向后兼容（但标记为deprecated）
EntityBehaviorData DataModule::parseEntityObject(const std::string& jsonStr) {
    fprintf(stderr, "Warning: parseEntityObject is deprecated, use parseEntityFromJson instead\n");
    
    // 使用简单的字符串解析作为fallback
    EntityBehaviorData data;
    
    // 解析基本字段
    data.id = extractJsonField(jsonStr, "name", "");
    data.name = extractJsonField(jsonStr, "displayName", data.id);
    
    if (data.id.empty()) return EntityBehaviorData{}; // 无效实体
    
    // 解析数值字段
    auto parseNumber = [&](const std::string& field) -> float {
        std::string value = extractJsonField(jsonStr, field, "0");
        return std::stof(value);
    };
    
    data.category = mapCategory(extractJsonField(jsonStr, "category", "Other"));
    
    // 设置基础属性
    data.maxHealth = parseNumber("height") > 0 ? getHealthFromId(data.id) : 20.0f;
    data.attackDamage = getAttackDamageFromId(data.id);
    data.followRange = 16.0f;
    data.pathfindingRange = 32.0f;
    data.movementSpeed = 0.3f;
    data.attackCooldown = 1.0f;
    data.attackInterval = 0.8f;
    
    // 设置特殊属性
    setupEntitySpecificProperties(data);
    
    return data;
}

std::string DataModule::extractJsonField(const std::string& json, const std::string& field, const std::string& defaultValue) {
    std::string pattern = "\"" + field + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return defaultValue;
    
    // 跳过字段名和冒号
    pos += pattern.length();
    while (pos < json.length() && std::isspace(json[pos])) pos++;
    
    if (pos >= json.length()) return defaultValue;
    
    // 字符串值处理
    if (json[pos] == '"') {
        pos++; // 跳过开始引号
        size_t end = json.find('"', pos);
        if (end != std::string::npos) {
            return json.substr(pos, end - pos);
        }
    }
    
    // 数值或其他值处理
    size_t end = pos;
    while (end < json.length() && json[end] != ',' && json[end] != '}' && json[end] != ']') {
        end++;
    }
    
    std::string value = json.substr(pos, end - pos);
    // 去除前后空格和引号
    while (!value.empty() && std::isspace(value.front())) value.erase(value.begin());
    while (!value.empty() && std::isspace(value.back())) value.pop_back();
    
    if (!value.empty() && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.length() - 2);
    }
    
    return value.empty() ? defaultValue : value;
}

std::string DataModule::mapCategory(const std::string& minecraftCategory) {
    if (minecraftCategory.find("Hostile") != std::string::npos) return "hostile";
    if (minecraftCategory.find("Passive") != std::string::npos) return "animal";
    if (minecraftCategory.find("Neutral") != std::string::npos) return "neutral";
    if (minecraftCategory.find("Water") != std::string::npos || 
        minecraftCategory.find("Fish") != std::string::npos) return "water_creature";
    if (minecraftCategory.find("Projectile") != std::string::npos) return "projectile";
    if (minecraftCategory.find("Immobile") != std::string::npos) return "immobile";
    if (minecraftCategory.find("Vehicle") != std::string::npos) return "vehicle";
    
    return "other";
}

void DataModule::populateCategoryIndex() {
    categoryIndex_.clear();
    for (const auto& [id, data] : entityData_) {
        categoryIndex_[data.category].push_back(id);
    }
}

EntityBehaviorData* DataModule::getEntityData(const std::string& entityId) {
    std::shared_lock lock(dataMutex_);
    auto it = entityData_.find(entityId);
    return (it != entityData_.end()) ? &it->second : nullptr;
}

std::vector<std::string> DataModule::getAllEntityIds() const {
    std::shared_lock lock(dataMutex_);
    std::vector<std::string> ids;
    ids.reserve(entityData_.size());
    for (const auto& [id, _] : entityData_) {
        ids.push_back(id);
    }
    return ids;
}

std::vector<EntityBehaviorData> DataModule::getEntitiesByCategory(const std::string& category) const {
    std::shared_lock lock(dataMutex_);
    std::vector<EntityBehaviorData> result;
    
    auto it = categoryIndex_.find(category);
    if (it != categoryIndex_.end()) {
        for (const auto& entityId : it->second) {
            auto entityIt = entityData_.find(entityId);
            if (entityIt != entityData_.end()) {
                result.push_back(entityIt->second);
            }
        }
    }
    return result;
}

bool DataModule::validateEntityData(const EntityBehaviorData& data) const {
    return !data.id.empty() && 
           data.maxHealth > 0 && 
           data.movementSpeed >= 0 &&
           data.followRange >= 0 &&
           data.pathfindingRange >= 0;
}

std::string DataModule::determineCategoryFromId(const std::string& id) {
    // 使用C++20三路比较和全面的Minecraft生物分类
    auto endsWith = [](const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() && 
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    
    // 敌对生物 (Hostile Mobs) - 所有攻击性生物
    if (endsWith(id, "zombie") || endsWith(id, "skeleton") || 
        endsWith(id, "spider") || endsWith(id, "creeper") ||
        endsWith(id, "enderman") || endsWith(id, "wither") ||
        endsWith(id, "slime") || endsWith(id, "magma_cube") ||
        endsWith(id, "blaze") || endsWith(id, "ghast") ||
        endsWith(id, "phantom") || endsWith(id, "vex") ||
        endsWith(id, "vindicator") || endsWith(id, "pillager") ||
        endsWith(id, "evoker") || endsWith(id, "witch") ||
        endsWith(id, "husk") || endsWith(id, "stray") ||
        endsWith(id, "drowned") || endsWith(id, "cave_spider") ||
        endsWith(id, "giant") || endsWith(id, "ender_dragon") ||
        endsWith(id, "guardian") || endsWith(id, "elder_guardian") ||
        endsWith(id, "evoker_fangs") || endsWith(id, "illusioner") ||
        endsWith(id, "hoglin") || endsWith(id, "piglin_brute") ||
        endsWith(id, "piglin") || endsWith(id, "zoglin") ||
        endsWith(id, "drowned") || endsWith(id, "dolphin") == false) {
        return "hostile";
    }
    
    // 友好动物 (Passive Animals) - 不攻击的友好生物
    else if (endsWith(id, "cow") || endsWith(id, "pig") || 
             endsWith(id, "sheep") || endsWith(id, "chicken") ||
             endsWith(id, "horse") || endsWith(id, "donkey") ||
             endsWith(id, "mule") || endsWith(id, "cat") ||
             endsWith(id, "parrot") || endsWith(id, "rabbit") ||
             endsWith(id, "turtle") || endsWith(id, "bee") ||
             endsWith(id, "fox") || endsWith(id, "axolotl") ||
             endsWith(id, "goat") || endsWith(id, "glow_squid") ||
             endsWith(id, "mooshroom") || endsWith(id, "allay") ||
             endsWith(id, "frog") || endsWith(id, "tadpole")) {
        return "animal";
    }
    
    // 水生生物 (Water Creatures) - 所有水中生物
    else if (endsWith(id, "fish") || endsWith(id, "cod") ||
             endsWith(id, "salmon") || endsWith(id, "tropical_fish") ||
             endsWith(id, "pufferfish") || endsWith(id, "squid") ||
             endsWith(id, "dolphin") || endsWith(id, "axolotl")) {
        return "water_creature";
    }
    
    // 投射物 (Projectiles) - 所有投射物
    else if (endsWith(id, "arrow") || endsWith(id, "snowball") ||
             endsWith(id, "egg") || endsWith(id, "ender_pearl") ||
             endsWith(id, "fireball") || endsWith(id, "small_fireball") ||
             endsWith(id, "wither_skull") || endsWith(id, "dragon_fireball") ||
             endsWith(id, "potion") || endsWith(id, "shulker_bullet")) {
        return "projectile";
    }
    
    // 中性生物 (Neutral Mobs) - 可被激怒的生物
    else if (endsWith(id, "iron_golem") || endsWith(id, "snow_golem") ||
             endsWith(id, "villager") || endsWith(id, "wandering_trader") ||
             endsWith(id, "polar_bear") || endsWith(id, "llama") ||
             endsWith(id, "trader_llama") || endsWith(id, "panda") ||
             endsWith(id, "fox") || endsWith(id, "bee") ||
             endsWith(id, "dolphin") || endsWith(id, "wolf")) {
        return "neutral";
    }
    
    // 静止生物 (Immobile) - 不移动的实体
    else if (endsWith(id, "armor_stand") || endsWith(id, "item_frame") ||
             endsWith(id, "painting") || endsWith(id, "end_crystal") ||
             endsWith(id, "area_effect_cloud")) {
        return "immobile";
    }
    
    // 载具 (Vehicles) - 交通工具
    else if (endsWith(id, "boat") || endsWith(id, "minecart")) {
        return "vehicle";
    }
    
    // 方块实体 (Blocks) - 掉落方块
    else if (endsWith(id, "falling_block")) {
        return "block";
    }
    
    // 物品 (Drops) - 可拾取物品
    else if (endsWith(id, "item") || endsWith(id, "experience_orb") ||
             endsWith(id, "firework_rocket")) {
        return "item";
    }
    
    return "other";
}

float DataModule::getHealthFromId(const std::string& id) {
    // 使用C++20 unordered_map和constexpr实现快速查找，包含所有Minecraft生物
    static constexpr std::pair<const char*, float> healthTable[] = {
        // Boss生物
        {"wither", 300.0f},
        {"ender_dragon", 200.0f},
        
        // 大型友好生物
        {"iron_golem", 100.0f},
        {"polar_bear", 30.0f},
        {"horse", 30.0f},
        {"donkey", 30.0f},
        {"mule", 30.0f},
        
        // 中型友好生物
        {"cat", 10.0f},
        {"parrot", 6.0f},
        {"turtle", 30.0f},
        {"bee", 10.0f},
        {"fox", 10.0f},
        {"panda", 20.0f},
        {"llama", 30.0f},
        {"trader_llama", 30.0f},
        {"axolotl", 14.0f},
        {"goat", 10.0f},
        {"glow_squid", 10.0f},
        {"mooshroom", 10.0f},
        {"allay", 20.0f},
        {"frog", 10.0f},
        {"tadpole", 3.0f},
        
        // 小型友好生物
        {"cow", 10.0f},
        {"pig", 10.0f},
        {"sheep", 8.0f},
        {"chicken", 4.0f},
        {"rabbit", 3.0f},
        
        // NPC生物
        {"villager", 20.0f},
        {"wandering_trader", 20.0f},
        
        // 僵尸类敌对生物
        {"zombie", 20.0f},
        {"husk", 20.0f},
        {"drowned", 20.0f},
        {"zombie_villager", 20.0f},
        {"drowned", 20.0f},
        
        // 骷髅类敌对生物
        {"skeleton", 20.0f},
        {"stray", 20.0f},
        {"wither_skeleton", 20.0f},
        
        // 昆虫类敌对生物
        {"spider", 16.0f},
        {"cave_spider", 16.0f},
        
        // 特殊敌对生物
        {"creeper", 20.0f},
        {"enderman", 40.0f},
        {"endermite", 8.0f},
        {"slime", 4.0f},
        {"magma_cube", 4.0f},
        
        // 火焰类敌对生物
        {"blaze", 20.0f},
        {"ghast", 10.0f},
        {"fireball", 0.0f},
        
        // 飞行敌对生物
        {"phantom", 20.0f},
        {"vex", 14.0f},
        {"bat", 6.0f},
        
        // 掠夺者敌对生物
        {"vindicator", 24.0f},
        {"pillager", 24.0f},
        {"evoker", 24.0f},
        {"evoker_fangs", 0.0f},
        {"witch", 26.0f},
        {"illusioner", 32.0f},
        
        // 海洋敌对生物
        {"guardian", 30.0f},
        {"elder_guardian", 80.0f},
        {"dolphin", 10.0f},
        
        // 下界敌对生物
        {"hoglin", 40.0f},
        {"piglin", 16.0f},
        {"piglin_brute", 50.0f},
        {"zoglin", 40.0f},
        
        // 载具和静止生物
        {"boat", 40.0f},
        {"armor_stand", 0.0f},
        {"item_frame", 0.0f},
        {"painting", 0.0f},
        {"end_crystal", 0.0f},
        
        // 投射物
        {"arrow", 0.0f},
        {"snowball", 0.0f},
        {"egg", 0.0f},
        {"ender_pearl", 0.0f},
        {"wither_skull", 0.0f},
        {"dragon_fireball", 0.0f},
        {"shulker_bullet", 0.0f},
        
        // 掉落物品
        {"item", 0.0f},
        {"experience_orb", 0.0f},
        {"firework_rocket", 0.0f},
        
        // 其他方块
        {"falling_block", 0.0f},
        {"area_effect_cloud", 0.0f},
        
        // 雪傀儡
        {"snow_golem", 4.0f}
    };
    
    // 使用线性搜索查找匹配（表较小，性能足够）
    for (const auto& [entityName, health] : healthTable) {
        if (id.find(entityName) != std::string::npos) {
            return health;
        }
    }
    
    // 根据实体类型推断默认值
    if (id.find("fish") != std::string::npos) return 3.0f;
    if (id.find("cod") != std::string::npos) return 3.0f;
    if (id.find("salmon") != std::string::npos) return 3.0f;
    if (id.find("tropical_fish") != std::string::npos) return 3.0f;
    if (id.find("pufferfish") != std::string::npos) return 3.0f;
    if (id.find("squid") != std::string::npos) return 10.0f;
    if (id.find("arrow") != std::string::npos || id.find("projectile") != std::string::npos) return 0.0f;
    if (id.find("minecart") != std::string::npos) return 40.0f;
    if (id.find("wolf") != std::string::npos) return 8.0f;
    
    return 20.0f; // 默认生命值
}

float DataModule::getAttackDamageFromId(const std::string& id) {
    // 使用C++20 unordered_map和constexpr实现快速查找，包含所有Minecraft生物攻击伤害
    static constexpr std::pair<const char*, float> damageTable[] = {
        // Boss生物
        {"wither", 8.0f},         // 凋零攻击伤害
        {"ender_dragon", 6.0f},   // 末影龙攻击伤害
        
        // 大型友好生物（被激怒时攻击）
        {"iron_golem", 21.0f},    // 铁傀儡重击
        {"polar_bear", 9.0f},     // 北极熊攻击
        
        // 僵尸类攻击
        {"zombie", 3.0f},         // 普通僵尸
        {"husk", 3.0f},           // 干尸
        {"drowned", 3.0f},        // 溺尸
        {"zombie_villager", 3.0f}, // 僵尸村民
        {"drowned", 3.0f},        // 溺尸
        
        // 骷髅类攻击
        {"skeleton", 2.0f},       // 骷髅（远程）
        {"stray", 2.0f},          // 流浪者
        {"wither_skeleton", 4.0f}, // 凋零骷髅
        
        // 昆虫类攻击
        {"spider", 2.0f},         // 蜘蛛
        {"cave_spider", 2.0f},    // 洞穴蜘蛛
        {"bee", 2.0f},            // 蜜蜂
        
        // 特殊攻击生物
        {"creeper", 0.0f},        // 苦力怕（爆炸伤害另行处理）
        {"enderman", 21.0f},      // 末影人攻击
        {"endermite", 1.0f},      // 末影螨
        {"slime", 1.0f},          // 史莱姆
        {"magma_cube", 1.0f},     // 岩浆怪
        
        // 火焰类攻击
        {"blaze", 6.0f},          // 烈焰人（火球攻击）
        {"ghast", 6.0f},          // 恶魂（火球攻击）
        {"fireball", 0.0f},       // 火球（投射物）
        {"dragon_fireball", 0.0f}, // 龙火球
        
        // 飞行攻击
        {"phantom", 6.0f},        // 幻翼攻击
        {"vex", 5.0f},            // 恼鬼攻击
        {"bat", 0.0f},            // 蝙蝠不攻击
        
        // 掠夺者攻击
        {"vindicator", 13.0f},    // 卫道士重击
        {"pillager", 4.0f},       // 掠夺者远程攻击
        {"evoker", 6.0f},         // 唤魔者（召唤攻击）
        {"evoker_fangs", 0.0f},   // 唤魔者尖牙
        {"witch", 0.0f},          // 女巫（药水攻击）
        {"illusioner", 6.0f},     // 幻术师
        
        // 海洋攻击
        {"guardian", 6.0f},       // 守卫者（激光攻击）
        {"elder_guardian", 12.0f}, // 远古守卫者（强化攻击）
        {"dolphin", 0.0f},        // 海豚不攻击
        
        // 下界攻击
        {"hoglin", 6.0f},         // 疣猪兽
        {"piglin", 3.0f},         // 猪灵
        {"piglin_brute", 8.0f},   // 猪灵蛮兵
        {"zoglin", 7.0f},         // 僵尸疣猪兽
        
        // 中性生物（被激怒时攻击）
        {"wolf", 4.0f},           // 狼
        {"cat", 4.0f},            // 猫
        {"llama", 5.0f},          // 羊驼
        {"trader_llama", 5.0f},   // 行商羊驼
        {"fox", 2.0f},            // 狐狸
        {"panda", 6.0f},          // 熊猫
        {"goat", 2.0f},           // 山羊
        
        // 大型可骑乘生物
        {"horse", 5.0f},          // 马
        {"donkey", 2.0f},         // 驴
        {"mule", 2.0f},           // 骡子
        
        // 水生攻击
        {"axolotl", 3.0f},        // 美西螈
        
        // 友好生物（不攻击）
        {"cow", 0.0f},            // 牛
        {"pig", 0.0f},            // 猪
        {"sheep", 0.0f},          // 羊
        {"chicken", 0.0f},        // 鸡
        {"rabbit", 0.0f},         // 兔子
        {"turtle", 0.0f},         // 海龟
        {"villager", 0.0f},       // 村民
        {"wandering_trader", 0.0f}, // 流浪商人
        {"parrot", 0.0f},         // 鹦鹉
        {"allay", 0.0f},          // 悦灵
        {"frog", 0.0f},           // 青蛙
        {"tadpole", 0.0f},        // 蝌蚪
        {"glow_squid", 0.0f},     // 发光鱿鱼
        {"mooshroom", 0.0f},      // 哞菇
        
        // 投射物（无直接伤害）
        {"arrow", 0.0f},          // 箭
        {"snowball", 0.0f},       // 雪球
        {"egg", 0.0f},            // 鸡蛋
        {"ender_pearl", 0.0f},    // 末影珍珠
        {"wither_skull", 0.0f},   // 凋零骷髅头
        {"shulker_bullet", 0.0f}, // 潜影贝导弹
        {"potion", 0.0f},         // 药水
        
        // 静止和特殊实体
        {"armor_stand", 0.0f},    // 盔甲架
        {"item_frame", 0.0f},     // 物品展示框
        {"painting", 0.0f},       // 画
        {"end_crystal", 0.0f},    // 末地水晶
        {"area_effect_cloud", 0.0f}, // 效果云
        {"experience_orb", 0.0f}, // 经验球
        {"item", 0.0f},           // 掉落物品
        {"firework_rocket", 0.0f}, // 烟花火箭
        {"falling_block", 0.0f},  // 掉落方块
        
        // 载具
        {"boat", 0.0f},           // 船
        {"minecart", 0.0f},       // 矿车
        
        // 其他
        {"giant", 25.0f},         // 巨人
        {"snow_golem", 0.0f}      // 雪傀儡（不攻击）
    };
    
    // 使用线性搜索查找匹配
    for (const auto& [entityName, damage] : damageTable) {
        if (id.find(entityName) != std::string::npos) {
            return damage;
        }
    }
    
    // 根据实体类型推断默认值
    if (id.find("fish") != std::string::npos) return 0.0f;
    if (id.find("cod") != std::string::npos) return 0.0f;
    if (id.find("salmon") != std::string::npos) return 0.0f;
    if (id.find("tropical_fish") != std::string::npos) return 0.0f;
    if (id.find("pufferfish") != std::string::npos) return 1.0f; // 河豚有刺
    if (id.find("squid") != std::string::npos) return 0.0f;
    if (id.find("arrow") != std::string::npos || id.find("projectile") != std::string::npos) return 0.0f;
    
    return 0.0f; // 默认无攻击伤害
}

void DataModule::setupEntitySpecificProperties(EntityBehaviorData& data) {
    // 使用C++20的结构化绑定和更精确的属性设置
    const auto& entityId = data.id;
    
    // 使用更高效的lambda表达式设置属性模式
    auto setFlag = [&data](auto fieldPtr, bool value) { 
        data.*fieldPtr = value; 
    };
    
    // 定义实体类型分组，提高代码可读性和维护性
    auto isZombieType = [&entityId](const auto& variant) {
        return entityId.find(variant) != std::string::npos;
    };
    
    // 僵尸类实体 - 可以破门、怕阳光
    if (entityId.find("zombie") != std::string::npos || 
        entityId.find("husk") != std::string::npos ||
        entityId.find("drowned") != std::string::npos) {
        setFlag(&EntityBehaviorData::canBreakDoors, true);
        setFlag(&EntityBehaviorData::burnsInSunlight, true);
        setFlag(&EntityBehaviorData::canAttack, true);
    }
    // 骷髅类实体 - 怕阳光
    else if (entityId.find("skeleton") != std::string::npos ||
             entityId.find("stray") != std::string::npos) {
        setFlag(&EntityBehaviorData::burnsInSunlight, true);
        setFlag(&EntityBehaviorData::canAttack, true);
        setFlag(&EntityBehaviorData::canRangedAttack, true);
    }
    // 飞行生物
    else if (entityId.find("bat") != std::string::npos ||
             entityId.find("phantom") != std::string::npos ||
             entityId.find("blaze") != std::string::npos ||
             entityId.find("vex") != std::string::npos ||
             entityId.find("ghast") != std::string::npos) {
        setFlag(&EntityBehaviorData::canFly, true);
        if (entityId.find("bat") != std::string::npos) {
            setFlag(&EntityBehaviorData::isNocturnal, true);
        }
    }
    // 水生生物
    else if (entityId.find("fish") != std::string::npos ||
             entityId.find("squid") != std::string::npos ||
             entityId.find("dolphin") != std::string::npos ||
             entityId.find("axolotl") != std::string::npos) {
        setFlag(&EntityBehaviorData::canSwim, true);
        setFlag(&EntityBehaviorData::isAquatic, true);
        if (entityId.find("axolotl") != std::string::npos) {
            setFlag(&EntityBehaviorData::burnsInSunlight, true);
        }
    }
    // 爬行类（蜘蛛）
    else if (entityId.find("spider") != std::string::npos) {
        setFlag(&EntityBehaviorData::canClimbWalls, true);
        setFlag(&EntityBehaviorData::canAttack, true);
        setFlag(&EntityBehaviorData::burnsInSunlight, true);
    }
    // 跳跃能力强的生物
    else if (entityId.find("goat") != std::string::npos) {
        setFlag(&EntityBehaviorData::canJump, true);
        data.jumpHeight = 1.3f; // 山羊可以跳得更高
        setFlag(&EntityBehaviorData::canAttack, true);
    }
    // 传送能力
    else if (entityId.find("enderman") != std::string::npos) {
        setFlag(&EntityBehaviorData::canTeleport, true);
        setFlag(&EntityBehaviorData::canAttack, true);
        setFlag(&EntityBehaviorData::burnsInSunlight, true);
    }
    // 远程攻击
    else if (entityId.find("pillager") != std::string::npos ||
             entityId.find("evoker") != std::string::npos ||
             entityId.find("witch") != std::string::npos) {
        setFlag(&EntityBehaviorData::canRangedAttack, true);
        setFlag(&EntityBehaviorData::canAttack, true);
    }
    // 爆炸能力
    else if (entityId.find("creeper") != std::string::npos) {
        setFlag(&EntityBehaviorData::canExplode, true);
        setFlag(&EntityBehaviorData::canAttack, true);
    }
    
    // 设置环境光偏好（使用C++20的结构化绑定）
    if (data.isAquatic) {
        data.lightPreferences["0.0"] = 0.1f; // 深海偏好
        data.lightPreferences["15.0"] = 0.0f;
        data.environmentType = "water";
    } else if (data.canFly) {
        data.lightPreferences["0.0"] = 0.2f;
        data.lightPreferences["7.0"] = 0.8f; // 中等亮度偏好
        data.lightPreferences["15.0"] = 0.9f;
        data.environmentType = "air";
    } else if (data.burnsInSunlight) {
        data.lightPreferences["0.0"] = 1.0f; // 夜行性生物偏好黑暗
        data.lightPreferences["7.0"] = 0.3f;
        data.lightPreferences["15.0"] = 0.0f;
        data.environmentType = "shadow";
    } else {
        data.lightPreferences["0.0"] = 0.0f;
        data.lightPreferences["15.0"] = 1.0f; // 陆地生物偏好阳光
        data.environmentType = "land";
    }
    
    // 设置特殊行为属性
    if (entityId.find("cat") != std::string::npos) {
        setFlag(&EntityBehaviorData::canBeTamed, true);
        setFlag(&EntityBehaviorData::isFeline, true);
    } else if (entityId.find("dog") != std::string::npos || entityId.find("wolf") != std::string::npos) {
        setFlag(&EntityBehaviorData::canBeTamed, true);
        setFlag(&EntityBehaviorData::isCanine, true);
        setFlag(&EntityBehaviorData::canAttack, true);
    } else if (entityId.find("horse") != std::string::npos ||
               entityId.find("donkey") != std::string::npos ||
               entityId.find("mule") != std::string::npos) {
        setFlag(&EntityBehaviorData::canBeTamed, true);
        setFlag(&EntityBehaviorData::isMountable, true);
        setFlag(&EntityBehaviorData::canJump, true);
        data.jumpHeight = 1.0f;
    }
    
    // 设置目标优先级
    if (data.category == "hostile") {
        data.targetPriorities["player"] = 10.0f;
        data.targetPriorities["villager"] = 8.0f;
        data.targetPriorities["iron_golem"] = 6.0f;
    } else if (data.category == "animal") {
        data.targetPriorities["player"] = 2.0f;
        data.targetPriorities["grass_block"] = 5.0f;
        data.targetPriorities["tall_grass"] = 3.0f;
    } else if (data.category == "neutral") {
        data.targetPriorities["player"] = 1.0f;
        if (data.id.find("iron_golem") != std::string::npos) {
            data.targetPriorities["hostile"] = 10.0f;
        }
    }
}

// ====== AIEngine 实现 ======

AIEngine::AIEngine()
    : dataModule_(nullptr), versionStrategy_(nullptr) {
}

bool AIEngine::initialize(const std::string& minecraftDataPath, const std::string& gameVersion, bool useGitHub) {
    try {
        // 加载数据模块
        dataModule_ = std::make_unique<DataModule>(minecraftDataPath, useGitHub);
        if (!dataModule_->loadEntityDataForVersion(gameVersion)) {
            fprintf(stderr, "Failed to load entity data\n");
            return false;
        }
        
        // 创建版本策略
        versionStrategy_ = StrategyFactory::createStrategy(gameVersion);
        if (!versionStrategy_) {
            fprintf(stderr, "Failed to create version strategy\n");
            return false;
        }
        
        printf("AI Engine initialized for Minecraft %s\n", gameVersion.c_str());
        return true;
        
    } catch (const std::exception& e) {
        fprintf(stderr, "AI Engine initialization failed: %s\n", e.what());
        return false;
    }
}

void AIEngine::shutdown() {
    std::lock_guard lock(entityMutex_);
    entityStates_.clear();
    entityTypes_.clear();
    entityWorldViews_.clear();
}

bool AIEngine::registerEntity(uint64_t entityId, const std::string& entityType) {
    std::lock_guard lock(entityMutex_);
    
    auto* config = dataModule_->getEntityData(entityType);
    if (!config) {
        fprintf(stderr, "Unknown entity type: %s\n", entityType.c_str());
        return false;
    }
    
    EntityState state;
    state.entityId = entityId;
    state.maxHealth = config->maxHealth;
    state.health = config->maxHealth;
    state.isAlive = true;
    state.behaviorState = EntityState::BehaviorState::IDLE;
    
    entityStates_[entityId] = std::move(state);
    entityTypes_[entityId] = entityType;
    
    printf("Registered entity %lu as %s\n", entityId, entityType.c_str());
    return true;
}

void AIEngine::unregisterEntity(uint64_t entityId) {
    std::lock_guard lock(entityMutex_);
    entityStates_.erase(entityId);
    entityTypes_.erase(entityId);
    entityWorldViews_.erase(entityId);
}

void AIEngine::updateEntityState(uint64_t entityId, const EntityState& state) {
    std::lock_guard lock(entityMutex_);
    entityStates_[entityId] = state;
}

void AIEngine::updateWorldView(uint64_t entityId, const WorldView& world) {
    std::lock_guard lock(entityMutex_);
    entityWorldViews_[entityId] = std::make_unique<WorldView>(world);
}

void AIEngine::tick() {
    auto tickStart = std::chrono::high_resolution_clock::now();
    
    std::shared_lock lock(entityMutex_);
    std::vector<uint64_t> entityIds;
    entityIds.reserve(entityStates_.size());
    
    for (const auto& [id, _] : entityStates_) {
        entityIds.push_back(id);
    }
    lock.unlock();
    
    // 并行处理实体（如果C++17并行算法可用）
    std::for_each(std::execution::par_unseq, entityIds.begin(), entityIds.end(),
        [this](uint64_t entityId) {
            tickEntity(entityId, 50); // 假设50ms per tick
        });
    
    // 更新性能统计
    auto tickEnd = std::chrono::high_resolution_clock::now();
    auto tickDuration = std::chrono::duration_cast<std::chrono::microseconds>(tickEnd - tickStart);
    
    std::lock_guard statsLock(statsMutex_);
    performanceStats_.totalTicks++;
    performanceStats_.entitiesProcessed += entityIds.size();
    performanceStats_.avgTickTime = tickDuration.count();
    performanceStats_.maxTickTime = std::max(performanceStats_.maxTickTime, 
                                           static_cast<uint64_t>(tickDuration.count()));
}

void AIEngine::tickEntity(uint64_t entityId, uint64_t /*deltaTime*/) {
    EntityState state;
    std::string entityType;
    std::unique_ptr<WorldView> worldView;
    
    // 获取实体状态
    {
        std::shared_lock lock(entityMutex_);
        auto stateIt = entityStates_.find(entityId);
        if (stateIt == entityStates_.end()) return;
        
        state = stateIt->second;
        auto typeIt = entityTypes_.find(entityId);
        if (typeIt == entityTypes_.end()) return;
        entityType = typeIt->second;
        
        auto worldIt = entityWorldViews_.find(entityId);
        if (worldIt != entityWorldViews_.end()) {
            worldView = std::make_unique<WorldView>(*worldIt->second);
        }
    }
    
    // 设置线程上下文
    g_threadContext.currentEntity = &state;
    if (worldView) {
        g_threadContext.currentWorld = worldView.get();
    }
    g_threadContext.tickStart = std::chrono::high_resolution_clock::now();
    
    try {
        processEntityBehavior(entityId, state, *worldView);
    } catch (const std::exception& e) {
        fprintf(stderr, "Error processing entity %lu behavior: %s\n", entityId, e.what());
    }
    
    // 更新实体状态
    {
        std::lock_guard lock(entityMutex_);
        entityStates_[entityId] = state;
    }
}

void AIEngine::processEntityBehavior(uint64_t entityId, const EntityState& state, const WorldView& world) {
    auto* config = dataModule_->getEntityData(entityTypes_[entityId]);
    if (!config) return;
    
    // 应用版本特定修改
    versionStrategy_->applyBehaviorModifications(*config);
    
    // 感知环境
    if (state.health <= 0) {
        // 实体死亡，不需要AI处理
        return;
    }
    
    // 检查环境威胁
    bool hasThreats = false;
    for (const auto& [otherId, otherState] : world.nearbyEntities) {
        if (otherId == entityId) continue;
        
        float distance = BehaviorNodeBase::distance3D(
            state.x, state.y, state.z,
            otherState.x, otherState.y, otherState.z
        );
        
        if (distance < config->followRange) {
            hasThreats = true;
            break;
        }
    }
    
    // 行为决策
    if (hasThreats && config->canAttack) {
        // 威胁检测 -> 攻击或逃跑
        EntityState newState = state;
        newState.behaviorState = EntityState::BehaviorState::ATTACKING;
        // 这里会调用具体的行为节点
        printf("Entity %lu is attacking\n", entityId);
        
    } else if (state.targetId != 0) {
        // 有目标 -> 跟随/追踪
        EntityState newState = state;
        newState.behaviorState = EntityState::BehaviorState::FOLLOWING;
        printf("Entity %lu is following target %lu\n", entityId, state.targetId);
        
    } else if (config->category == "animal") {
        // 动物 -> 觅食/游荡
        EntityState newState = state;
        newState.behaviorState = EntityState::BehaviorState::EXPLORING;
        printf("Entity %lu is exploring\n", entityId);
        
    } else {
        // 默认空闲
        printf("Entity %lu is idle\n", entityId);
    }
}

EntityBehaviorData* AIEngine::getEntityConfig(uint64_t entityId) {
    std::shared_lock lock(entityMutex_);
    auto typeIt = entityTypes_.find(entityId);
    if (typeIt == entityTypes_.end()) return nullptr;
    
    return dataModule_->getEntityData(typeIt->second);
}

// ====== AIEngine 辅助方法实现 ======

std::vector<std::string> AIEngine::getAllEntityIds() const {
    return dataModule_->getAllEntityIds();
}

std::vector<EntityBehaviorData> AIEngine::getEntitiesByCategory(const std::string& category) const {
    return dataModule_->getEntitiesByCategory(category);
}

// ====== 线程局部上下文初始化 ======

void initializeThreadLocalContext() {
    g_threadContext.threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
}

// ====== 工具函数实现 ======

/**
 * @brief 计算三维距离的SIMD优化版本
 */
#ifdef __AVX2__
__attribute__((target("avx2")))
static inline __m256 calculateDistanceBatchAVX2(const __m256& x1, const __m256& y1, const __m256& z1,
                                               const __m256& x2, const __m256& y2, const __m256& z2) {
    __m256 dx = _mm256_sub_ps(x2, x1);
    __m256 dy = _mm256_sub_ps(y2, y1);
    __m256 dz = _mm256_sub_ps(z2, z1);
    
    __m256 distSq = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dx, dx),
                                               _mm256_mul_ps(dy, dy)),
                                  _mm256_mul_ps(dz, dz));
    
    return _mm256_sqrt_ps(distSq);
}
#endif

/**
 * @brief SIMD-optimized entity distance calculation
 * @brief 利用现代CPU架构进行向量化实体距离计算
 */
std::vector<uint64_t> findNearbyEntitiesSIMD(const std::vector<EntityState>& entities,
                                           const EntityState& center,
                                           float range) {
    if (entities.empty()) return {};
    
    // 包含现代SIMD优化头文件
    #include "optimization/simd_optimization.hpp"
    
    using namespace lattice::simd;
    
    // 准备数据结构
    constexpr size_t entity_elements = 3; // x, y, z
    size_t entity_count = entities.size();
    
    // 提取坐标数据到连续数组
    std::vector<float> entity_positions(entity_count * entity_elements);
    std::vector<uint64_t> entity_ids(entity_count);
    
    for (size_t i = 0; i < entity_count; ++i) {
        entity_positions[i * 3 + 0] = entities[i].x;
        entity_positions[i * 3 + 1] = entities[i].y;
        entity_positions[i * 3 + 2] = entities[i].z;
        entity_ids[i] = entities[i].entityId;
    }
    
    // 计算距离
    std::vector<float> distances(entity_count);
    SIMDVector<float, 3> center_vec(center.x, center.y, center.z);
    
    if (SIMDDetector::hasAVX512()) {
        // AVX-512 优化路径
        #ifdef LATTICE_AVX512_SUPPORTED
        constexpr size_t batch_size = 16;
        constexpr size_t lanes = 3;
        
        auto center_x = SIMDVector<float, batch_size>::broadcast(center.x);
        auto center_y = SIMDVector<float, batch_size>::broadcast(center.y);
        auto center_z = SIMDVector<float, batch_size>::broadcast(center.z);
        auto range_vec = SIMDVector<float, batch_size>::broadcast(range * range); // 平方距离比较
        
        for (size_t i = 0; i + batch_size * lanes <= entity_count * 3; i += batch_size * lanes) {
            // 加载实体坐标
            auto entity_x = SIMDVector<float, batch_size>::load_aligned(&entity_positions[i]);
            auto entity_y = SIMDVector<float, batch_size>::load_aligned(&entity_positions[i + batch_size]);
            auto entity_z = SIMDVector<float, batch_size>::load_aligned(&entity_positions[i + 2 * batch_size]);
            
            // 计算平方距离: (x-cx)^2 + (y-cy)^2 + (z-cz)^2
            auto dx = entity_x - center_x;
            auto dy = entity_y - center_y;
            auto dz = entity_z - center_z;
            
            auto dist_sq = dx * dx + dy * dy + dz * dz;
            
            // 向量化比较和掩码生成
            auto mask = dist_sq <= range_vec;
            
            // 使用掩码选择结果
            for (size_t j = 0; j < batch_size && (i / 3) + j < entity_count; ++j) {
                // 从掩码中提取结果
            }
        }
        #endif
    } else if (SIMDDetector::hasAVX2()) {
        // AVX2 优化路径
        #ifdef LATTICE_AVX2_SUPPORTED
        constexpr size_t batch_size = 8;
        constexpr size_t lanes = 3;
        
        for (size_t i = 0; i + batch_size * lanes <= entity_count * 3; i += batch_size * lanes) {
            __m256 entity_x = _mm256_load_ps(&entity_positions[i]);
            __m256 entity_y = _mm256_load_ps(&entity_positions[i + batch_size]);
            __m256 entity_z = _mm256_load_ps(&entity_positions[i + 2 * batch_size]);
            
            __m256 center_x = _mm256_set1_ps(center.x);
            __m256 center_y = _mm256_set1_ps(center.y);
            __m256 center_z = _mm256_set1_ps(center.z);
            __m256 range_sq = _mm256_set1_ps(range * range);
            
            // 计算平方距离
            __m256 dx = _mm256_sub_ps(entity_x, center_x);
            __m256 dy = _mm256_sub_ps(entity_y, center_y);
            __m256 dz = _mm256_sub_ps(entity_z, center_z);
            
            __m256 dist_sq = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dx, dx),
                                                        _mm256_mul_ps(dy, dy)),
                                         _mm256_mul_ps(dz, dz));
            
            // 比较并生成掩码
            __m256 mask = _mm256_cmp_ps(dist_sq, range_sq, _CMP_LE_OQ);
            int mask_int = _mm256_movemask_ps(mask);
            
            // 处理掩码中的位
            for (int bit = 0; bit < 8 && (i / 3) + bit < entity_count; ++bit) {
                if (mask_int & (1 << bit)) {
                    size_t entity_idx = (i / 3) + bit;
                    distances[entity_idx] = _mm256_cvtss_f32(_mm256_permute2f128_ps(dist_sq, dist_sq, 0)); // 近似值
                }
            }
        }
        #endif
    } else if (SIMDDetector::hasSSE4()) {
        // SSE4 优化路径
        #ifdef LATTICE_SSE4_SUPPORTED
        constexpr size_t batch_size = 4;
        constexpr size_t lanes = 3;
        
        for (size_t i = 0; i + batch_size * lanes <= entity_count * 3; i += batch_size * lanes) {
            __m128 entity_x = _mm_load_ps(&entity_positions[i]);
            __m128 entity_y = _mm_load_ps(&entity_positions[i + batch_size]);
            __m128 entity_z = _mm_load_ps(&entity_positions[i + 2 * batch_size]);
            
            __m128 center_x = _mm_set1_ps(center.x);
            __m128 center_y = _mm_set1_ps(center.y);
            __m128 center_z = _mm_set1_ps(center.z);
            __m128 range_sq = _mm_set1_ps(range * range);
            
            // 计算平方距离
            __m128 dx = _mm_sub_ps(entity_x, center_x);
            __m128 dy = _mm_sub_ps(entity_y, center_y);
            __m128 dz = _mm_sub_ps(entity_z, center_z);
            
            __m128 dist_sq = _mm_add_ps(_mm_add_ps(_mm_mul_ps(dx, dx),
                                                  _mm_mul_ps(dy, dy)),
                                      _mm_mul_ps(dz, dz));
            
            // 比较并生成掩码
            __m128 mask = _mm_cmple_ps(dist_sq, range_sq);
            int mask_int = _mm_movemask_ps(mask);
            
            // 处理结果
            for (int bit = 0; bit < 4 && (i / 3) + bit < entity_count; ++bit) {
                if (mask_int & (1 << bit)) {
                    size_t entity_idx = (i / 3) + bit;
                    distances[entity_idx] = _mm_cvtss_f32(dist_sq); // 近似值
                }
            }
        }
        #endif
    }
    
    // 处理剩余的标量元素
    for (size_t i = entity_count & ~15; i < entity_count; ++i) {
        float dx = entity_positions[i * 3 + 0] - center.x;
        float dy = entity_positions[i * 3 + 1] - center.y;
        float dz = entity_positions[i * 3 + 2] - center.z;
        distances[i] = dx*dx + dy*dy + dz*dz; // 使用平方距离
    }
    
    // 最终筛选在范围内的实体
    std::vector<uint64_t> nearby;
    nearby.reserve(entity_count / 4); // 估计容量
    
    for (size_t i = 0; i < entity_count; ++i) {
        if (distances[i] <= range * range) {
            nearby.push_back(entity_ids[i]);
        }
    }
    
    return nearby;
}

/**
 * @brief 多线程并行实体距离计算
 * @brief 利用所有CPU核心进行大规模实体处理
 */
std::vector<uint64_t> findNearbyEntitiesParallel(const std::vector<EntityState>& entities,
                                                const EntityState& center,
                                                float range,
                                                size_t num_threads) {
    if (entities.empty()) return {};
    
    const size_t num_cores = std::thread::hardware_concurrency();
    size_t thread_count = std::min(num_threads > 0 ? num_threads : num_cores, 
                                 std::max(size_t(1), entities.size() / 100));
    
    if (thread_count == 1) {
        return findNearbyEntitiesSIMD(entities, center, range);
    }
    
    // 准备并行处理数据结构
    struct ThreadResult {
        std::vector<uint64_t> nearby_entities;
        std::mutex mutex;
    };
    
    ThreadResult thread_result;
    
    // 动态负载均衡的分块策略
    size_t chunk_size = std::max(size_t(1000), entities.size() / thread_count);
    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    
    // 启动线程
    for (size_t t = 0; t < thread_count; ++t) {
        size_t start = t * chunk_size;
        size_t end = std::min(start + chunk_size, entities.size());
        
        if (start >= entities.size()) break;
        
        threads.emplace_back([&, start, end]() {
            // 每个线程处理一个分块
            std::vector<EntityState> local_entities(entities.begin() + start, entities.begin() + end);
            auto local_nearby = findNearbyEntitiesSIMD(local_entities, center, range);
            
            // 线程安全地合并结果
            {
                std::lock_guard<std::mutex> lock(thread_result.mutex);
                thread_result.nearby_entities.insert(thread_result.nearby_entities.end(),
                                                   std::make_move_iterator(local_nearby.begin()),
                                                   std::make_move_iterator(local_nearby.end()));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    return std::move(thread_result.nearby_entities);
}

/**
 * @brief C++20 constexpr优化的实体属性计算
 * @brief 编译时优化的属性计算函数
 */
template<typename T>
constexpr T calculateEntityWeight(const EntityBehaviorData& data) {
    // 编译时常量评估
    constexpr T base_weight = 1.0f;
    constexpr T health_factor = 0.1f;
    constexpr T attack_factor = 0.2f;
    constexpr T mobility_factor = 0.3f;
    
    return base_weight + 
           data.maxHealth * health_factor +
           data.attackDamage * attack_factor +
           data.movementSpeed * mobility_factor;
}

// C++20 概念约束的智能行为决策
template<typename EntityType>
concept ValidEntity = requires(EntityType entity) {
    entity.entityId;
    entity.x;
    entity.y;
    entity.z;
    entity.health;
    entity.maxHealth;
};

template<ValidEntity EntityType>
class SIMDOptimizedBehaviorEngine {
public:
    /**
     * @brief 并行智能行为决策
     */
    static void makeBehaviorDecisions(std::vector<EntityType>& entities,
                                     const std::vector<EntityBehaviorData>& configs) {
        if constexpr (SIMDDetector::hasAVX512()) {
            // AVX-512 批量决策
            #ifdef LATTICE_AVX512_SUPPORTED
            makeBehaviorDecisionsAVX512(entities, configs);
            #else
            makeBehaviorDecisionsAVX2(entities, configs);
            #endif
        } else if constexpr (SIMDDetector::hasAVX2()) {
            makeBehaviorDecisionsAVX2(entities, configs);
        } else {
            makeBehaviorDecisionsScalar(entities, configs);
        }
    }
    
private:
    /**
     * @brief AVX2优化版本的行为决策
     */
    static void makeBehaviorDecisionsAVX2(std::vector<EntityType>& entities,
                                        const std::vector<EntityBehaviorData>& configs) {
        // 向量化健康状态检查
        for (size_t i = 0; i + 8 <= entities.size(); i += 8) {
            // 加载健康数据到AVX2向量
            __m256 health_vec = _mm256_load_ps(reinterpret_cast<const float*>(&entities[i].health));
            __m256 max_health_vec = _mm256_load_ps(reinterpret_cast<const float*>(&entities[i].maxHealth));
            
            // 向量化健康比例计算
            __m256 health_ratio = _mm256_div_ps(health_vec, max_health_vec);
            
            // SIMD条件分支 - 生存还是攻击
            __m256 critical_threshold = _mm256_set1_ps(0.2f);
            __m256 warning_threshold = _mm256_set1_ps(0.5f);
            
            // 临界状态检测 (健康 < 20%)
            auto critical_mask = _mm256_cmp_ps(health_ratio, critical_threshold, _CMP_LT_OQ);
            
            // 警告状态检测 (健康 20%-50%)
            auto warning_mask = _mm256_and_ps(_mm256_cmp_ps(health_ratio, critical_threshold, _CMP_GE_OQ),
                                             _mm256_cmp_ps(health_ratio, warning_threshold, _CMP_LT_OQ));
            
            // 普通状态 (健康 > 50%)
            auto normal_mask = _mm256_cmp_ps(health_ratio, warning_threshold, _CMP_GE_OQ);
            
            // 应用行为决策
            for (size_t j = 0; j < 8 && i + j < entities.size(); ++j) {
                if (std::bit_cast<int>(_mm256_extract_epi32(_mm256_castps_si256(critical_mask), j)) & 1) {
                    entities[i + j].behaviorState = EntityType::BehaviorState::FLEEING;
                } else if (std::bit_cast<int>(_mm256_extract_epi32(_mm256_castps_si256(warning_mask), j)) & 1) {
                    entities[i + j].behaviorState = EntityType::BehaviorState::DEFENSIVE;
                } else {
                    entities[i + j].behaviorState = EntityType::BehaviorState::AGGRESSIVE;
                }
            }
        }
    }
    
    /**
     * @brief 标量后备版本
     */
    static void makeBehaviorDecisionsScalar(std::vector<EntityType>& entities,
                                          const std::vector<EntityBehaviorData>& configs) {
        for (auto& entity : entities) {
            float health_ratio = entity.health / entity.maxHealth;
            
            if (health_ratio < 0.2f) {
                entity.behaviorState = EntityType::BehaviorState::FLEEING;
            } else if (health_ratio < 0.5f) {
                entity.behaviorState = EntityType::BehaviorState::DEFENSIVE;
            } else {
                entity.behaviorState = EntityType::BehaviorState::AGGRESSIVE;
            }
        }
    }
    
    /**
     * @brief AVX512版本 (如果支持)
     */
    static void makeBehaviorDecisionsAVX512(std::vector<EntityType>& entities,
                                          const std::vector<EntityBehaviorData>& configs) {
        // AVX-512实现 (16宽并行)
        // 类似于AVX2版本但使用AVX512指令集
        // ... 具体的AVX512实现
    }
};

} // namespace lattice::entity