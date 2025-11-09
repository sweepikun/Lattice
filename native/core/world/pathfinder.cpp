#include "pathfinder.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <limits>
#include <queue>
#include <unordered_set>
#include <unordered_map>

namespace lattice {
namespace world {

    // 定义方向结构体
    struct Node {
        int x, y, z;
        Node(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}
    };

    // 方向向量（包括对角线方向）
    static const std::vector<Node> DIRECTIONS = {
        // 水平和垂直方向
        Node(1, 0, 0), Node(-1, 0, 0), Node(0, 0, 1), Node(0, 0, -1),
        // 垂直方向
        Node(0, 1, 0), Node(0, -1, 0),
        // 对角线方向
        Node(1, 0, 1), Node(1, 0, -1), Node(-1, 0, 1), Node(-1, 0, -1),
        Node(1, 1, 0), Node(1, -1, 0), Node(-1, 1, 0), Node(-1, -1, 0),
        Node(0, 1, 1), Node(0, 1, -1), Node(0, -1, 1), Node(0, -1, -1),
        Node(1, 1, 1), Node(1, 1, -1), Node(1, -1, 1), Node(1, -1, -1),
        Node(-1, 1, 1), Node(-1, 1, -1), Node(-1, -1, 1), Node(-1, -1, -1)
    };

    // PathNode 实现
    PathNode::PathNode(int x, int y, int z)
        : x(x), y(y), z(z), g_cost(0.0f), h_cost(0.0f), f_cost(0.0f), 
          is_open(false), is_closed(false), parent(nullptr) {
    }
    
