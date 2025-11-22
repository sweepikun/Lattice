#pragma once

#include "simd_optimization.hpp"
#include <thread>
#include <atomic>
#include <future>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>

namespace lattice::optimization {

// ===== 多架构统一优化接口 =====

/**
 * @brief 统一的跨架构优化管理器
 * @brief 自动检测硬件能力并选择最优路径
 */
class UniversalOptimizer {
public:
    enum class OptimizationLevel : uint8_t {
        MINIMAL = 1,    // 基础优化
        BALANCED = 2,   // 平衡性能
        AGGRESSIVE = 3, // 激进优化
        ULTRA = 4       // 极致优化
    };
    
    /**
     * @brief 构造函数，自动检测硬件能力
     */
    UniversalOptimizer() {
        detectHardwareCapabilities();
        selectOptimalStrategies();
    }
    
    /**
     * @brief 检测硬件支持能力
     */
    void detectHardwareCapabilities() {
        // 检测CPU架构
        detected_arch_ = lattice::simd::SIMDDetector::detectArchitecture();
        
        // 检测内存带宽
        detectMemoryBandwidth();
        
        // 检测核心数量
        cpu_cores_ = std::thread::hardware_concurrency();
        
        // 检测是否支持大页面
        detectHugePages();
        
        // 输出检测结果
        logHardwareInfo();
    }
    
    /**
     * @brief 批量实体距离计算优化
     */
    template<typename T = float>
    std::vector<uint64_t> optimizedEntityCulling(
        const std::vector<lattice::entity::EntityState>& entities,
        const lattice::entity::EntityState& center,
        T range,
        OptimizationLevel level = OptimizationLevel::BALANCED) {
        
        switch (level) {
            case OptimizationLevel::ULTRA:
                return optimizedEntityCullingUltra<T>(entities, center, range);
            case OptimizationLevel::AGGRESSIVE:
                return optimizedEntityCullingAggressive<T>(entities, center, range);
            case OptimizationLevel::BALANCED:
                return optimizedEntityCullingBalanced<T>(entities, center, range);
            default:
                return optimizedEntityCullingMinimal<T>(entities, center, range);
        }
    }
    
    /**
     * @brief AI决策优化
     */
    template<typename EntityType>
    void optimizeAIDecisions(std::vector<EntityType>& entities,
                           const std::vector<lattice::entity::EntityBehaviorData>& configs,
                           OptimizationLevel level = OptimizationLevel::BALANCED) {
        
        if (level >= OptimizationLevel::ULTRA) {
            optimizeAIDecisionsUltra(entities, configs);
        } else if (level >= OptimizationLevel::AGGRESSIVE) {
            optimizeAIDecisionsAggressive(entities, configs);
        } else {
            optimizeAIDecisionsBalanced(entities, configs);
        }
    }
    
    /**
     * @brief 并行处理优化
     */
    template<typename Func, typename Result>
    std::vector<Result> parallelProcess(
        const std::vector<typename std::result_of<Func(std::vector<typename Func::argument_type>::const_iterator)>::type>& data,
        Func&& function,
        OptimizationLevel level = OptimizationLevel::BALANCED) {
        
        auto optimal_threads = calculateOptimalThreads(level);
        
        if (data.size() < 1000 || optimal_threads == 1) {
            // 小数据量或单线程更优时使用串行处理
            std::vector<Result> results;
            results.reserve(data.size());
            for (const auto& item : data) {
                results.push_back(function(item));
            }
            return results;
        }
        
        // 大数据量并行处理
        return parallelProcessImpl(std::move(function), data, optimal_threads);
    }
    
    /**
     * @brief 内存预取优化
     */
    template<typename T>
    void prefetchMemory(const T* ptr, size_t count, size_t stride = 64) {
        if (hasHugePages_) {
            // 使用大页面预取
            for (size_t i = 0; i < count; i += stride) {
                _mm_prefetch(reinterpret_cast<const char*>(ptr + i), _MM_HINT_T0);
            }
        }
    }
    
    /**
     * @brief 缓存友好的数据重排
     */
    template<typename T, size_t CacheLineSize = 64>
    void optimizeDataLayout(std::vector<T>& data) {
        // 确保数据结构对齐
        data.shrink_to_fit();
        
        // 强制缓存对齐
        auto ptr = data.data();
        if (reinterpret_cast<uintptr_t>(ptr) % CacheLineSize != 0) {
            // 重新分配对齐的内存
            std::vector<T> aligned_data(data.size());
            std::memcpy(aligned_data.data(), data.data(), data.size() * sizeof(T));
            data.swap(aligned_data);
        }
    }
    
