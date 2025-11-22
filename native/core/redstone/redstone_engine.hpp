#ifndef LATTICE_REDSTONE_ENGINE_HPP
#define LATTICE_REDSTONE_ENGINE_HPP

#include <memory>
#include <vector>
#include <concepts>
#include <coroutine>
#include <ranges>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <execution>
#include <set>
#include <map>
#include "../net/memory_arena.hpp"
#include "../net/native_compressor.hpp"

namespace lattice::redstone {

    // ========== 核心概念定义 ==========
    
    // 红石组件概念约束
    template<typename T>
    concept RedstoneComponent = requires(T comp, int signal) {
        { comp.calculateOutput(signal) } -> std::convertible_to<int>;
        { comp.getDelay() } -> std::convertible_to<int>;
        { comp.canReceiveSignal(signal) } -> std::convertible_to<bool>;
        { comp.getPosition() } -> std::convertible_to<std::tuple<int, int, int>>;
    };

    // 位置结构
    struct Position {
        int x, y, z;
        
        Position() : x(0), y(0), z(0) {}
        Position(int x, int y, int z) : x(x), y(y), z(z) {}
        
        auto operator<=>(const Position& other) const = default;
        bool operator==(const Position& other) const = default;
    };

    // 信号事件结构
    struct SignalEvent {
        uint64_t tick;
        Position pos;
        int signal;
        
        auto operator<=>(const SignalEvent& other) const = default;
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

    // 组件基类
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

    // ========== 协程信号传播 ==========
    
    // 前向声明RedstoneEngine
    class RedstoneEngine;
    
    // 异步信号传播协程
    struct AsyncSignal {
        struct promise_type {
            AsyncSignal get_return_object() noexcept { return {}; }
            std::suspend_never initial_suspend() noexcept { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept {}
        };
        
        static AsyncSignal propagateWithDelay(Position pos, int signal, int delayTicks) {
            co_await std::suspend_always{}; // 挂起
            
            // 使用 chrono 处理精确延迟
            auto delayNs = std::chrono::milliseconds(delayTicks * 50); // 1 tick = 50ms
            std::this_thread::sleep_for(delayNs);
            
            // 恢复执行 - 注意：这里需要RedstoneEngine完整定义
            // 暂时使用占位符实现
            // RedstoneEngine::getInstance().applySignal(pos, signal);
        }
    };

    // ========== 信号调度器 ==========
    
    class SignalScheduler {
    public:
        SignalScheduler() : currentTick_(0) {}
        
        // 调度信号事件
        void scheduleEvent(Position pos, int signal, int delay) {
            auto tick = currentTick_.load() + delay;
            std::lock_guard lock(eventsMutex_);
            pendingEvents_.insert({tick, pos, signal});
        }
        
        // 处理事件（非阻塞）
        void processEvents() {
            auto now = currentTick_.load();
            std::vector<SignalEvent> toProcess;
            
            {
                std::lock_guard lock(eventsMutex_);
                auto it = pendingEvents_.begin();
                while (it != pendingEvents_.end() && it->tick <= now) {
                    toProcess.push_back(*it);
                    it = pendingEvents_.erase(it);
                }
            }
            
            // 并行处理非冲突事件
            std::for_each(std::execution::par, toProcess.begin(), toProcess.end(),
                [this](const SignalEvent& event) {
                    processSignal(event.pos, event.signal);
                });
        }
        
        // Tick推进
        void tick() {
            currentTick_.fetch_add(1, std::memory_order_relaxed);
            processEvents();
        }
        
        uint64_t getCurrentTick() const { return currentTick_.load(); }

    private:
        std::atomic<uint64_t> currentTick_;
        std::multiset<SignalEvent> pendingEvents_;
        std::mutex eventsMutex_;
        
        void processSignal(Position pos, int signal);
    };

    // ========== 组件注册表 ==========
    
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
        
        // 移除组件
        void unregisterComponent(const Position& pos) {
            std::lock_guard lock(registryMutex_);
            components_.erase(pos);
        }
        
        // 获取所有组件（使用范围视图）
        auto getComponentsInRange(const Position& center, int radius) {
            // 默认返回所有范围内的组件
            auto allComponents = components_ | std::views::transform([](auto& pair) {
                return pair.second.get();
            });
            
            return allComponents | std::views::filter([center, radius](RedstoneComponentBase* comp) {
                const auto& pos = comp->getPosition();
                int dx = pos.x - center.x;
                int dy = pos.y - center.y;
                int dz = pos.z - center.z;
                return (dx*dx + dy*dy + dz*dz) <= (radius*radius);
            });
        }
        
        template<std::ranges::viewable_range R>
        auto getComponentsInRange(const Position& center, int radius, R&& filter) {
            auto allComponents = components_ | std::views::transform([](auto& pair) {
                return pair.second.get();
            });
            
            auto inRange = allComponents | std::views::filter([center, radius](RedstoneComponentBase* comp) {
                const auto& pos = comp->getPosition();
                int dx = pos.x - center.x;
                int dy = pos.y - center.y;
                int dz = pos.z - center.z;
                return (dx*dx + dy*dy + dz*dz) <= (radius * radius);
            });
            
            // 过滤范围内的组件
            std::vector<RedstoneComponentBase*> filtered;
            for (auto* component : inRange) {
                if (filter(component)) {
                    filtered.push_back(component);
                }
            }
            return filtered;
        }

    private:
        std::map<Position, std::unique_ptr<RedstoneComponentBase>> components_;
        std::mutex registryMutex_;
    };

    // ========== 主要红石引擎 ==========
    
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
        void updateComponent(const Position& pos, int inputSignal);
        void applySignal(const Position& pos, int signal); // 新增：应用信号到指定位置
        std::vector<RedstoneComponentBase*> queryComponentsInRange(const Position& center, int radius);
        
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
        uint64_t getCurrentTick() const { return scheduler_.getCurrentTick(); }
        
        // 检查是否启用优雅降级
        bool isGracefulDegradationEnabled() const { return gracefulDegradation_.load(); }
        
        // 优雅降级机制 - 公开给JNI使用
        bool validateBehavior(const std::vector<int>& javaSignals, 
                            const std::vector<int>& nativeSignals);
        void enableGracefulDegradation();

    private:
        RedstoneEngine() = default;
        
        SignalScheduler scheduler_;
        ComponentRegistry registry_;
        PerformanceStats stats_;
        std::atomic<bool> gracefulDegradation_{false};
        
        // 信号传播优化
        void propagateSignalAsync(const Position& startPos, int signal);
        std::vector<Position> findAffectedComponents(const Position& center, int radius);
    };

} // namespace lattice::redstone

#endif // LATTICE_REDSTONE_ENGINE_HPP