    float PathNode::distance_to(const PathNode& other) const {
        // 使用欧几里得距离
        int dx = x - other.x;
        int dy = y - other.y;
        int dz = z - other.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    
    bool PathNode::operator>(const PathNode& other) const {
        // 用于优先队列的比较（较小的f_cost优先）
        if (f_cost != other.f_cost) {
            return f_cost > other.f_cost;
        }
        // 如果f_cost相同，比较h_cost
        return h_cost > other.h_cost;
    }

    // 自定义比较器用于优先队列
    struct PathNodeComparator {
        bool operator()(const std::shared_ptr<PathNode>& a, const std::shared_ptr<PathNode>& b) const {
            return a->f_cost > b->f_cost;
        }
    };
    
    // PathfindingContext 实现
    PathfindingContext::PathfindingContext(int max_steps, float max_dist)
        : max_pathfinding_steps(max_steps), max_distance(max_dist) {
    }
    
    bool PathfindingContext::is_walkable(int x, int y, int z, MobType mob_type) const {
        // 简化实现 - 实际中需要检查方块类型
        // 这里我们只做基本的边界检查
        
        // 检查Y坐标是否在有效范围内
        if (y < 0 || y > 255) {
            return false;
        }
        
        // 根据生物类型调整可行走性检查
        switch (mob_type) {
            case MobType::WATER:
                // 水生生物可以在水中行走
                return true;
                
            case MobType::FLYING:
                // 飞行生物可以穿过大多数方块
                return true;
                
            default:
                // 其他生物需要检查方块是否为固体
                // 简化处理，假设大部分位置可行走
                return (y > 0); // 简单检查避免在虚空行走
        }
    }
    
    std::vector<std::shared_ptr<PathNode>> PathfindingContext::get_neighbors(const PathNode& node, MobType mob_type) const {
        std::vector<std::shared_ptr<PathNode>> neighbors;
        neighbors.reserve(27); // 3x3x3 网格内的邻居
        
        // 根据Minecraft 1.21.8的寻路逻辑，生物会优先考虑水平移动
        // 先添加水平方向的邻居（4个）
        int dx_vals[] = { -1, 1, 0, 0 };
        int dz_vals[] = { 0, 0, -1, 1 };
        
        for (int i = 0; i < 4; ++i) {
            int nx = node.x + dx_vals[i];
            int ny = node.y;
            int nz = node.z + dz_vals[i];
            
            // 检查是否可行走
            if (is_walkable(nx, ny, nz, mob_type)) {
                // 创建或获取节点
                uint64_t key = (static_cast<uint64_t>(nx) << 32) | 
                              (static_cast<uint64_t>(nz) & 0xFFFFFFFF);
                key = (key << 16) | (ny & 0xFFFF);
                
                auto it = node_cache.find(key);
                std::shared_ptr<PathNode> neighbor;
                
                if (it != node_cache.end()) {
                    neighbor = it->second;
                } else {
                    neighbor = std::make_shared<PathNode>(nx, ny, nz);
                    node_cache[key] = neighbor;
                }
                
                neighbors.push_back(neighbor);
            }
        }
        
        // 添加垂直方向的邻居（上下）
        int dy_vals[] = { -1, 1 };
        for (int i = 0; i < 2; ++i) {
            int nx = node.x;
            int ny = node.y + dy_vals[i];
            int nz = node.z;
            
            // 检查是否可行走
            if (is_walkable(nx, ny, nz, mob_type)) {
                // 创建或获取节点
                uint64_t key = (static_cast<uint64_t>(nx) << 32) | 
                              (static_cast<uint64_t>(nz) & 0xFFFFFFFF);
                key = (key << 16) | (ny & 0xFFFF);
                
                auto it = node_cache.find(key);
                std::shared_ptr<PathNode> neighbor;
                
                if (it != node_cache.end()) {
                    neighbor = it->second;
                } else {
                    neighbor = std::make_shared<PathNode>(nx, ny, nz);
                    node_cache[key] = neighbor;
                }
                
                neighbors.push_back(neighbor);
            }
        }
        
        // 根据生物类型添加额外的邻居
        const auto& params = PathfinderOptimizer::get_instance().get_mob_params(mob_type);
        
        // 如果生物可以飞行或游泳，添加对角线邻居
        if (params.can_fly || params.can_swim) {
            // 添加对角线方向的邻居
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dz = -1; dz <= 1; ++dz) {
                        // 跳过已经添加的方向
                        if ((dx == 0 && dy == 0 && dz == 0) || 
                            (dx != 0 && dz != 0) || 
                            (dy != 0 && (dx != 0 || dz != 0))) {
                            continue;
                        }
                        
                        int nx = node.x + dx;
                        int ny = node.y + dy;
                        int nz = node.z + dz;
                        
                        // 检查是否可行走
                        if (is_walkable(nx, ny, nz, mob_type)) {
                            // 创建或获取节点
                            uint64_t key = (static_cast<uint64_t>(nx) << 32) | 
                                          (static_cast<uint64_t>(nz) & 0xFFFFFFFF);
                            key = (key << 16) | (ny & 0xFFFF);
                            
                            auto it = node_cache.find(key);
                            std::shared_ptr<PathNode> neighbor;
                            
                            if (it != node_cache.end()) {
                                neighbor = it->second;
                            } else {
                                neighbor = std::make_shared<PathNode>(nx, ny, nz);
                                node_cache[key] = neighbor;
                            }
                            
                            neighbors.push_back(neighbor);
                        }
                    }
                }
            }
        }
        
