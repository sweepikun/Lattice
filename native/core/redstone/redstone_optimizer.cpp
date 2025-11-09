#include "redstone_optimizer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
#include <set>
#include <queue>
#include <jni.h>

namespace lattice {
namespace redstone {

    // RedstoneWire 实现
    RedstoneWire::RedstoneWire(const RedstonePos& pos)
        : position(pos), power_level(0), is_powered(false), update_order(0) {
        // 预留邻居空间
        neighbors.reserve(8);
    }
    
    void RedstoneWire::add_neighbor(const RedstonePos& neighbor) {
        // 检查是否已存在
        auto it = std::find(neighbors.begin(), neighbors.end(), neighbor);
        if (it == neighbors.end()) {
            neighbors.push_back(neighbor);
        }
    }
    
    // RedstoneNetwork 实现
    RedstoneNetwork::RedstoneNetwork()
        : max_power(0) {
    }
    
    void RedstoneNetwork::add_wire(const RedstonePos& pos) {
        std::lock_guard<std::mutex> lock(network_mutex);
        if (wires.find(pos) == wires.end()) {
            auto wire = std::make_shared<RedstoneWire>(pos);
            wires[pos] = wire;
        }
    }
    
    void RedstoneNetwork::add_power_source(const RedstonePos& pos, int power) {
        std::lock_guard<std::mutex> lock(network_mutex);
        power_sources[pos] = power;
        if (power > max_power) {
            max_power = power;
        }
    }
    
    void RedstoneNetwork::connect_wires(const RedstonePos& pos1, const RedstonePos& pos2) {
        std::lock_guard<std::mutex> lock(network_mutex);
        auto it1 = wires.find(pos1);
        auto it2 = wires.find(pos2);
        
        if (it1 != wires.end() && it2 != wires.end()) {
            it1->second->add_neighbor(pos2);
            it2->second->add_neighbor(pos1);
        }
    }
    
    int RedstoneNetwork::calculate_power_at(const RedstonePos& pos) {
        std::lock_guard<std::mutex> lock(network_mutex);
        
        // 检查是否已计算过该位置的更新值
        auto updated_it = updated_positions.find(pos);
        if (updated_it != updated_positions.end()) {
            return updated_it->second;
        }
        
        // 检查是否为电源
        auto power_it = power_sources.find(pos);
        if (power_it != power_sources.end()) {
            updated_positions[pos] = power_it->second;
            return power_it->second;
        }
        
        // 计算从邻居获得的最大功率（减1）
        int max_received_power = 0;
        
        // 检查是否为红石线
        auto wire_it = wires.find(pos);
        if (wire_it != wires.end()) {
            const auto& wire = wire_it->second;
            
            // 检查所有邻居
            for (const auto& neighbor_pos : wire->neighbors) {
                // 检查邻居是否为电源
                auto neighbor_power_it = power_sources.find(neighbor_pos);
                if (neighbor_power_it != power_sources.end()) {
                    int received_power = neighbor_power_it->second - 1;
                    if (received_power > max_received_power) {
                        max_received_power = received_power;
                    }
                } else {
                    // 检查邻居是否为其他红石线
                    auto neighbor_wire_it = wires.find(neighbor_pos);
                    if (neighbor_wire_it != wires.end()) {
                        // 递归计算邻居的功率（需要限制深度以防止无限递归）
                        int received_power = calculate_power_at(neighbor_pos) - 1;
                        if (received_power > max_received_power) {
                            max_received_power = received_power;
                        }
                    }
                }
            }
        }
        
        // 功率不能低于0
        if (max_received_power < 0) {
            max_received_power = 0;
        }
        
        // 缓存结果
        updated_positions[pos] = max_received_power;
        return max_received_power;
    }
    
    bool RedstoneNetwork::update_network(const RedstonePos& changed_pos, int max_distance) {
        std::lock_guard<std::mutex> lock(network_mutex);
        
        // 实现BFS算法更新网络，符合Minecraft红石传播逻辑
        std::queue<std::pair<RedstonePos, int>> update_queue; // 位置和距离
        std::set<RedstonePos> visited;
        
        // 添加起始位置
        update_queue.push({changed_pos, 0});
        visited.insert(changed_pos);
        
        bool changed = false;
        
        while (!update_queue.empty()) {
            auto [current_pos, distance] = update_queue.front();
            update_queue.pop();
            
            // 如果超出最大距离，停止传播
            if (distance > max_distance) {
                continue;
            }
            
            // 计算当前位置的新功率
            int new_power = calculate_power_at(current_pos);
            
            // 检查是否为红石线
            auto wire_it = wires.find(current_pos);
            if (wire_it != wires.end()) {
                auto& wire = wire_it->second;
                
                // 检查功率是否发生变化
                if (wire->power_level != new_power) {
                    wire->power_level = new_power;
                    changed = true;
                }
                
                // 如果还有传播距离，将邻居加入队列
                if (distance < max_distance) {
                    for (const auto& neighbor_pos : wire->neighbors) {
                        // 检查是否已访问
                        if (visited.find(neighbor_pos) == visited.end()) {
                            visited.insert(neighbor_pos);
                            update_queue.push({neighbor_pos, distance + 1});
                        }
                    }
                }
            }
        }
        
        return changed;
    }
    