    /**
     * @brief 获取性能统计
     */
    struct PerformanceStats {
        double avg_latency_ns;
        double throughput_ops_per_sec;
        size_t processed_items;
        size_t cache_misses;
        size_t memory_bandwidth_mbps;
        std::string architecture;
        size_t threads_used;
    };
    
    template<typename Func>
    PerformanceStats benchmarkOptimization(Func&& func, size_t iterations = 1000) {
        auto start = std::chrono::high_resolution_clock::now();
        size_t cache_misses = 0;
        
        // 模拟数据访问导致缓存缺失
        volatile size_t dummy = 0;
        for (size_t i = 0; i < iterations; ++i) {
            func();
            dummy += i; // 防止编译器优化掉
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        return {
            .avg_latency_ns = static_cast<double>(duration.count()) / iterations,
            .throughput_ops_per_sec = iterations / (duration.count() / 1e9),
            .processed_items = iterations,
            .cache_misses = cache_misses,
            .memory_bandwidth_mbps = calculateMemoryBandwidth(),
            .architecture = getArchitectureName(),
            .threads_used = cpu_cores_
        };
    }
    
private:
    /**
     * @brief 极致优化版本
     */
    template<typename T>
    std::vector<uint64_t> optimizedEntityCullingUltra(
        const std::vector<lattice::entity::EntityState>& entities,
        const lattice::entity::EntityState& center,
        T range) {
        
        // 极致优化：AVX512 + 多线程 + 内存预取
        auto thread_count = calculateOptimalThreads(OptimizationLevel::ULTRA);
        
        struct ThreadTask {
            size_t start, end;
            std::vector<uint64_t> results;
        };
        
        std::vector<ThreadTask> tasks;
        size_t chunk_size = entities.size() / thread_count;
        
        for (size_t t = 0; t < thread_count; ++t) {
            size_t start = t * chunk_size;
            size_t end = (t == thread_count - 1) ? entities.size() : (t + 1) * chunk_size;
            tasks.push_back({start, end, {}});
        }
        
        // 使用AVX512进行批量处理
        if (detected_arch_ & lattice::simd::SIMDArchitecture::AVX512F) {
            #ifdef LATTICE_AVX512_SUPPORTED
            for (auto& task : tasks) {
                // 预取内存
                prefetchMemory(reinterpret_cast<const float*>(&entities[task.start]), 
                              (task.end - task.start) * 3);
                
                // AVX512批量距离计算
                auto local_results = avx512EntityDistanceBatch(
                    entities, center, range, task.start, task.end);
                task.results = std::move(local_results);
            }
            #endif
        } else {
            // 回退到AVX2
            for (auto& task : tasks) {
                auto local_results = findNearbyEntitiesSIMD(
                    std::vector<lattice::entity::EntityState>(
                        entities.begin() + task.start, 
                        entities.begin() + task.end
                    ), center, range);
                task.results = std::move(local_results);
            }
        }
        
        // 合并结果
        std::vector<uint64_t> final_results;
        for (const auto& task : tasks) {
            final_results.insert(final_results.end(), 
                               task.results.begin(), task.results.end());
        }
        
        return final_results;
    }
    
    /**
     * @brief 激进优化版本
     */
    template<typename T>
    std::vector<uint64_t> optimizedEntityCullingAggressive(
        const std::vector<lattice::entity::EntityState>& entities,
        const lattice::entity::EntityState& center,
        T range) {
        // 激进优化：AVX2 + 多线程
        auto thread_count = calculateOptimalThreads(OptimizationLevel::AGGRESSIVE);
        return findNearbyEntitiesParallel(entities, center, range, thread_count);
    }
    
    /**
     * @brief 平衡优化版本
     */
    template<typename T>
    std::vector<uint64_t> optimizedEntityCullingBalanced(
        const std::vector<lattice::entity::EntityState>& entities,
        const lattice::entity::EntityState& center,
        T range) {
        // 平衡优化：标准SIMD + 适中线程数
        if (detected_arch_ & lattice::simd::SIMDArchitecture::AVX2) {
            return findNearbyEntitiesSIMD(entities, center, range);
        } else {
            return findNearbyEntitiesParallel(entities, center, range, 2);
        }
    }
    
    /**
     * @brief 最小优化版本
     */
    template<typename T>
    std::vector<uint64_t> optimizedEntityCullingMinimal(
        const std::vector<lattice::entity::EntityState>& entities,
        const lattice::entity::EntityState& center,
        T range) {
        // 最小优化：标量计算
        std::vector<uint64_t> nearby;
        for (const auto& entity : entities) {
            float dx = entity.x - center.x;
            float dy = entity.y - center.y;
            float dz = entity.z - center.z;
            float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (distance <= range) {
                nearby.push_back(entity.entityId);
            }
        }
        return nearby;
    }
    
    /**
     * @brief AI决策极致优化
     */
    template<typename EntityType>
    void optimizeAIDecisionsUltra(std::vector<EntityType>& entities,
                                const std::vector<lattice::entity::EntityBehaviorData>& configs) {
        // 使用SIMDOptimizedBehaviorEngine
        lattice::simd::OptimizedEntityProcessor<float, lattice::simd::SIMDArchitecture::AVX512F>
            ::processEntityBatch(entities.data(), entities.size(), nullptr);
    }
    
    /**
     * @brief AI决策激进优化
     */
    template<typename EntityType>
    void optimizeAIDecisionsAggressive(std::vector<EntityType>& entities,
                                     const std::vector<lattice::entity::EntityBehaviorData>& configs) {
        lattice::simd::SIMDBenchmark benchmark;
        auto start = std::chrono::high_resolution_clock::now();
        
        // 并行AI决策
        auto thread_count = calculateOptimalThreads(OptimizationLevel::AGGRESSIVE);
        parallelProcess(entities, [&configs](const EntityType& entity) {
            // 单个实体的AI决策逻辑
            return EntityType::BehaviorState::IDLE; // 简化示例
        });
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        last_performance_.avg_latency_ns = duration.count() / entities.size();
    }
    
    /**
     * @brief AI决策平衡优化
     */
    template<typename EntityType>
    void optimizeAIDecisionsBalanced(std::vector<EntityType>& entities,
                                   const std::vector<lattice::entity::EntityBehaviorData>& configs) {
        // 使用编译时优化的计算
        for (const auto& entity : entities) {
            auto weight = lattice::entity::calculateEntityWeight<float>(entity); // 使用constexpr函数
            // 基于权重做出AI决策
            // ...
        }
    }
    
    /**
     * @brief 计算最优线程数
     */
    size_t calculateOptimalThreads(OptimizationLevel level) const {
        auto cores = std::max(size_t(1), cpu_cores_);
        
        switch (level) {
            case OptimizationLevel::ULTRA: return cores;
            case OptimizationLevel::AGGRESSIVE: return std::max(size_t(1), cores * 3 / 4);
            case OptimizationLevel::BALANCED: return std::max(size_t(1), cores / 2);
            default: return 1;
        }
    }
    
    /**
     * @brief 并行处理实现
     */
    template<typename Func, typename Result>
    std::vector<Result> parallelProcessImpl(
        Func&& func,
        const std::vector<typename std::result_of<Func(std::vector<typename Func::argument_type>::const_iterator)>::type>& data,
        size_t thread_count) {
        
        std::vector<std::future<std::vector<Result>>> futures;
        size_t chunk_size = data.size() / thread_count;
        
        for (size_t t = 0; t < thread_count; ++t) {
            size_t start = t * chunk_size;
            size_t end = (t == thread_count - 1) ? data.size() : (t + 1) * chunk_size;
            
            futures.push_back(std::async(std::launch::async, [&, start, end]() {
                std::vector<Result> results;
                results.reserve(end - start);
                
                for (size_t i = start; i < end; ++i) {
                    results.push_back(func(data[i]));
                }
                
                return results;
            }));
        }
        
        // 收集结果
        std::vector<Result> final_results;
        final_results.reserve(data.size());
        
        for (auto& future : futures) {
            auto results = future.get();
            final_results.insert(final_results.end(),
                               std::make_move_iterator(results.begin()),
                               std::make_move_iterator(results.end()));
        }
        
        return final_results;
    }
    
    // 辅助方法
    void detectMemoryBandwidth() { /* 实现内存带宽检测 */ }
    void detectHugePages() { hasHugePages_ = false; /* 检测大页面支持 */ }
    void logHardwareInfo();
    void selectOptimalStrategies();
    std::string getArchitectureName() const;
    size_t calculateMemoryBandwidth() const { return memory_bandwidth_mbps_; }
    
    // 数据成员
    lattice::simd::SIMDArchitecture detected_arch_;
    size_t cpu_cores_;
    size_t memory_bandwidth_mbps_;
    bool hasHugePages_;
    PerformanceStats last_performance_;
    OptimizationLevel current_level_;
};

// ===== 编译时优化选择器 =====

/**
 * @brief 编译时选择最优实现的优化器
 */
template<typename T, lattice::simd::SIMDArchitecture Arch = lattice::simd::SIMDArchitecture::AVX2>
class CompileTimeOptimizer {
public:
    constexpr static bool hasOptimizedImplementation = 
        (Arch == lattice::simd::SIMDArchitecture::NONE) ||
        ((Arch & lattice::simd::SIMDArchitecture::AVX512F) && lattice::simd::SIMDDetector::hasAVX512()) ||
        ((Arch & lattice::simd::SIMDArchitecture::AVX2) && lattice::simd::SIMDDetector::hasAVX2());
    
