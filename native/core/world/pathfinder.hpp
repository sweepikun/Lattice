#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <queue>
#include <mutex>

namespace lattice {
namespace world {

// 生物类型枚举
enum class MobType {
    PASSIVE,  // 被动生物
    NEUTRAL,  // 中立生物
    HOSTILE,  // 敌对生物
    WATER,    // 水生生物
    FLYING    // 飞行生物
};

// 节点结构
struct Node {
    int x, y, z;
    
    Node() : x(0), y(0), z(0) {}
    Node(int x, int y, int z) : x(x), y(y), z(z) {}
};

// 路径节点结构
struct PathNode : public Node {
    float g_cost;  // 从起始节点到当前节点的实际成本
    float h_cost;  // 从当前节点到目标节点的启发式估计成本
    float f_cost;  // g_cost + h_cost
    std::shared_ptr<PathNode> parent;  // 父节点
    
    PathNode() : Node(), g_cost(0.0f), h_cost(0.0f), f_cost(0.0f), parent(nullptr) {}
    PathNode(int x, int y, int z) : Node(x, y, z), g_cost(0.0f), h_cost(0.0f), f_cost(0.0f), parent(nullptr) {}
};

// 路径节点比较器（用于优先队列）
struct PathNodeComparator {
    bool operator()(const std::shared_ptr<PathNode>& a, const std::shared_ptr<PathNode>& b) const {
        return a->f_cost > b->f_cost;  // 最小堆
    }
};

// 生物寻路参数
struct MobPathfindingParams {
    float step_height;      // 可跨越的最大高度
    float max_drop_height;  // 可安全掉落的最大高度
    bool can_swim;          // 是否可以游泳
    bool can_fly;           // 是否可以飞行
    bool avoids_water;      // 是否避开水
    bool avoids_sun;        // 是否避开阳光
    float speed_factor;     // 移动速度因子
};

// 寻路优化器
class PathfinderOptimizer {
public:
    PathfinderOptimizer();
    ~PathfinderOptimizer();

    // 优化寻路计算
    std::vector<std::shared_ptr<PathNode>> optimize_pathfinding(
        int start_x, int start_y, int start_z,
        int end_x, int end_y, int end_z,
        MobType mob_type);
    
    // 获取生物寻路参数
    const MobPathfindingParams& get_mob_params(MobType mob_type);
    
    // 获取单例实例
    static PathfinderOptimizer& get_instance() {
        static PathfinderOptimizer instance;
        return instance;
    }

private:
    // 重构路径
    std::vector<std::shared_ptr<PathNode>> reconstruct_path(std::shared_ptr<PathNode> node);
    
    // 计算启发式函数
    float calculate_heuristic(std::shared_ptr<PathNode> a, std::shared_ptr<PathNode> b);
    
    // 计算移动成本
    float calculate_move_cost(std::shared_ptr<PathNode> from, const Node& direction, const MobPathfindingParams& params);
    
    // 检查节点是否可行走
    bool is_walkable(int x, int y, int z, const MobPathfindingParams& params);
    
    // 不同生物类型的寻路参数
    std::unordered_map<MobType, MobPathfindingParams> mob_params;
    std::unordered_map<uint64_t, std::vector<std::shared_ptr<PathNode>>> path_cache;
    mutable std::mutex optimizer_mutex;
};

} // namespace world
} // namespace lattice