#include "simple_redstone_engine.hpp"
#include <iostream>
#include <thread>
#include <future>
#include <cmath>

namespace lattice::redstone {

    // ========== RedstoneEngine 实现 ==========
    
    RedstoneEngine& RedstoneEngine::getInstance() {
        static RedstoneEngine instance;
        return instance;
    }

    void RedstoneEngine::tick() {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // 推进tick计数器
        currentTick_.fetch_add(1, std::memory_order_relaxed);
        
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
            signalsProcessed_.fetch_add(1, std::memory_order_relaxed);
            stats_.signalsProcessed = signalsProcessed_.load();
            
            // 传播到相邻组件
            propagateSignal(pos, outputSignal);
            
            #ifdef DEBUG_REDSTONE
            std::cout << "Applied signal " << signal << " at (" 
                      << pos.x << "," << pos.y << "," << pos.z 
                      << "), output: " << outputSignal << std::endl;
            #endif
        }
    }

    void RedstoneEngine::updateComponent(const Position& pos, int inputSignal) {
        auto component = registry_.getComponent(pos);
        if (!component) return;
        
        int outputSignal = component->calculateOutput(inputSignal);
        
        // 如果信号发生变化，启动信号传播
        if (outputSignal != component->currentSignal) {
            component->currentSignal = outputSignal;
            signalsProcessed_.fetch_add(1, std::memory_order_relaxed);
            stats_.signalsProcessed = signalsProcessed_.load();
            
            // 传播信号
            propagateSignal(pos, outputSignal);
        }
    }

    void RedstoneEngine::propagateSignal(const Position& startPos, int signal) {
        // 简单的邻近传播（只传播到相邻6个位置）
        const int dx[] = {1, -1, 0, 0, 0, 0};
        const int dy[] = {0, 0, 1, -1, 0, 0};
        const int dz[] = {0, 0, 0, 0, 1, -1};
        
        for (int i = 0; i < 6; i++) {
            Position neighborPos(startPos.x + dx[i], startPos.y + dy[i], startPos.z + dz[i]);
            
            auto neighbor = registry_.getComponent(neighborPos);
            if (neighbor && neighbor->canReceiveSignal(signal)) {
                // 立即传播（实际游戏中可能有延迟）
                applySignal(neighborPos, signal);
            }
        }
    }

    // ========== 性能监控 ==========
    
    RedstoneEngine::PerformanceStats RedstoneEngine::getPerformanceStats() const {
        PerformanceStats stats = stats_;
        // 计算内存使用量（估算）
        stats.memoryUsageBytes = stats_.totalComponents * sizeof(RedstoneComponentBase) * 1.5; // 预留开销
        
        return stats;
    }

    void RedstoneEngine::resetPerformanceStats() {
        stats_ = PerformanceStats{};
        signalsProcessed_.store(0);
    }

} // namespace lattice::redstone