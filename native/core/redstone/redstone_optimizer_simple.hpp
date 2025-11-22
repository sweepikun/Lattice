#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

namespace lattice {
namespace redstone {

// 红石位置
struct RedstonePos {
    int32_t x, y, z;
    
    RedstonePos() : x(0), y(0), z(0) {}
    RedstonePos(int32_t x, int32_t y, int32_t z) : x(x), y(y), z(z) {}
    
    bool operator==(const RedstonePos& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    
    // 为unordered_map提供哈希函数
    struct Hash {
        std::size_t operator()(const RedstonePos& pos) const {
            return std::hash<int32_t>{}(pos.x) ^ 
                   (std::hash<int32_t>{}(pos.y) << 1) ^ 
                   (std::hash<int32_t>{}(pos.z) << 2);
        }
    };
};

// 红石线类
class RedstoneWire {
public:
    RedstoneWire(const RedstonePos& pos);
    
    // 添加邻居
    void add_neighbor(const RedstonePos& neighbor);
    void remove_neighbor(const RedstonePos& neighbor);
    
    // 设置功率级别
    void set_power_level(uint8_t level);
    
    // 计算功率级别
    uint8_t calculate_power_level() const;
    
    // 获取属性
    const RedstonePos& get_position() const { return position; }
    uint8_t get_power_level() const { return power_level; }
    bool is_powered() const { return powered_state; }
    uint32_t get_update_order() const { return update_order; }
    const std::vector<RedstonePos>& get_neighbors() const { return neighbors; }
    
    // 设置属性（供内部使用）
    void set_update_order(uint32_t order) { update_order = order; }
    void set_powered_state(bool powered) { powered_state = powered; }

private:
    RedstonePos position;
    uint8_t power_level;
    bool powered_state;
    uint32_t update_order;
    std::vector<RedstonePos> neighbors;
};

// 红石网络类
class RedstoneNetwork {
public:
    RedstoneNetwork();
    ~RedstoneNetwork();
    
    // 添加/移除红石线
    bool add_wire(const RedstonePos& pos);
    bool remove_wire(const RedstonePos& pos);
    
    // 获取红石线
    RedstoneWire* get_wire(const RedstonePos& pos);
    const RedstoneWire* get_wire(const RedstonePos& pos) const;
    
    // 获取邻居位置
    std::vector<RedstonePos> get_neighbors(const RedstonePos& pos) const;
    
    // 更新邻居关系
    void update_neighbors(const RedstonePos& pos);
    
    // 更新整个网络
    bool update_network();
    
    // 清理网络
    void clear();
    
    // 统计信息
    size_t size() const;
    uint8_t get_max_power() const;
    std::vector<RedstoneWire*> get_all_wires();

private:
    std::unordered_map<RedstonePos, std::unique_ptr<RedstoneWire>, RedstonePos::Hash> wire_map;
    std::vector<RedstoneWire*> wires;
    uint8_t max_power;
    uint32_t update_count;
};

// 红石优化器
class RedstoneOptimizer {
public:
    RedstoneOptimizer();
    ~RedstoneOptimizer();
    
    // 获取或创建网络
    std::shared_ptr<RedstoneNetwork> get_or_create_network(const RedstonePos& pos);
    
    // 添加/移除红石线到网络
    bool add_wire_to_network(const RedstonePos& pos);
    bool remove_wire_from_network(const RedstonePos& pos);
    
    // 更新所有网络
    bool update_all_networks();
    
    // 优化控制
    void set_optimization_enabled(bool enabled);
    bool is_optimization_enabled() const;
    
    // 统计信息
    size_t get_network_count() const;
    size_t get_total_wire_count() const;
    
    struct OptimizationStats {
        size_t total_networks;
        size_t total_wires;
        size_t max_network_size;
        float average_network_size;
    };
    
    OptimizationStats get_optimization_stats() const;

private:
    std::unordered_map<RedstonePos, std::shared_ptr<RedstoneNetwork>, RedstonePos::Hash> networks_;
    bool enable_optimization;
};

} // namespace redstone
} // namespace lattice