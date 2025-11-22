#pragma once

#include "biological_ai.hpp"
#include "fmt_wrapper.hpp"
#include <cmath>
#include <algorithm>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <memory_resource>
#include <chrono>

namespace lattice::entity {

// ====== 内存池和资源管理 ======

/**
 * @brief 路径节点内存池
 */
using PathNodePool = std::pmr::memory_resource;
using PathNode = struct PathNode {
    float x, y, z;
    PathNode* parent;
    float gCost, hCost, fCost;
    bool walkable;
    
    PathNode(float x, float y, float z, PathNode* parent = nullptr)
        : x(x), y(y), z(z), parent(parent), gCost(0), hCost(0), fCost(0), walkable(true) {}
    
    auto operator<=>(const PathNode& other) const {
        return fCost <=> other.fCost;
    }
};

// ====== 行为节点实现 ======

/**
 * @brief 注视玩家节点
 */
class LookAtPlayerNode : public BehaviorNodeBase {
public:
    bool tick(EntityState& state, const WorldView& world) override {
        // 查找最近的玩家
        float closestDistance = std::numeric_limits<float>::max();
        uint64_t closestPlayer = 0;
        
        for (const auto& [entityId, entityState] : world.nearbyEntities) {
            // 简化检查：假设玩家ID < 1000
            if (entityId < 1000) {
                float distance = distance3D(state.x, state.y, state.z,
                                          entityState.x, entityState.y, entityState.z);
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestPlayer = entityId;
                }
            }
        }
        
        if (closestPlayer != 0) {
            state.targetId = closestPlayer;
            state.targetDistance = closestDistance;
            return true;
        }
        
        return false;
    }
    
    void reset() override {}
    std::string getNodeType() const override { return "LookAtPlayer"; }
};

/**
 * @brief 近战攻击节点
 */
class MeleeAttackNode : public BehaviorNodeBase {
public:
    MeleeAttackNode(float attackRange = 1.5f, float attackDamage = 3.0f)
        : attackRange_(attackRange), attackDamage_(attackDamage) {}
    
    bool tick(EntityState& state, const WorldView& world) override {
        if (state.targetId == 0) return false;
        
        // 获取目标状态
        auto targetIt = world.nearbyEntities.find(state.targetId);
        if (targetIt == world.nearbyEntities.end()) return false;
        
        const auto& target = targetIt->second;
        float distance = distance3D(state.x, state.y, state.z,
                                  target.x, target.y, target.z);
        
        // 检查攻击范围
        if (distance <= attackRange_) {
            // 检查攻击冷却
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();
            
            if (now - state.lastAttackTime >= 800) { // 0.8秒攻击间隔
                state.lastAttackTime = now;
                
                // 执行攻击（这里只是模拟）
                fmt::print("Entity %llu attacks entity %llu for %f damage\n", 
                          state.entityId, state.targetId, attackDamage_);
                return true;
            }
        } else {
            // 距离太远，跟随目标
            state.behaviorState = EntityState::BehaviorState::FOLLOWING;
        }
        
        return false;
    }
    
    void reset() override {}
    std::string getNodeType() const override { return "MeleeAttack"; }
    
private:
    float attackRange_;
    float attackDamage_;
};

/**
 * @brief 逃避实体节点
 */
class AvoidEntityNode : public BehaviorNodeBase {
public:
    AvoidEntityNode(const std::unordered_set<std::string>& threats)
        : threats_(threats) {}
    
    bool tick(EntityState& state, const WorldView& world) override {
        bool hasThreat = false;
        float closestThreat = std::numeric_limits<float>::max();
        
        for (const auto& [entityId, entityState] : world.nearbyEntities) {
            if (entityId == state.entityId) continue;
            
            float distance = distance3D(state.x, state.y, state.z,
                                      entityState.x, entityState.y, entityState.z);
            
            // 检查威胁类型
            if (threats_.find("fire") != threats_.end() && entityState.isOnFire) {
                hasThreat = true;
                closestThreat = std::min(closestThreat, distance);
            }
            
            if (threats_.find("player") != threats_.end() && entityId < 1000) {
                hasThreat = true;
                closestThreat = std::min(closestThreat, distance);
            }
        }
        
        if (hasThreat && closestThreat < 8.0f) {
            state.behaviorState = EntityState::BehaviorState::FLEEING;
            return true;
        }
        
        return false;
    }
    
