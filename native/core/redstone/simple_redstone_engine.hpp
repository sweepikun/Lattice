#ifndef LATTICE_SIMPLE_REDSTONE_ENGINE_HPP
#define LATTICE_SIMPLE_REDSTONE_ENGINE_HPP

#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <iostream>

namespace lattice::redstone {

    // 位置结构
    struct Position {
        int x, y, z;
        
        Position() : x(0), y(0), z(0) {}
        Position(int x, int y, int z) : x(x), y(y), z(z) {}
        
        auto operator<=>(const Position& other) const = default;
        bool operator==(const Position& other) const = default;
    };

    // 组件类型枚举
    enum class ComponentType : uint8_t {
        WIRE,
        REPEATER, 
        COMPARATOR,
        TORCH,
        LEVER,
        BUTTON,
        PRESSURE_PLATE,
        OBSERVER,
        PISTON
    };

    // 基础组件接口
    class RedstoneComponentBase {
    public:
        Position position;
        ComponentType type;
        int currentSignal;
        int delay;
        
        RedstoneComponentBase(Position pos, ComponentType type) 
            : position(pos), type(type), currentSignal(0), delay(0) {}
        
        virtual ~RedstoneComponentBase() = default;
        
        // 虚函数接口
        virtual int calculateOutput(int inputSignal) const = 0;
        virtual bool canReceiveSignal(int signal) const = 0;
        virtual int getDelay() const { return delay; }
        virtual Position getPosition() const { return position; }
        
        // 三路比较运算符
        auto operator<=>(const RedstoneComponentBase& other) const {
            return position <=> other.position;
        }
    };

    // 红石线组件
    class RedstoneWire : public RedstoneComponentBase {
    public:
        explicit RedstoneWire(Position pos) 
            : RedstoneComponentBase(pos, ComponentType::WIRE) {}
        
        int calculateOutput(int inputSignal) const override {
            return std::max(0, std::min(15, inputSignal));
        }
        
        bool canReceiveSignal(int signal) const override {
            return signal >= 0 && signal <= 15;
        }
    };

    // 拉杆组件
    class Lever : public RedstoneComponentBase {
    public:
        explicit Lever(Position pos, bool powered = false) 
            : RedstoneComponentBase(pos, ComponentType::LEVER), 
              powered_(powered) {}
        
        int calculateOutput(int inputSignal) const override {
            return powered_ ? 15 : 0;
        }
        
        bool canReceiveSignal(int signal) const override {
            return false; // 拉杆本身不接收外部信号
        }
        
        void toggle() { powered_ = !powered_; }
        void setPowered(bool powered) { powered_ = powered; }
        bool isPowered() const { return powered_; }
        
    private:
        bool powered_;
    };

    // 按钮组件
    class Button : public RedstoneComponentBase {
    public:
        explicit Button(Position pos, bool pressed = false) 
            : RedstoneComponentBase(pos, ComponentType::BUTTON),
              pressed_(pressed) {}
        
        int calculateOutput(int inputSignal) const override {
            return pressed_ ? 15 : 0;
        }
        
        bool canReceiveSignal(int signal) const override {
            return false; // 按钮本身不接收外部信号
        }
        
        void press() { pressed_ = true; }
        void release() { pressed_ = false; }
        bool isPressed() const { return pressed_; }
        
    private:
        bool pressed_;
    };

    // 红石火把组件
    class RedstoneTorch : public RedstoneComponentBase {
    public:
        explicit RedstoneTorch(Position pos, bool powered = false) 
            : RedstoneComponentBase(pos, ComponentType::TORCH), 
              powered_(powered) {}
        
        int calculateOutput(int inputSignal) const override {
            return powered_ ? 0 : 15; // 反向器
        }
        
        bool canReceiveSignal(int signal) const override {
            return true; // 火把可以接收任何信号
        }
        
        void setPowered(bool powered) { powered_ = powered; }
        bool isPowered() const { return powered_; }
        
    private:
        bool powered_;
    };

    // 组件注册表
    class ComponentRegistry {
    public:
        // 注册组件
        void registerComponent(std::unique_ptr<RedstoneComponentBase> component) {
            std::lock_guard lock(registryMutex_);
            auto pos = component->getPosition();
            components_[pos] = std::move(component);
        }
        
        // 获取组件
        RedstoneComponentBase* getComponent(const Position& pos) {
            std::lock_guard lock(registryMutex_);
            auto it = components_.find(pos);
            return (it != components_.end()) ? it->second.get() : nullptr;
        }
        
        // 获取所有组件
        std::vector<RedstoneComponentBase*> getAllComponents() {
            std::lock_guard lock(registryMutex_);
            std::vector<RedstoneComponentBase*> result;
            for (auto& pair : components_) {
                result.push_back(pair.second.get());
            }
            return result;
        }

    private:
        std::map<Position, std::unique_ptr<RedstoneComponentBase>> components_;
        std::mutex registryMutex_;
    };

    // 主要红石引擎
    class RedstoneEngine {
    public:
        static RedstoneEngine& getInstance();
        
        // 生命周期管理
        RedstoneEngine(const RedstoneEngine&) = delete;
        RedstoneEngine& operator=(const RedstoneEngine&) = delete;
        RedstoneEngine(RedstoneEngine&&) = delete;
        RedstoneEngine& operator=(RedstoneEngine&&) = delete;
        
        // 核心接口
        void tick();
        void registerComponent(std::unique_ptr<RedstoneComponentBase> component);
        void applySignal(const Position& pos, int signal); // 新增：应用信号到指定位置
        void updateComponent(const Position& pos, int inputSignal);
        
        // 性能监控
        struct PerformanceStats {
            uint64_t totalComponents;
            uint64_t signalsProcessed;
            uint64_t circuitTicks;
            double avgProcessingTimeMs;
            uint64_t memoryUsageBytes;
        };
        
        PerformanceStats getPerformanceStats() const;
        void resetPerformanceStats();
        
        // 获取当前tick
        uint64_t getCurrentTick() const { return currentTick_.load(); }

    private:
        RedstoneEngine() = default;
        ~RedstoneEngine() = default;
        
        void propagateSignal(const Position& startPos, int signal);
        
        ComponentRegistry registry_;
        PerformanceStats stats_{};
        std::atomic<uint64_t> currentTick_{0};
        std::atomic<uint64_t> signalsProcessed_{0};
    };

} // namespace lattice::redstone

#endif // LATTICE_SIMPLE_REDSTONE_ENGINE_HPP