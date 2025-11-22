#include "pathfinder.hpp"
#include <cmath>

namespace lattice {
namespace world {

// PathfinderOptimizer 实现
PathfinderOptimizer::PathfinderOptimizer() {
    // 初始化默认生物参数
    mob_params[MobType::PASSIVE] = {0.5f, 3.0f, false, false, true, false, 1.0f};
    mob_params[MobType::NEUTRAL] = {0.5f, 3.0f, false, false, false, false, 1.0f};
    mob_params[MobType::HOSTILE] = {0.5f, 3.0f, false, false, false, false, 1.2f};
    mob_params[MobType::WATER] = {0.5f, 1.0f, true, false, false, false, 1.0f};
    mob_params[MobType::FLYING] = {0.0f, 10.0f, false, true, false, false, 1.5f};
}

PathfinderOptimizer::~PathfinderOptimizer() = default;

std::vector<std::shared_ptr<PathNode>> PathfinderOptimizer::optimize_pathfinding(
    int start_x, int start_y, int start_z,
    int end_x, int end_y, int end_z,
    MobType mob_type) {
    
    // 简化的寻路实现 - 返回空路径
    return std::vector<std::shared_ptr<PathNode>>();
}

const MobPathfindingParams& PathfinderOptimizer::get_mob_params(MobType mob_type) {
    static MobPathfindingParams default_params = {0.5f, 3.0f, false, false, false, false, 1.0f};
    auto it = mob_params.find(mob_type);
    if (it != mob_params.end()) {
        return it->second;
    }
    return default_params;
}

std::vector<std::shared_ptr<PathNode>> PathfinderOptimizer::reconstruct_path(std::shared_ptr<PathNode> node) {
    std::vector<std::shared_ptr<PathNode>> path;
    auto current = node;
    while (current) {
        path.push_back(current);
        current = current->parent;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

float PathfinderOptimizer::calculate_heuristic(std::shared_ptr<PathNode> a, std::shared_ptr<PathNode> b) {
    // 简单的欧几里得距离
    float dx = b->x - a->x;
    float dy = b->y - a->y;
    float dz = b->z - a->z;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

float PathfinderOptimizer::calculate_move_cost(std::shared_ptr<PathNode> from, const Node& direction, const MobPathfindingParams& params) {
    // 简化的移动成本计算
    return 1.0f;
}

bool PathfinderOptimizer::is_walkable(int x, int y, int z, const MobPathfindingParams& params) {
    // 简化的可行走检查 - 总是返回true
    return true;
}

} // namespace world
} // namespace lattice