    void fleeFromThreat(EntityState& state, float threatX, float /*threatY*/, float threatZ) {
        // 计算逃离方向（远离威胁）
        float dx = state.x - threatX;
        float dz = state.z - threatZ;
        float distance = std::sqrt(dx * dx + dz * dz);
        
        if (distance > 0.1f) {
            // 归一化并设置逃跑速度
            float fleeSpeed = state.movementSpeed * 1.5f; // 逃跑速度比正常移动快
            state.velocityX = (dx / distance) * fleeSpeed;
            state.velocityZ = (dz / distance) * fleeSpeed;
        }
    }
    
    void reset() override {}
    std::string getNodeType() const override { return "AvoidEntity"; }
    
private:
    std::unordered_set<std::string> threats_;
};

/**
 * @brief A*寻路节点
 */
class PathfindingNode : public BehaviorNodeBase {
public:
    PathfindingNode(float stepHeight = 0.6f, bool allowDiagonal = false)
        : stepHeight_(stepHeight), allowDiagonal_(allowDiagonal) {}
    
    bool tick(EntityState& state, const WorldView& world) override {
        if (state.targetId == 0) return false;
        
        auto targetIt = world.nearbyEntities.find(state.targetId);
        if (targetIt == world.nearbyEntities.end()) return false;
        
        const auto& target = targetIt->second;
        std::vector<Position> path;
        
        if (calculatePathAStar(state, target, world, path)) {
            // 沿路径移动（简化实现）
            if (!path.empty()) {
                auto& nextPos = path[0];
                moveTowardsTarget(state, nextPos);
                fmt::print("Entity %llu moving towards target via path\n", state.entityId);
            }
            return true;
        }
        
        return false;
    }
    
    void reset() override {}
    std::string getNodeType() const override { return "Pathfinding"; }
    
private:
    struct Position {
        float x, y, z;
        auto operator<=>(const Position& other) const = default;
    };
    
    float stepHeight_;
    bool allowDiagonal_;
    
    bool calculatePathAStar(const EntityState& start, const EntityState& target,
                           const WorldView& /*world*/, std::vector<Position>& path) {
        // A*寻路算法实现
        auto openSet = std::priority_queue<PathNode*, std::vector<PathNode*>, 
                                          decltype([](PathNode* a, PathNode* b) { return a->fCost > b->fCost; })>();
        
        auto startNode = new PathNode(start.x, start.y, start.z);
        auto targetNode = new PathNode(target.x, target.y, target.z);
        
        startNode->gCost = 0;
        startNode->hCost = heuristic(startNode, targetNode);
        startNode->fCost = startNode->gCost + startNode->hCost;
        
        openSet.push(startNode);
        
        // 简化的A*实现（实际应该使用网格系统）
        // 这里只是演示概念
        path.clear();
        path.push_back({target.x, target.y, target.z});
        
        delete startNode;
        delete targetNode;
        return true;
    }
    
    float heuristic(PathNode* a, PathNode* b) {
        return distance3D(a->x, a->y, a->z, b->x, b->y, b->z);
    }
    
    void moveTowardsTarget(EntityState& state, const Position& target) {
        float dx = target.x - state.x;
        float dz = target.z - state.z;
        float length = std::sqrt(dx * dx + dz * dz);
        
        if (length > 0.1f) {
            float movementSpeed = 4.0f; // 默认移动速度
            state.velocityX = (dx / length) * movementSpeed;
            state.velocityZ = (dz / length) * movementSpeed;
        }
    }
};