        return neighbors;
    }
    
    float PathfindingContext::calculate_heuristic(const PathNode& from, const PathNode& to) const {
        // 使用曼哈顿距离作为启发式函数，符合Minecraft寻路算法
        int dx = std::abs(from.x - to.x);
        int dy = std::abs(from.y - to.y);
        int dz = std::abs(from.z - to.z);
        return static_cast<float>(dx + dy + dz);
    }
    
    std::vector<std::shared_ptr<PathNode>> PathfindingContext::find_path(int start_x, int start_y, int start_z,
                                                                        int end_x, int end_y, int end_z,
                                                                        MobType mob_type) {
        std::lock_guard<std::mutex> lock(context_mutex);
        
        // 清空之前的状态
        clear_cache();
        closed_set.clear();
        while (!open_queue.empty()) {
            open_queue.pop();
        }
        
        // 创建起始和结束节点
        auto start_node = std::make_shared<PathNode>(start_x, start_y, start_z);
        auto end_node = std::make_shared<PathNode>(end_x, end_y, end_z);
        
        // 检查起始和结束点是否可行走
        if (!is_walkable(start_x, start_y, start_z, mob_type) ||
            !is_walkable(end_x, end_y, end_z, mob_type)) {
            return {}; // 无法寻路
        }
        
        // 将起始节点加入开放列表
        start_node->g_cost = 0.0f;
        start_node->h_cost = calculate_heuristic(*start_node, *end_node);
        start_node->f_cost = start_node->g_cost + start_node->h_cost;
        start_node->is_open = true;
        open_queue.push(start_node);
        
        int steps = 0;
        
        // A* 寻路算法主循环
        while (!open_queue.empty() && steps < max_pathfinding_steps) {
            // 获取f_cost最小的节点
            auto current = open_queue.top();
            open_queue.pop();
            
            // 如果当前节点已在关闭列表中，跳过
            uint64_t current_key = (static_cast<uint64_t>(current->x) << 32) | 
                                  (static_cast<uint64_t>(current->z) & 0xFFFFFFFF);
            current_key = (current_key << 16) | (current->y & 0xFFFF);
            
            if (closed_set.find(current_key) != closed_set.end()) {
                continue;
            }
            
            // 将当前节点加入关闭列表
            current->is_open = false;
            current->is_closed = true;
            closed_set.insert(current_key);
            
            steps++;
            
            // 检查是否到达终点
            if (current->x == end_node->x && 
                current->y == end_node->y && 
                current->z == end_node->z) {
                // 重构路径
                std::vector<std::shared_ptr<PathNode>> path;
                auto path_node = current;
                while (path_node != nullptr) {
                    path.push_back(path_node);
                    path_node = path_node->parent;
                }
                std::reverse(path.begin(), path.end());
                return path;
            }
            
            // 获取邻居节点
            auto neighbors = get_neighbors(*current, mob_type);
            
            for (const auto& neighbor : neighbors) {
                // 检查邻居是否已在关闭列表中
                uint64_t neighbor_key = (static_cast<uint64_t>(neighbor->x) << 32) | 
                                       (static_cast<uint64_t>(neighbor->z) & 0xFFFFFFFF);
                neighbor_key = (neighbor_key << 16) | (neighbor->y & 0xFFFF);
                
                if (closed_set.find(neighbor_key) != closed_set.end()) {
                    continue;
                }
                
                // 计算到邻居的代价
                float move_cost = current->distance_to(*neighbor);
                float tentative_g_cost = current->g_cost + move_cost;
                
                // 如果邻居不在开放列表中，或者找到了更短的路径
                if (!neighbor->is_open || tentative_g_cost < neighbor->g_cost) {
                    neighbor->parent = current;
                    neighbor->g_cost = tentative_g_cost;
                    neighbor->h_cost = calculate_heuristic(*neighbor, *end_node);
                    neighbor->f_cost = neighbor->g_cost + neighbor->h_cost;
                    
                    if (!neighbor->is_open) {
                        neighbor->is_open = true;
                        open_queue.push(neighbor);
                    }
                }
            }
        }
        
        // 未找到路径
        return {};
    }
    
    void PathfindingContext::clear_cache() {
        node_cache.clear();
    }
    
    // PathfinderOptimizer 实现
    std::unique_ptr<PathfinderOptimizer> PathfinderOptimizer::instance = nullptr;
    std::mutex PathfinderOptimizer::instance_mutex;
    
    PathfinderOptimizer::PathfinderOptimizer() {
        initialize_mob_params();
    }
    
    void PathfinderOptimizer::initialize_mob_params() {
        // 被动生物参数（如羊、牛）
        mob_params[MobType::PASSIVE] = {
            1.25f,   // step_height - Minecraft中大多数生物的stepHeight都是0.6，但这里使用1.25表示可以走上一个完整方块加半方块
            1.25f,   // max_drop_height - Minecraft中大多数生物可以安全掉落的高度
            false,   // can_swim
            false,   // can_fly
            true,    // avoids_water - 被动生物通常避开水
            true,    // avoids_sun - 被动生物在白天会避开阳光（如猪、牛等）
            1.0f     // speed_factor
        };
        
        // 中立生物参数（如狼、猪）
        mob_params[MobType::NEUTRAL] = {
            1.25f,   // step_height
            1.25f,   // max_drop_height
            false,   // can_swim
            false,   // can_fly
            false,   // avoids_water - 中立生物对水的态度不一
            true,    // avoids_sun - 大多数中立生物避开阳光
            1.1f     // speed_factor - 中立生物通常比被动生物稍微快一点
        };
        
        // 敌对生物参数（如僵尸、骷髅）
        mob_params[MobType::HOSTILE] = {
            1.25f,   // step_height
            1.25f,   // max_drop_height
            false,   // can_swim
            false,   // can_fly
            false,   // avoids_water - 敌对生物对水的态度不一（骷髅避开，但 drowned 是水生的）
            false,   // avoids_sun - 大多数敌对生物不怕阳光（但骷髅怕）
            1.2f     // speed_factor - 敌对生物通常比被动生物快
        };
        
        // 水生生物参数（如鱼、鱿鱼）
        mob_params[MobType::WATER] = {
            0.5f,    // step_height - 水生生物在水中移动
            0.5f,    // max_drop_height
            true,    // can_swim
            false,   // can_fly
            false,   // avoids_water
            false,   // avoids_sun
            0.8f     // speed_factor - 水中移动速度
        };
        
        // 飞行生物参数（如蝙蝠、末影龙）
        mob_params[MobType::FLYING] = {
            2.0f,    // step_height - 飞行生物可以飞过障碍物
            2.0f,    // max_drop_height
            false,   // can_swim
            true,    // can_fly
            false,   // avoids_water
            false,   // avoids_sun
            1.5f     // speed_factor - 飞行生物通常很快
        };
    }
    
    PathfinderOptimizer& PathfinderOptimizer::get_instance() {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (!instance) {
            instance = std::unique_ptr<PathfinderOptimizer>(new PathfinderOptimizer());
        }
        return *instance;
    }
    
    const PathfinderOptimizer::MobPathfindingParams& PathfinderOptimizer::get_mob_params(MobType mob_type) const {
        auto it = mob_params.find(mob_type);
        if (it != mob_params.end()) {
            return it->second;
        }
        // 返回默认参数
        static MobPathfindingParams default_params = {
            1.0f,    // step_height
            1.0f,    // max_drop_height
            false,   // can_swim
            false,   // can_fly
            false,   // avoids_water
            false,   // avoids_sun
            1.0f     // speed_factor
        };
        return default_params;
    }
    
    std::vector<std::shared_ptr<PathNode>> PathfinderOptimizer::optimize_pathfinding(int start_x, int start_y, int start_z,
                                                                                   int end_x, int end_y, int end_z,
                                                                                   MobType mob_type) {
        // 检查缓存
        auto cached_path = get_cached_path(start_x, start_y, start_z, end_x, end_y, end_z, mob_type);
        if (!cached_path.empty()) {
            return cached_path;
        }
        
        // 获取生物寻路参数
        const auto& params = get_mob_params(mob_type);
        
        // 创建起始节点和目标节点
        auto start_node = std::make_shared<PathNode>(start_x, start_y, start_z);
        auto end_node = std::make_shared<PathNode>(end_x, end_y, end_z);
        
        // 初始化开放列表和关闭列表
        std::priority_queue<std::shared_ptr<PathNode>, std::vector<std::shared_ptr<PathNode>>, PathNodeComparator> open_list;
        std::unordered_set<uint64_t> closed_list;
        
        // 将起始节点添加到开放列表
        start_node->g_cost = 0;
        start_node->h_cost = calculate_heuristic(start_node, end_node);
        start_node->f_cost = start_node->g_cost + start_node->h_cost;
        open_list.push(start_node);
        
        // 主循环
        while (!open_list.empty()) {
            // 获取f_cost最低的节点
            auto current = open_list.top();
            open_list.pop();
            
            // 检查是否到达目标节点
            if (current->x == end_node->x && current->y == end_node->y && current->z == end_node->z) {
                // 重构路径
                auto path = reconstruct_path(current);
                // 缓存结果
                if (!path.empty()) {
                    cache_path(start_x, start_y, start_z, end_x, end_y, end_z, mob_type, path);
                }
                return path;
            }
            
            // 将当前节点添加到关闭列表
            uint64_t current_key = (static_cast<uint64_t>(current->x) << 32) | 
                                  (static_cast<uint64_t>(current->z) << 16) | 
                                  static_cast<uint64_t>(current->y);
            closed_list.insert(current_key);
            
            // 检查邻居节点
            for (const auto& direction : DIRECTIONS) {
                int neighbor_x = current->x + direction.x;
                int neighbor_y = current->y + direction.y;
                int neighbor_z = current->z + direction.z;
                
                // 检查节点是否在关闭列表中
                uint64_t neighbor_key = (static_cast<uint64_t>(neighbor_x) << 32) | 
                                       (static_cast<uint64_t>(neighbor_z) << 16) | 
                                       static_cast<uint64_t>(neighbor_y);
                if (closed_list.find(neighbor_key) != closed_list.end()) {
                    continue;
                }
                
                // 检查邻居节点是否可通行
                if (!is_walkable(neighbor_x, neighbor_y, neighbor_z, params)) {
                    continue;
                }
                
                // 计算到邻居节点的g_cost
                float move_cost = calculate_move_cost(current, direction, params);
                float tentative_g_cost = current->g_cost + move_cost;
                
                // 创建邻居节点
                auto neighbor = std::make_shared<PathNode>(neighbor_x, neighbor_y, neighbor_z);
                neighbor->g_cost = tentative_g_cost;
                neighbor->h_cost = calculate_heuristic(neighbor, end_node);
                neighbor->f_cost = neighbor->g_cost + neighbor->h_cost;
                neighbor->parent = current;
                
                // 将邻居节点添加到开放列表
                open_list.push(neighbor);
            }
        }
        
        // 未找到路径
        return {};
    }
    
    std::vector<std::shared_ptr<PathNode>> PathfinderOptimizer::reconstruct_path(std::shared_ptr<PathNode> node) {
        std::vector<std::shared_ptr<PathNode>> path;
        
        // 从目标节点回溯到起始节点
        auto current = node;
        while (current != nullptr) {
            path.push_back(current);
            current = current->parent;
        }
        
        // 反转路径（从起始节点到目标节点）
        std::reverse(path.begin(), path.end());
        
        return path;
    }
    
    float PathfinderOptimizer::calculate_heuristic(std::shared_ptr<PathNode> a, std::shared_ptr<PathNode> b) {
        // 使用曼哈顿距离作为启发式函数
        return std::abs(a->x - b->x) + std::abs(a->y - b->y) + std::abs(a->z - b->z);
    }
    
    float PathfinderOptimizer::calculate_move_cost(std::shared_ptr<PathNode> from, const Node& direction, const MobPathfindingParams& params) {
        // 基本移动成本
        float cost = 1.0f;
        
        // 垂直移动成本
        if (direction.y != 0) {
            if (direction.y > 0) {
                // 向上移动
                cost += 1.0f; // 基本的向上移动成本
            } else {
                // 向下移动
                cost += 0.5f; // 向下移动相对容易
            }
        }
        
        // 对角线移动成本
        int horizontal_movement = std::abs(direction.x) + std::abs(direction.z);
        int vertical_movement = std::abs(direction.y);
        if (horizontal_movement > 1 || vertical_movement > 1) {
            cost *= 1.41f; // 对角线移动成本约为直线移动的√2倍
        }
        
        // 应用速度因子
        cost /= params.speed_factor;
        
        return cost;
    }
    
    bool PathfinderOptimizer::is_walkable(int x, int y, int z, const MobPathfindingParams& params) {
        // 简化实现：根据坐标生成伪随机数来模拟可行走性
        uint64_t hash = (static_cast<uint64_t>(x) * 73856093) ^ 
                        (static_cast<uint64_t>(y) * 19349663) ^ 
                        (static_cast<uint64_t>(z) * 83492791);
        
        // 模拟不同类型的方块
        int block_type = static_cast<int>(hash % 10);
        
        switch (block_type) {
            case 0: // 空气方块
                return true;
            case 1: // 水方块
                return params.can_swim;
            case 2: // 熔岩方块
                return false;
            case 3: // 树叶方块
                return true;
            case 4: // 玻璃方块
                return true;
            case 5: // 铁栏杆方块
                return true;
            case 6: // 栅栏方块
                return true;
            case 7: // 活板门方块
                return true;
            case 8: // 楼梯方块
                return true;
            case 9: // 固体方块
                return false;
            default:
                return true;
        }
    }
    
    void PathfinderOptimizer::cache_path(int start_x, int start_y, int start_z,
                                        int end_x, int end_y, int end_z,
                                        MobType mob_type,
                                        const std::vector<std::shared_ptr<PathNode>>& path) {
        std::lock_guard<std::mutex> lock(optimizer_mutex);
        
        uint64_t key = (static_cast<uint64_t>(start_x) << 32) | 
                      (static_cast<uint64_t>(start_z) & 0xFFFFFFFF);
        key = (key << 16) | (start_y & 0xFFFF);
        key ^= (static_cast<uint64_t>(end_x) << 32) | 
               (static_cast<uint64_t>(end_z) & 0xFFFFFFFF);
        key = (key << 16) | (end_y & 0xFFFF);
        key ^= static_cast<uint64_t>(static_cast<int>(mob_type));
        
        path_cache[key] = path;
        
        // 控制缓存大小
        if (path_cache.size() > 1000) {
            cleanup_cache();
        }
    }
    
    std::vector<std::shared_ptr<PathNode>> PathfinderOptimizer::get_cached_path(int start_x, int start_y, int start_z,
                                                                              int end_x, int end_y, int end_z,
                                                                              MobType mob_type) const {
        std::lock_guard<std::mutex> lock(optimizer_mutex);
        
        uint64_t key = (static_cast<uint64_t>(start_x) << 32) | 
                      (static_cast<uint64_t>(start_z) & 0xFFFFFFFF);
        key = (key << 16) | (start_y & 0xFFFF);
        key ^= (static_cast<uint64_t>(end_x) << 32) | 
               (static_cast<uint64_t>(end_z) & 0xFFFFFFFF);
        key = (key << 16) | (end_y & 0xFFFF);
        key ^= static_cast<uint64_t>(static_cast<int>(mob_type));
        
        auto it = path_cache.find(key);
        if (it != path_cache.end()) {
            return it->second;
        }
        
        return {};
    }
    
    void PathfinderOptimizer::cleanup_cache() {
        // 移除约20%的缓存项
        size_t remove_count = path_cache.size() / 5;
        auto it = path_cache.begin();
        for (size_t i = 0; i < remove_count && it != path_cache.end(); ++i) {
            it = path_cache.erase(it);
        }
    }

} // namespace world
} // namespace lattice