    std::shared_ptr<RedstoneWire> RedstoneNetwork::get_wire(const RedstonePos& pos) const {
        std::lock_guard<std::mutex> lock(network_mutex);
        auto it = wires.find(pos);
        return (it != wires.end()) ? it->second : nullptr;
    }
    
    void RedstoneNetwork::clear() {
        std::lock_guard<std::mutex> lock(network_mutex);
        wires.clear();
        power_sources.clear();
        updated_positions.clear();
    }
    
    size_t RedstoneNetwork::size() const {
        std::lock_guard<std::mutex> lock(network_mutex);
        return wires.size();
    }
    
    void RedstoneNetwork::invalidate_cache(const RedstonePos& pos) {
        std::lock_guard<std::mutex> lock(network_mutex);
        updated_positions.erase(pos);
        
        // 也使邻居的缓存失效
        RedstonePos neighbors[6] = {
            RedstonePos(pos.x - 1, pos.y, pos.z), // 西
            RedstonePos(pos.x + 1, pos.y, pos.z), // 东
            RedstonePos(pos.x, pos.y - 1, pos.z), // 下
            RedstonePos(pos.x, pos.y + 1, pos.z), // 上
            RedstonePos(pos.x, pos.y, pos.z - 1), // 北
            RedstonePos(pos.x, pos.y, pos.z + 1)  // 南
        };
        
        for (const auto& neighbor_pos : neighbors) {
            updated_positions.erase(neighbor_pos);
        }
    }
    
    // RedstoneOptimizer 实现
    std::unique_ptr<RedstoneOptimizer> RedstoneOptimizer::instance = nullptr;
    std::mutex RedstoneOptimizer::instance_mutex;
    
    RedstoneOptimizer::RedstoneOptimizer()
        : caching_enabled(true), max_cache_size(1000) {
    }
    
    RedstoneOptimizer& RedstoneOptimizer::get_instance() {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (!instance) {
            instance = std::unique_ptr<RedstoneOptimizer>(new RedstoneOptimizer());
        }
        return *instance;
    }
    