/**
 * @brief 水中导航节点
 */
class WaterNavigationNode : public BehaviorNodeBase {
public:
    WaterNavigationNode(float swimSpeed = 0.2f) 
        : swimSpeed_(swimSpeed) {}
    
    bool tick(EntityState& state, const WorldView& world) override {
        if (!state.isInWater) return false;
        
        // 水中生物特殊行为
        if (state.isOnFire) {
            // 寻找水源灭火
            auto waterPos = findNearestWater(world, state);
            if (waterPos.has_value()) {
                moveTowardsTarget(state, waterPos.value());
                state.behaviorState = EntityState::BehaviorState::FLEEING;
                return true;
            }
        }
        
        // 鱼类游泳模式
        if (std::rand() % 100 < 5) { // 5%概率改变方向
            state.velocityX = (std::rand() % 100 - 50) / 100.0f * swimSpeed_;
            state.velocityZ = (std::rand() % 100 - 50) / 100.0f * swimSpeed_;
            state.velocityY = (std::rand() % 100 - 50) / 100.0f * (swimSpeed_ * 0.5f);
        }
        
        return true;
    }
    
    void reset() override {}
    std::string getNodeType() const override { return "WaterNavigation"; }
    
private:
    float swimSpeed_;
    
    struct Position { float x, y, z; };
    
    std::optional<Position> findNearestWater(const WorldView& world, const EntityState& state) {
        // 简化实现：假设附近有水
        for (const auto& [pos, block] : world.nearbyBlocks) {
            if (block.type.find("water") != std::string::npos) {
                // 解析位置（简化）
                return Position{state.x + 1, state.y, state.z};
            }
        }
        return std::nullopt;
    }
    
    void moveTowardsTarget(EntityState& state, const Position& target) {
        float dx = target.x - state.x;
        float dy = target.y - state.y;
        float dz = target.z - state.z;
        float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        
        if (length > 0.1f) {
            state.velocityX = (dx / length) * swimSpeed_;
            state.velocityY = (dy / length) * (swimSpeed_ * 0.8f);
            state.velocityZ = (dz / length) * swimSpeed_;
        }
    }
};

/**
 * @brief 飞行行为节点
 */
class FlyingBehaviorNode : public BehaviorNodeBase {
public:
    FlyingBehaviorNode(float flightSpeed = 0.4f, float maxAltitude = 32.0f)
        : flightSpeed_(flightSpeed), maxAltitude_(maxAltitude) {}
    
    bool tick(EntityState& state, const WorldView& /*world*/) override {
        // 只有会飞的实体才能使用此行为
        if (state.y > maxAltitude_) {
            // 下降到安全高度
            state.velocityY = -flightSpeed_;
            return true;
        }
        
        // 蝙蝠行为：随机飞行模式
        if (std::rand() % 100 < 3) { // 3%概率改变高度
            state.velocityY = (std::rand() % 100 - 50) / 100.0f * flightSpeed_;
        }
        
        // 水平移动
        if (std::rand() % 100 < 10) { // 10%概率改变方向
            state.velocityX = (std::rand() % 100 - 50) / 100.0f * flightSpeed_;
            state.velocityZ = (std::rand() % 100 - 50) / 100.0f * flightSpeed_;
        }
        
        // 夜间活跃
        if (state.lightLevel < 4.0f) {
            state.velocityX *= 1.5f;
            state.velocityZ *= 1.5f;
        }
        
        return true;
    }
    
    void reset() override {}
    std::string getNodeType() const override { return "FlyingBehavior"; }
    
private:
    float flightSpeed_;
    float maxAltitude_;
};

/**
 * @brief 觅食节点
 */
class ForagingNode : public BehaviorNodeBase {
public:
    ForagingNode(const std::vector<std::string>& preferredBlocks)
        : preferredBlocks_(preferredBlocks) {}
    
