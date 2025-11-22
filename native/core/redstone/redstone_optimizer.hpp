#ifndef LATTICE_REDSTONE_OPTIMIZER_HPP
#define LATTICE_REDSTONE_OPTIMIZER_HPP

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>

namespace lattice {
namespace redstone {

    // 红石位置结构
    struct RedstonePos {
        int x, y, z;
        
        RedstonePos() : x(0), y(0), z(0) {}
        RedstonePos(int x, int y, int z) : x(x), y(y), z(z) {}
        
        // 用于哈希表的相等比较运算符
        bool operator==(const RedstonePos& other) const {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    
    // 红石位置哈希函数
    struct RedstonePosHash {
        std::size_t operator()(const RedstonePos& pos) const {
            // 使用标准哈希组合方法
            std::size_t h1 = std::hash<int>{}(pos.x);
            std::size_t h2 = std::hash<int>{}(pos.y);
            std::size_t h3 = std::hash<int>{}(pos.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
    
    // 前向声明
    class RedstoneWire;
    class RedstoneNetwork;
    
    // 红石线类
    class RedstoneWire {
    public:
        RedstonePos position;                           // 位置
        int power_level;                                // 当前功率等级 (0-15)
        bool is_powered;                                // 是否被激活
        int update_order;                               // 更新顺序
        std::vector<RedstonePos> neighbors;             // 邻居列表
        
        explicit RedstoneWire(const RedstonePos& pos);
        ~RedstoneWire() = default;
        
        // 添加邻居
        void add_neighbor(const RedstonePos& neighbor);
    };
    
    // 红石网络类
    class RedstoneNetwork {
    private:
        std::unordered_map<RedstonePos, std::shared_ptr<RedstoneWire>, RedstonePosHash> wires;  // 红石线集合
        std::unordered_map<RedstonePos, int, RedstonePosHash> power_sources;                    // 电源位置和功率
        std::unordered_map<RedstonePos, int, RedstonePosHash> updated_positions;                // 已更新位置的功率值
        mutable std::mutex network_mutex;                                                      // 网络互斥锁
        int max_power;                                                                         // 网络中的最大功率
        
    public:
        RedstoneNetwork();
        ~RedstoneNetwork() = default;
        
        // 添加红石线
        void add_wire(const RedstonePos& pos);
        
        // 添加电源
        void add_power_source(const RedstonePos& pos, int power);
        
        // 连接两根红石线
        void connect_wires(const RedstonePos& pos1, const RedstonePos& pos2);
        
        // 计算指定位置的功率
        int calculate_power_at(const RedstonePos& pos);
        
        // 更新网络
        bool update_network(const RedstonePos& changed_pos, int max_distance = 15);
        
        // 获取指定位置的红石线
        std::shared_ptr<RedstoneWire> get_wire(const RedstonePos& pos) const;
        
        // 清空网络
        void clear();
        
        // 获取网络大小
        size_t size() const;
        
        // 使指定位置的缓存失效
        void invalidate_cache(const RedstonePos& pos);
    };

    // 全局红石优化器
    class RedstoneOptimizer {
    private:
        static std::unique_ptr<RedstoneOptimizer> instance;
        static std::mutex instance_mutex;
        
        std::unordered_map<uint64_t, std::unique_ptr<RedstoneNetwork>> networks;
        std::unordered_map<RedstonePos, uint64_t, RedstonePosHash> network_mapping;
        mutable std::mutex optimizer_mutex;
        
        bool caching_enabled;
        size_t max_cache_size;
        
        RedstoneOptimizer();
        
    public:
        ~RedstoneOptimizer() = default;
        
        // 获取单例实例
        static RedstoneOptimizer& get_instance();
        
        // 计算指定位置的红石电源强度
        int get_redstone_power(int x, int y, int z);
        
        // 计算红石网络的电源强度
        int calculate_network_power(int x, int y, int z, int max_distance);
        
        // 更新红石网络
        bool update_redstone_network(int x, int y, int z, int max_distance);
        
        // 使网络缓存失效
        void invalidate_network_cache(int x, int y, int z);
        
        // 启用/禁用缓存
        void set_caching_enabled(bool enabled);
        
        // 设置最大缓存大小
        void set_max_cache_size(size_t size);
        
        // 清理过期缓存
        void cleanup_cache();
        
    private:
        // 计算邻居位置的功率（辅助方法）
        int calculate_neighbor_power(int x, int y, int z);
    };

} // namespace redstone
} // namespace lattice

// 用于std::unordered_map的哈希特化
namespace std {
    template <>
    struct hash<lattice::redstone::RedstonePos> {
        std::size_t operator()(const lattice::redstone::RedstonePos& pos) const {
            return lattice::redstone::RedstonePosHash{}(pos);
        }
    };
}

#endif // LATTICE_REDSTONE_OPTIMIZER_HPP