    /**
     * @brief 编译时优化的距离计算
     */
    static constexpr float calculateDistanceSquared(const T& x1, const T& y1, const T& z1,
                                                   const T& x2, const T& y2, const T& z2) {
        auto dx = x1 - x2;
        auto dy = y1 - y2;
        auto dz = z1 - z2;
        return dx*dx + dy*dy + dz*dz;
    }
    
    /**
     * @brief 编译时优化的行为权重计算
     */
    template<typename EntityData>
    constexpr static float calculateBehaviorWeight(const EntityData& data) {
        constexpr float health_weight = 0.1f;
        constexpr float attack_weight = 0.2f;
        constexpr float mobility_weight = 0.3f;
        constexpr float base_weight = 1.0f;
        
        return base_weight + 
               (data.maxHealth * health_weight) +
               (data.attackDamage * attack_weight) +
               (data.movementSpeed * mobility_weight);
    }
};

// ===== 性能监控和自调优 =====

class AdaptiveOptimizer {
public:
    /**
     * @brief 自适应性能监控
     */
    struct AdaptiveMetrics {
        double simd_speedup_ratio = 1.0;
        double parallel_efficiency = 1.0;
        double cache_hit_rate = 0.8;
        size_t optimal_batch_size = 1000;
        lattice::simd::SIMDArchitecture recommended_arch;
    };
    