    bool tick(EntityState& state, const WorldView& world) override {
        // 寻找食物或资源方块
        auto targetBlock = findPreferredBlock(world, state);
        if (targetBlock.has_value()) {
            moveTowardsBlock(state, targetBlock.value());
            return true;
        }
        
        // 随机游荡
        if (std::rand() % 100 < 5) { // 5%概率改变方向
            state.velocityX = (std::rand() % 100 - 50) / 100.0f * 0.1f;
            state.velocityZ = (std::rand() % 100 - 50) / 100.0f * 0.1f;
        }
        
        return false;
    }
    
    void reset() override {}
    std::string getNodeType() const override { return "Foraging"; }
    
private:
    std::vector<std::string> preferredBlocks_;
    
    struct BlockPos { std::string type; float x, y, z; };
    
    std::optional<BlockPos> findPreferredBlock(const WorldView& world, const EntityState& state) {
        float closestDistance = std::numeric_limits<float>::max();
        std::optional<BlockPos> closestBlock;
        
        for (const auto& [posKey, block] : world.nearbyBlocks) {
            // 检查是否是首选方块类型
            bool isPreferred = false;
            for (const auto& preferred : preferredBlocks_) {
                if (block.type.find(preferred) != std::string::npos) {
                    isPreferred = true;
                    break;
                }
            }
            
            if (isPreferred) {
                // 解析位置（简化）
                float blockX = state.x + (std::rand() % 10 - 5);
                float blockZ = state.z + (std::rand() % 10 - 5);
                float distance = distance3D(state.x, state.y, state.z,
                                          blockX, state.y, blockZ);
                
                if (distance < closestDistance && distance < 10.0f) {
                    closestDistance = distance;
                    closestBlock = BlockPos{block.type, blockX, state.y, blockZ};
                }
            }
        }
        
        return closestBlock;
    }
    
    void moveTowardsBlock(EntityState& state, const BlockPos& block) {
        float dx = block.x - state.x;
        float dz = block.z - state.z;
        float length = std::sqrt(dx * dx + dz * dz);
        
        if (length > 0.1f) {
            state.velocityX = (dx / length) * 0.1f;
            state.velocityZ = (dz / length) * 0.1f;
            state.behaviorState = EntityState::BehaviorState::EXPLORING;
        }
    }
};

/**
 * @brief 环境适应节点
 */
class EnvironmentAdaptationNode : public BehaviorNodeBase {
public:
    EnvironmentAdaptationNode(const EntityBehaviorData& config)
        : config_(config) {}
    
    bool tick(EntityState& state, const WorldView& world) override {
        bool adapted = false;
        
        // 光照适应
        if (config_.burnsInSunlight && state.lightLevel > 7.0f && !state.isInWater) {
            // 寻找阴凉处
            auto shadePos = findShade(world, state);
            if (shadePos.has_value()) {
                moveTowardsShade(state, shadePos.value());
                adapted = true;
            }
        }
        
        // 水适应
        if (config_.avoidsWater && state.isInWater) {
            // 寻找干燥地面
            auto dryPos = findDryLand(world, state);
            if (dryPos.has_value()) {
                moveTowardsDryLand(state, dryPos.value());
                adapted = true;
            }
        }
        
        // 岩浆适应
        if (state.isInLava) {
            auto waterPos = findNearestWater(world, state);
            if (waterPos.has_value()) {
                moveTowardsTarget(state, waterPos.value());
                state.behaviorState = EntityState::BehaviorState::FLEEING;
                adapted = true;
            }
        }
        
        return adapted;
    }
    
    void reset() override {}
    std::string getNodeType() const override { return "EnvironmentAdaptation"; }
    
private:
    EntityBehaviorData config_;
    
    struct Position { float x, y, z; };
    
    std::optional<Position> findShade(const WorldView& world, const EntityState& state) {
        // 寻找较暗的区域
        for (const auto& [posKey, block] : world.nearbyBlocks) {
            if (!block.transparent) { // 不透明方块可以提供阴影
                return Position{state.x + 1, state.y, state.z + 1};
            }
        }
        return std::nullopt;
    }
    
