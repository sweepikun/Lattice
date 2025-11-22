#include "redstone_engine.hpp"
#include <iostream>
#include <thread>
#include <future>
#include <cmath>

namespace lattice::redstone {

    // ========== SignalScheduler 实现 ==========
    
    void SignalScheduler::processSignal(Position pos, int signal) {
        // 获取RedstoneEngine实例并应用信号
        RedstoneEngine::getInstance().applySignal(pos, signal);
        
        #ifdef DEBUG_REDSTONE
        std::cout << "Processed signal at (" << pos.x << "," << pos.y << "," << pos.z 
                  << ") with strength " << signal << " at tick " << currentTick_.load() << std::endl;
        #endif
    }

    // ========== ComponentRegistry 范围视图优化 ==========
    
    // 为方便使用，提供便捷函数
    inline std::vector<RedstoneComponentBase*> getComponentsInRegion(
        ComponentRegistry& registry, 
        const Position& center, 
        int radius) {
        
        std::vector<RedstoneComponentBase*> result;
        
        // 使用范围视图优化查询
        auto components = registry.getComponentsInRange(center, radius);
        std::ranges::copy(components, std::back_inserter(result));
        
        return result;
    }

    // ========== RedstoneEngine 实现 ==========
    
    RedstoneEngine& RedstoneEngine::getInstance() {
        static RedstoneEngine instance;
        return instance;
    }

    void RedstoneEngine::tick() {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // 推进调度器
        scheduler_.tick();
        
        // 更新性能统计
        stats_.circuitTicks++;
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        // 更新平均处理时间（使用移动平均）
        double currentAvg = stats_.avgProcessingTimeMs;
        stats_.avgProcessingTimeMs = (currentAvg * 0.9) + (duration.count() / 1000.0 * 0.1);
    }

    void RedstoneEngine::registerComponent(std::unique_ptr<RedstoneComponentBase> component) {
        if (!component) return;
        
        registry_.registerComponent(std::move(component));
        stats_.totalComponents++;
    }

    void RedstoneEngine::updateComponent(const Position& pos, int inputSignal) {
        auto component = registry_.getComponent(pos);
        if (!component) return;
        
        int outputSignal = component->calculateOutput(inputSignal);
        
        // 如果信号发生变化，启动异步传播
        if (outputSignal != component->currentSignal) {
            component->currentSignal = outputSignal;
            
            // 异步传播避免栈溢出
            propagateSignalAsync(pos, outputSignal);
            stats_.signalsProcessed++;
        }
    }

    void RedstoneEngine::applySignal(const Position& pos, int signal) {
        auto component = registry_.getComponent(pos);
        if (!component) {
            #ifdef DEBUG_REDSTONE
            std::cerr << "Warning: No component found at position (" 
                      << pos.x << "," << pos.y << "," << pos.z << ")" << std::endl;
            #endif
            return;
        }
        
        // 检查组件是否能够接收此信号
        if (!component->canReceiveSignal(signal)) {
            #ifdef DEBUG_REDSTONE
            std::cout << "Component at (" << pos.x << "," << pos.y << "," << pos.z 
                      << ") cannot receive signal " << signal << std::endl;
            #endif
            return;
        }
        
        // 计算输出信号
        int outputSignal = component->calculateOutput(signal);
        
        // 如果输出信号发生变化，更新组件状态并传播
        if (outputSignal != component->currentSignal) {
            component->currentSignal = outputSignal;
            stats_.signalsProcessed++;
            
            // 立即传播到相邻组件
            propagateSignalAsync(pos, outputSignal);
            
            #ifdef DEBUG_REDSTONE
            std::cout << "Applied signal " << signal << " at (" 
                      << pos.x << "," << pos.y << "," << pos.z 
                      << "), output: " << outputSignal << std::endl;
            #endif
        }
    }

    std::vector<RedstoneComponentBase*> RedstoneEngine::queryComponentsInRange(
        const Position& center, int radius) {
        return getComponentsInRegion(registry_, center, radius);
    }

    // ========== 信号传播优化 ==========
    
    void RedstoneEngine::propagateSignalAsync(const Position& startPos, int signal) {
        // 使用范围视图找到受影响的组件
        auto affectedComponents = findAffectedComponents(startPos, 15); // 最大传播距离15
        
        // 并行处理信号传播
        std::for_each(std::execution::par, affectedComponents.begin(), affectedComponents.end(),
            [this, signal](const Position& pos) {
                auto component = registry_.getComponent(pos);
                if (component && component->canReceiveSignal(signal)) {
                    int output = component->calculateOutput(signal);
                    
                    // 调度延迟输出（基于组件延迟）
                    scheduler_.scheduleEvent(pos, output, component->getDelay());
                }
            });
    }

    std::vector<Position> RedstoneEngine::findAffectedComponents(
        const Position& center, int radius) {
        std::vector<Position> affected;
        
        // 获取范围内非线路组件
        auto filter_lambda = [](RedstoneComponentBase* comp) {
            return comp->type != ComponentType::WIRE; // 排除基础线路
        };
        auto components = registry_.getComponentsInRange(center, radius, filter_lambda);
        
        // 转换为位置向量
        std::ranges::transform(components, std::back_inserter(affected),
            [](RedstoneComponentBase* comp) { return comp->getPosition(); });
        
        return affected;
    }

    // ========== 性能监控 ==========
    
    RedstoneEngine::PerformanceStats RedstoneEngine::getPerformanceStats() const {
        // 计算内存使用量（估算）
        stats_.memoryUsageBytes = stats_.totalComponents * sizeof(RedstoneComponentBase) * 1.5; // 预留开销
        
        return stats_;
    }

    void RedstoneEngine::resetPerformanceStats() {
        stats_ = PerformanceStats{};
    }

    // ========== 优雅降级机制 ==========
    
    bool RedstoneEngine::validateBehavior(
        const std::vector<int>& javaSignals, 
        const std::vector<int>& nativeSignals) {
        
        // 严格比较关键点
        if (javaSignals.size() != nativeSignals.size()) return false;
        
        for (size_t i = 0; i < javaSignals.size(); i++) {
            if (std::abs(javaSignals[i] - nativeSignals[i]) > 0.1f) {
                return false;
            }
        }
        
        return true;
    }

    void RedstoneEngine::enableGracefulDegradation() {
        gracefulDegradation_.store(true);
        std::cerr << "Redstone engine enabled graceful degradation - falling back to vanilla behavior" << std::endl;
    }

    // ========== 便利函数实现 ==========
    
    namespace utils {
        
        // 计算两点之间的距离
        inline double calculateDistance(const Position& a, const Position& b) {
            int dx = a.x - b.x;
            int dy = a.y - b.y;
            int dz = a.z - b.z;
            return std::sqrt(dx*dx + dy*dy + dz*dz);
        }
        
        // 检查位置是否在立方体范围内
        inline bool isInCubeRange(const Position& center, const Position& target, int radius) {
            return std::abs(target.x - center.x) <= radius &&
                   std::abs(target.y - center.y) <= radius &&
                   std::abs(target.z - center.z) <= radius;
        }
        
        // 批量创建信号事件
        inline std::vector<SignalEvent> createSignalBatch(
            const std::vector<std::tuple<Position, int, int>>& signals) {
            std::vector<SignalEvent> events;
            events.reserve(signals.size());
            
            for (const auto& [pos, signal, delay] : signals) {
                events.push_back({0, pos, signal}); // tick会在调度时设置
            }
            
            return events;
        }
        
    } // namespace utils

} // namespace lattice::redstone