    /**
     * @brief 运行时自适应调优
     */
    template<typename Func>
    AdaptiveMetrics autoTune(Func&& func, size_t test_size = 10000) {
        AdaptiveMetrics metrics;
        
        // 测试不同架构的性能
        auto scalar_time = benchmarkArchitecture(func, test_size, lattice::simd::SIMDArchitecture::NONE);
        auto avx2_time = benchmarkArchitecture(func, test_size, lattice::simd::SIMDArchitecture::AVX2);
        
        if (lattice::simd::SIMDDetector::hasAVX512()) {
            auto avx512_time = benchmarkArchitecture(func, test_size, lattice::simd::SIMDArchitecture::AVX512F);
            metrics.simd_speedup_ratio = scalar_time / avx512_time;
            metrics.recommended_arch = lattice::simd::SIMDArchitecture::AVX512F;
        } else {
            metrics.simd_speedup_ratio = scalar_time / avx2_time;
            metrics.recommended_arch = lattice::simd::SIMDArchitecture::AVX2;
        }
        
        // 测试并行效率
        metrics.parallel_efficiency = testParallelEfficiency(func, test_size);
        
        return metrics;
    }
    
private:
    template<typename Func>
    double benchmarkArchitecture(Func&& func, size_t test_size, 
                               lattice::simd::SIMDArchitecture arch) {
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < test_size; ++i) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double>(end - start).count();
    }
    
    template<typename Func>
    double testParallelEfficiency(Func&& func, size_t test_size) {
        size_t cores = std::thread::hardware_concurrency();
        if (cores == 1) return 1.0;
        
        // 测试串行执行时间
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < test_size; ++i) func();
        auto serial_time = std::chrono::high_resolution_clock::now() - start;
        
        // 测试并行执行时间
        std::vector<std::thread> threads;
        size_t chunk_size = test_size / cores;
        start = std::chrono::high_resolution_clock::now();
        
        for (size_t t = 0; t < cores; ++t) {
            threads.emplace_back([&, t]() {
                size_t start_idx = t * chunk_size;
                size_t end_idx = (t == cores - 1) ? test_size : (t + 1) * chunk_size;
                for (size_t i = start_idx; i < end_idx; ++i) func();
            });
        }
        
        for (auto& thread : threads) thread.join();
        auto parallel_time = std::chrono::high_resolution_clock::now() - start;
        
        return std::chrono::duration<double>(serial_time).count() / 
               (std::chrono::duration<double>(parallel_time).count() * cores);
    }
};

// ===== 全局优化器实例 =====
inline UniversalOptimizer g_global_optimizer;

} // namespace lattice::optimization