    std::optional<Position> findDryLand(const WorldView& world, const EntityState& state) {
        // 寻找非液体方块
        for (const auto& [posKey, block] : world.nearbyBlocks) {
            if (!block.liquid) {
                return Position{state.x + 1, state.y, state.z + 1};
            }
        }
        return std::nullopt;
    }
    
    std::optional<Position> findNearestWater(const WorldView& world, const EntityState& state) {
        for (const auto& [posKey, block] : world.nearbyBlocks) {
            if (block.type.find("water") != std::string::npos) {
                return Position{state.x + 1, state.y, state.z + 1};
            }
        }
        return std::nullopt;
    }
    
    void moveTowardsShade(EntityState& state, const Position& target) {
        // 移动到阴影区域
        moveTowardsTarget(state, target);
    }
    
    void moveTowardsDryLand(EntityState& state, const Position& target) {
        // 移动到干燥地面
        moveTowardsTarget(state, target);
    }
    
    void moveTowardsTarget(EntityState& state, const Position& target) {
        float dx = target.x - state.x;
        float dz = target.z - state.z;
        float length = std::sqrt(dx * dx + dz * dz);
        
        if (length > 0.1f) {
            state.velocityX = (dx / length) * 0.15f;
            state.velocityZ = (dz / length) * 0.15f;
        }
    }
};

// ====== 行为树组合器 ======

/**
 * @brief 优先级选择器 - 依次尝试子节点，直到一个成功
 */
class PrioritySelector : public BehaviorNodeBase {
public:
    void addChild(std::unique_ptr<BehaviorNodeBase> child) {
        children_.push_back(std::move(child));
    }
    
    bool tick(EntityState& state, const WorldView& world) override {
        for (auto& child : children_) {
            if (child->tick(state, world)) {
                return true;
            }
        }
        return false;
    }
    
    void reset() override {
        for (auto& child : children_) {
            child->reset();
        }
    }
    
    std::string getNodeType() const override { return "PrioritySelector"; }
    
private:
    std::vector<std::unique_ptr<BehaviorNodeBase>> children_;
};

/**
 * @brief 序列节点 - 按顺序执行所有子节点
 */
class SequenceNode : public BehaviorNodeBase {
public:
    void addChild(std::unique_ptr<BehaviorNodeBase> child) {
        children_.push_back(std::move(child));
    }
    
    bool tick(EntityState& state, const WorldView& world) override {
        for (auto& child : children_) {
            if (!child->tick(state, world)) {
                return false; // 任何一个失败，整个序列失败
            }
        }
        return true; // 所有子节点都成功
    }
    
    void reset() override {
        currentChild_ = 0;
        for (auto& child : children_) {
            child->reset();
        }
    }
    
    std::string getNodeType() const override { return "Sequence"; }
    
private:
    std::vector<std::unique_ptr<BehaviorNodeBase>> children_;
    size_t currentChild_{0};
};

/**
 * @brief 并行节点 - 同时执行所有子节点
 */
class ParallelNode : public BehaviorNodeBase {
public:
    ParallelNode(int requiredSuccesses = 1) 
        : requiredSuccesses_(requiredSuccesses) {}
    
    void addChild(std::unique_ptr<BehaviorNodeBase> child) {
        children_.push_back(std::move(child));
    }
    
    bool tick(EntityState& state, const WorldView& world) override {
        int successCount = 0;
        for (auto& child : children_) {
            if (child->tick(state, world)) {
                successCount++;
            }
        }
        return successCount >= requiredSuccesses_;
    }
    
    void reset() override {
        for (auto& child : children_) {
            child->reset();
        }
    }
    
    std::string getNodeType() const override { return "Parallel"; }
    
private:
    std::vector<std::unique_ptr<BehaviorNodeBase>> children_;
    int requiredSuccesses_;
};

} // namespace lattice::entity