    int RedstoneOptimizer::get_redstone_power(int x, int y, int z) {
        RedstonePos pos(x, y, z);
        
        // 如果启用缓存，尝试查找现有网络
        if (caching_enabled) {
            std::lock_guard<std::mutex> lock(optimizer_mutex);
            auto it = network_mapping.find(pos);
            if (it != network_mapping.end()) {
                uint64_t network_id = it->second;
                auto network_it = networks.find(network_id);
                if (network_it != networks.end()) {
                    return network_it->second->calculate_power_at(pos);
                }
            }
        }
        
        // 创建临时网络计算
        RedstoneNetwork temp_network;
        // 在实际实现中，这里应该从Minecraft世界数据中获取附近的红石组件
        // 并构建一个临时的红石网络进行计算
        
        // 添加当前位置作为红石线
        temp_network.add_wire(pos);
        
        // 模拟从周围方块获取功率
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    
                    // 添加邻居位置作为可能的电源
                    RedstonePos neighbor_pos(x + dx, y + dy, z + dz);
                    temp_network.add_power_source(neighbor_pos, calculate_neighbor_power(x + dx, y + dy, z + dz));
                }
            }
        }
        
        return temp_network.calculate_power_at(pos);
    }
    
    int RedstoneOptimizer::calculate_neighbor_power(int x, int y, int z) {
        // 使用坐标生成伪随机数来模拟不同位置的红石功率
        // 在实际实现中，这里应该通过JNI调用访问真实的Minecraft世界数据来获取红石功率
        uint64_t hash = (static_cast<uint64_t>(x) * 73856093) ^ 
                        (static_cast<uint64_t>(y) * 19349663) ^ 
                        (static_cast<uint64_t>(z) * 83492791);
        
        // 返回0-15之间的值模拟红石功率
        return static_cast<int>(hash % 16);
    }
    
    int RedstoneOptimizer::calculate_network_power(int x, int y, int z, int max_distance) {
        // 创建临时网络进行计算
        RedstoneNetwork temp_network;
        
        // 添加附近的红石线（在指定区域内）
        for (int dx = -max_distance; dx <= max_distance; dx++) {
            for (int dy = -max_distance; dy <= max_distance; dy++) {
                for (int dz = -max_distance; dz <= max_distance; dz++) {
                    RedstonePos pos(x + dx, y + dy, z + dz);
                    temp_network.add_wire(pos);
                    
                    // 添加邻居作为可能的电源
                    for (int ndx = -1; ndx <= 1; ndx++) {
                        for (int ndy = -1; ndy <= 1; ndy++) {
                            for (int ndz = -1; ndz <= 1; ndz++) {
                                if (ndx == 0 && ndy == 0 && ndz == 0) continue;
                                RedstonePos neighbor_pos(x + dx + ndx, y + dy + ndy, z + dz + ndz);
                                temp_network.add_power_source(neighbor_pos, calculate_neighbor_power(
                                    x + dx + ndx, y + dy + ndy, z + dz + ndz));
                            }
                        }
                    }
                }
            }
        }
        
        // 在实际实现中，这里应该从Minecraft世界数据中获取附近的红石组件
        // 并正确连接它们以构建红石网络
        
        // 更新网络并返回是否有变化
        return temp_network.update_network(RedstonePos(x, y, z), max_distance) ? 1 : 0;
    }
    
    bool RedstoneOptimizer::update_redstone_network(int x, int y, int z, int max_distance) {
        // 创建临时网络进行更新
        RedstoneNetwork temp_network;
        
        // 添加附近的红石线（在指定区域内）
        for (int dx = -max_distance; dx <= max_distance; dx++) {
            for (int dy = -max_distance; dy <= max_distance; dy++) {
                for (int dz = -max_distance; dz <= max_distance; dz++) {
                    RedstonePos pos(x + dx, y + dy, z + dz);
                    temp_network.add_wire(pos);
                    
                    // 添加邻居作为可能的电源
                    for (int ndx = -1; ndx <= 1; ndx++) {
                        for (int ndy = -1; ndy <= 1; ndy++) {
                            for (int ndz = -1; ndz <= 1; ndz++) {
                                if (ndx == 0 && ndy == 0 && ndz == 0) continue;
                                RedstonePos neighbor_pos(x + dx + ndx, y + dy + ndy, z + dz + ndz);
                                temp_network.add_power_source(neighbor_pos, calculate_neighbor_power(
                                    x + dx + ndx, y + dy + ndy, z + dz + ndz));
                            }
                        }
                    }
                }
            }
        }
        
        // 在实际实现中，这里应该从Minecraft世界数据中获取附近的红石组件
        // 并正确连接它们以构建红石网络
        
        // 更新网络并返回是否有变化
        return temp_network.update_network(RedstonePos(x, y, z), max_distance);
    }
    
    void RedstoneOptimizer::invalidate_network_cache(int x, int y, int z) {
        if (!caching_enabled) return;
        
        std::lock_guard<std::mutex> lock(optimizer_mutex);
        
        RedstonePos pos(x, y, z);
        auto it = network_mapping.find(pos);
        if (it != network_mapping.end()) {
            uint64_t network_id = it->second;
            auto network_it = networks.find(network_id);
            if (network_it != networks.end()) {
                network_it->second->clear();
                networks.erase(network_it);
            }
            network_mapping.erase(it);
        }
    }
    
    void RedstoneOptimizer::set_caching_enabled(bool enabled) {
        caching_enabled = enabled;
    }
    
    void RedstoneOptimizer::set_max_cache_size(size_t size) {
        max_cache_size = size;
    }
    
    void RedstoneOptimizer::cleanup_cache() {
        if (!caching_enabled) return;
        
        std::lock_guard<std::mutex> lock(optimizer_mutex);
        if (networks.size() > max_cache_size * 0.8) {
            // 清理20%的缓存
            size_t cleanup_count = networks.size() / 5;
            auto it = networks.begin();
            for (size_t i = 0; i < cleanup_count && it != networks.end(); ++i) {
                // 在实际实现中，应该正确清理network_mapping中的条目
                it = networks.erase(it);
            }
        }
    }

} // namespace redstone
} // namespace lattice