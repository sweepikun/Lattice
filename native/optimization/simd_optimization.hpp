#pragma once

#include <immintrin.h>
#include <cpuid.h>
#include <iostream>
#include <type_traits>
#include <vector>
#include <array>
#include <cmath>
#include <bit>

namespace lattice::simd {

// ===== C++20 Concepts for SIMD =====

/**
 * @brief Concept for SIMD compatible types
 */
template<typename T>
concept SIMDType = std::is_arithmetic_v<T> && 
                   (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8);

/**
 * @brief Vector width concept 
 */
template<typename T, size_t Width>
concept VectorWidth = SIMDType<T> && (
    (Width == 2 && sizeof(T) == 8) ||   // __m128d
    (Width == 4 && sizeof(T) == 4) ||   // __m128, __m128i
    (Width == 8 && sizeof(T) == 4) ||   // __m256
    (Width == 8 && sizeof(T) == 8) ||   // __m256d
    (Width == 16 && sizeof(T) == 4) ||  // __m512
    (Width == 16 && sizeof(T) == 8)     // __m512d
);

// ===== Architecture Detection =====

enum class SIMDArchitecture : uint32_t {
    NONE = 0,
    SSE4_2 = 1 << 0,
    AVX = 1 << 1, 
    AVX2 = 1 << 2,
    AVX512F = 1 << 3,
    NEON = 1 << 4,
    VFP = 1 << 5
};

class SIMDDetector {
public:
    static SIMDArchitecture detectArchitecture() {
        SIMDArchitecture arch = SIMDArchitecture::NONE;
        
        // Intel/AMD x86/x64 detection
        uint32_t eax, ebx, ecx, edx;
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            // SSE4.2 detection (CPUID.1:ECX.SSE42)
            if (ecx & bit_SSE4_2) arch = static_cast<SIMDArchitecture>(
                static_cast<uint32_t>(arch) | static_cast<uint32_t>(SIMDArchitecture::SSE4_2));
            
            // AVX detection (CPUID.1:ECX.AVX)  
            if (ecx & bit_AVX) arch = static_cast<SIMDArchitecture>(
                static_cast<uint32_t>(arch) | static_cast<uint32_t>(SIMDArchitecture::AVX));
            
            // FMA detection (CPUID.1:ECX.FMA)
            if (ecx & bit_FMA) arch = static_cast<SIMDArchitecture>(
                static_cast<uint32_t>(arch) | static_cast<uint32_t>(SIMDArchitecture::AVX));
        }
        
        // AVX2 detection (CPUID.7:EBX.AVX2)
        if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            if (ebx & bit_AVX2) arch = static_cast<SIMDArchitecture>(
                static_cast<uint32_t>(arch) | static_cast<uint32_t>(SIMDArchitecture::AVX2));
                
            // AVX-512F detection
            if (ebx & bit_AVX512F) arch = static_cast<SIMDArchitecture>(
                static_cast<uint32_t>(arch) | static_cast<uint32_t>(SIMDArchitecture::AVX512F));
        }
        
        return arch;
    }
    
    static bool hasAVX2() {
        return (detectArchitecture() & SIMDArchitecture::AVX2) != SIMDArchitecture::NONE;
    }
    
    static bool hasAVX512() {
        return (detectArchitecture() & SIMDArchitecture::AVX512F) != SIMDArchitecture::NONE;
    }
    
    static bool hasSSE4() {
        return (detectArchitecture() & SIMDArchitecture::SSE4_2) != SIMDArchitecture::NONE;
    }
};

// ===== C++20 Template-based SIMD Classes =====

/**
 * @brief SIMD vector wrapper with compile-time width
 */
template<SIMDType T, size_t Width>
class SIMDVector {
public:
    using value_type = T;
    using vector_type = std::conditional_t<
        sizeof(T) == 4,
        std::conditional_t<Width == 4, __m128, std::conditional_t<Width == 8, __m256, __m512>>,
        std::conditional_t<Width == 2, __m128d, std::conditional_t<Width == 8, __m256d, __m512d>>
    >;
    
    static constexpr size_t width = Width;
    static constexpr size_t element_count = Width;
    
    SIMDVector() = default;
    
    constexpr explicit SIMDVector(value_type value) {
        if constexpr (sizeof(T) == 4) {
            if constexpr (Width == 4) {
                data_ = _mm_set1_ps(value);
            } else if constexpr (Width == 8) {
                data_ = _mm256_set1_ps(value);
            } else if constexpr (Width == 16) {
                data_ = _mm512_set1_ps(value);
            }
        } else if constexpr (sizeof(T) == 8) {
            if constexpr (Width == 2) {
                data_ = _mm_set1_pd(value);
            } else if constexpr (Width == 8) {
                data_ = _mm256_set1_pd(value);
            } else if constexpr (Width == 16) {
                data_ = _mm512_set1_pd(value);
            }
        }
    }
    
    // Load from aligned pointer
    static SIMDVector load_aligned(const value_type* ptr) {
        SIMDVector result;
        if constexpr (sizeof(T) == 4) {
            if constexpr (Width == 4) {
                result.data_ = _mm_load_ps(ptr);
            } else if constexpr (Width == 8) {
                result.data_ = _mm256_load_ps(ptr);
            } else if constexpr (Width == 16) {
                result.data_ = _mm512_load_ps(ptr);
            }
        } else if constexpr (sizeof(T) == 8) {
            if constexpr (Width == 2) {
                result.data_ = _mm_load_pd(ptr);
            } else if constexpr (Width == 8) {
                result.data_ = _mm256_load_pd(ptr);
            } else if constexpr (Width == 16) {
                result.data_ = _mm512_load_pd(ptr);
            }
        }
        return result;
    }
    
    // Store to aligned pointer
    void store_aligned(value_type* ptr) const {
        if constexpr (sizeof(T) == 4) {
            if constexpr (Width == 4) {
                _mm_store_ps(ptr, data_);
            } else if constexpr (Width == 8) {
                _mm256_store_ps(ptr, data_);
            } else if constexpr (Width == 16) {
                _mm512_store_ps(ptr, data_);
            }
        } else if constexpr (sizeof(T) == 8) {
            if constexpr (Width == 2) {
                _mm_store_pd(ptr, data_);
            } else if constexpr (Width == 8) {
                _mm256_store_pd(ptr, data_);
            } else if constexpr (Width == 16) {
                _mm512_store_pd(ptr, data_);
            }
        }
    }
    
    // Arithmetic operations
    SIMDVector operator+(const SIMDVector& other) const {
        SIMDVector result;
        if constexpr (sizeof(T) == 4) {
            if constexpr (Width == 4) {
                result.data_ = _mm_add_ps(data_, other.data_);
            } else if constexpr (Width == 8) {
                result.data_ = _mm256_add_ps(data_, other.data_);
            } else if constexpr (Width == 16) {
                result.data_ = _mm512_add_ps(data_, other.data_);
            }
        } else if constexpr (sizeof(T) == 8) {
            if constexpr (Width == 2) {
                result.data_ = _mm_add_pd(data_, other.data_);
            } else if constexpr (Width == 8) {
                result.data_ = _mm256_add_pd(data_, other.data_);
            } else if constexpr (Width == 16) {
                result.data_ = _mm512_add_pd(data_, other.data_);
            }
        }
        return result;
    }
    
    SIMDVector operator*(const SIMDVector& other) const {
        SIMDVector result;
        if constexpr (sizeof(T) == 4) {
            if constexpr (Width == 4) {
                result.data_ = _mm_mul_ps(data_, other.data_);
            } else if constexpr (Width == 8) {
                result.data_ = _mm256_mul_ps(data_, other.data_);
            } else if constexpr (Width == 16) {
                result.data_ = _mm512_mul_ps(data_, other.data_);
            }
        } else if constexpr (sizeof(T) == 8) {
            if constexpr (Width == 2) {
                result.data_ = _mm_mul_pd(data_, other.data_);
            } else if constexpr (Width == 8) {
                result.data_ = _mm256_mul_pd(data_, other.data_);
            } else if constexpr (Width == 16) {
                result.data_ = _mm512_mul_pd(data_, other.data_);
            }
        }
        return result;
    }
    
    // Square root
    SIMDVector sqrt() const {
        SIMDVector result;
        if constexpr (sizeof(T) == 4) {
            if constexpr (Width == 4) {
                result.data_ = _mm_sqrt_ps(data_);
            } else if constexpr (Width == 8) {
                result.data_ = _mm256_sqrt_ps(data_);
            } else if constexpr (Width == 16) {
                result.data_ = _mm512_sqrt_ps(data_);
            }
        } else if constexpr (sizeof(T) == 8) {
            if constexpr (Width == 2) {
                result.data_ = _mm_sqrt_pd(data_);
            } else if constexpr (Width == 8) {
                result.data_ = _mm256_sqrt_pd(data_);
            } else if constexpr (Width == 16) {
                result.data_ = _mm512_sqrt_pd(data_);
            }
        }
        return result;
    }
    
    // Horizontal reduction (sum of all elements)
    value_type hsum() const {
        value_type result = 0;
        if constexpr (sizeof(T) == 4) {
            if constexpr (Width == 4) {
                result = _mm_hsum_ps(data_);
            } else if constexpr (Width == 8) {
                result = _mm256_hsum_ps(data_);
            } else if constexpr (Width == 16) {
                result = _mm512_reduce_add_ps(data_);
            }
        } else if constexpr (sizeof(T) == 8) {
            if constexpr (Width == 2) {
                result = _mm_hsum_pd(data_);
            } else if constexpr (Width == 8) {
                result = _mm256_hsum_pd(data_);
            } else if constexpr (Width == 16) {
                result = _mm512_reduce_add_pd(data_);
            }
        }
        return result;
    }
    
    // Broadcast constructor from scalar
    static SIMDVector broadcast(const value_type& value) {
        return SIMDVector(value);
    }
    
    // Get raw SIMD register
    vector_type get() const { return data_; }
    
private:
    vector_type data_;
};

// ===== Optimized Entity Operations =====

/**
 * @brief SIMD-optimized entity distance calculation
 * @brief Vectorized 3D distance calculation
 */
template<SIMDType T, SIMDArchitecture Arch = SIMDArchitecture::AVX2>
class EntityDistanceCalculator {
public:
    constexpr static bool isSupported = (Arch == SIMDArchitecture::NONE) || 
        ((Arch & SIMDArchitecture::AVX2) && SIMDDetector::hasAVX2()) ||
        ((Arch & SIMDArchitecture::AVX512F) && SIMDDetector::hasAVX512());
        
    /**
     * @brief Calculate distances between multiple entities and center point
     * @param entities Array of entity positions [x0, y0, z0, x1, y1, z1, ...]
     * @param center Center position
     * @param count Number of entities
     * @param result Distance array
     */
    static void calculateDistances(const T* entities, const SIMDVector<T, 3>& center, 
                                   size_t count, T* result) {
        if constexpr (Arch == SIMDArchitecture::AVX512F && SIMDDetector::hasAVX512()) {
            // AVX-512 optimized version for large batches
            constexpr size_t batch_size = 16;
            constexpr size_t lanes = 3;
            
            // Pre-construct center vectors
            auto center_x = SIMDVector<T, 16>::broadcast(center.get()[0]);
            auto center_y = SIMDVector<T, 16>::broadcast(center.get()[1]); 
            auto center_z = SIMDVector<T, 16>::broadcast(center.get()[2]);
            
            for (size_t i = 0; i + batch_size * lanes <= count * 3; i += batch_size * lanes) {
                auto entity_x = SIMDVector<T, 16>::load_aligned(&entities[i]);
                auto entity_y = SIMDVector<T, 16>::load_aligned(&entities[i + batch_size]);
                auto entity_z = SIMDVector<T, 16>::load_aligned(&entities[i + 2 * batch_size]);
                
                // Calculate (x - cx)^2 + (y - cy)^2 + (z - cz)^2
                auto dx = entity_x - center_x;
                auto dy = entity_y - center_y;
                auto dz = entity_z - center_z;
                
                auto distance_squared = dx * dx + dy * dy + dz * dz;
                auto distance = distance_squared.sqrt();
                
                distance.store_aligned(&result[i / lanes]);
            }
        } else if constexpr (Arch == SIMDArchitecture::AVX2 && SIMDDetector::hasAVX2()) {
            // AVX2 optimized version
            constexpr size_t batch_size = 8;
            constexpr size_t lanes = 3;
            
            for (size_t i = 0; i + batch_size * lanes <= count * 3; i += batch_size * lanes) {
                auto entity_x = SIMDVector<T, 8>::load_aligned(&entities[i]);
                auto entity_y = SIMDVector<T, 8>::load_aligned(&entities[i + batch_size]);
                auto entity_z = SIMDVector<T, 8>::load_aligned(&entities[i + 2 * batch_size]);
                
                auto dx = entity_x - center.getArray()[0];
                auto dy = entity_y - center.getArray()[1];
                auto dz = entity_z - center.getArray()[2];
                
                auto distance_squared = dx * dx + dy * dy + dz * dz;
                auto distance = distance_squared.sqrt();
                
                distance.store_aligned(&result[i / lanes]);
            }
        } else {
            // Fallback to scalar computation
            calculateDistancesScalar(entities, center, count, result);
        }
    }
    
private:
    static void calculateDistancesScalar(const T* entities, const SIMDVector<T, 3>& center, 
                                       size_t count, T* result) {
        const T* center_pos = center.get();
        for (size_t i = 0; i < count; ++i) {
            const T* entity = &entities[i * 3];
            T dx = entity[0] - center_pos[0];
            T dy = entity[1] - center_pos[1];
            T dz = entity[2] - center_pos[2];
            result[i] = std::sqrt(dx*dx + dy*dy + dz*dz);
        }
    }
};

/**
 * @brief SIMD-optimized entity culling and selection
 */
template<SIMDType T>
class EntitySelector {
public:
    /**
     * @brief Select entities within range using SIMD comparison
     * @param distances Distance array
     * @param ids Entity ID array
     * @param count Number of entities
     * @param range Maximum distance
     * @param selected_ids Output array for selected entity IDs
     * @return Number of selected entities
     */
    static size_t selectInRange(const T* distances, const uint64_t* ids, 
                               size_t count, T range, uint64_t* selected_ids) {
        size_t selected_count = 0;
        
        if constexpr (sizeof(T) == 4) {
            // AVX2 comparison for float
            __m256 range_vec = _mm256_set1_ps(range);
            
            for (size_t i = 0; i + 8 <= count; i += 8) {
                __m256 dist_vec = _mm256_load_ps(&distances[i]);
                __m256 mask_vec = _mm256_cmp_ps(dist_vec, range_vec, _CMP_LE_OQ);
                
                int mask = _mm256_movemask_ps(mask_vec);
                
                // Extract selected IDs
                for (int bit = 0; bit < 8; ++bit) {
                    if (mask & (1 << bit)) {
                        selected_ids[selected_count++] = ids[i + bit];
                    }
                }
            }
        } else {
            // Scalar fallback
            for (size_t i = 0; i < count; ++i) {
                if (distances[i] <= range) {
                    selected_ids[selected_count++] = ids[i];
                }
            }
        }
        
        // Handle remaining elements
        for (size_t i = count & ~7; i < count; ++i) {
            if (distances[i] <= range) {
                selected_ids[selected_count++] = ids[i];
            }
        }
        
        return selected_count;
    }
};

// ===== Advanced C++20 Optimizations =====

/**
 * @brief Constexpr-based compile-time vector size detection
 */
template<typename T>
constexpr size_t getOptimalSIMDWidth() {
    if constexpr (std::same_as<T, float>) {
        if constexpr (SIMDDetector::hasAVX512()) return 16;
        if constexpr (SIMDDetector::hasAVX2()) return 8;
        return 4; // SSE
    } else if constexpr (std::same_as<T, double>) {
        if constexpr (SIMDDetector::hasAVX512()) return 8;
        if constexpr (SIMDDetector::hasAVX2()) return 4;
        return 2; // SSE
    }
    return 1; // fallback
}

/**
 * @brief Template specialization for different architectures
 */
template<typename T, SIMDArchitecture Arch = SIMDArchitecture::AVX2>
class OptimizedEntityProcessor {
public:
    using SIMDVec = SIMDVector<T, getOptimalSIMDWidth<T>()>;
    
    /**
     * @brief Process entity behaviors with SIMD optimization
     */
    static void processEntityBatch(const EntityState* entities, size_t count, 
                                  BehaviorState* results) {
        if constexpr (Arch == SIMDArchitecture::AVX512F && SIMDDetector::hasAVX512()) {
            processWithAVX512(entities, count, results);
        } else if constexpr (Arch == SIMDArchitecture::AVX2 && SIMDDetector::hasAVX2()) {
            processWithAVX2(entities, count, results);
        } else {
            processScalar(entities, count, results);
        }
    }
    
private:
    static void processWithAVX512(const EntityState* entities, size_t count, 
                                 BehaviorState* results) {
        // AVX-512 implementation
        constexpr size_t simd_width = 16;
        constexpr size_t batch_size = simd_width;
        
        for (size_t i = 0; i + batch_size <= count; i += batch_size) {
            // Load health data
            __m512 health_vec = _mm512_load_ps(reinterpret_cast<const float*>(&entities[i].health));
            
            // SIMD decision logic
            // Health > 50% -> AGGRESSIVE
            // Health 20-50% -> DEFENSIVE  
            // Health < 20% -> FLEEING
            __m512 threshold_high = _mm512_set1_ps(0.5f);
            __m512 threshold_low = _mm512_set1_ps(0.2f);
            
            auto mask_aggressive = _mm512_cmp_ps_mask(health_vec, threshold_high, _CMP_GT_OQ);
            auto mask_defensive = _mm512_and_ps_mask(
                _mm512_cmp_ps_mask(health_vec, threshold_high, _CMP_LE_OQ),
                _mm512_cmp_ps_mask(health_vec, threshold_low, _CMP_GE_OQ)
            );
            
            // Store results with SIMD masking
            BehaviorState* target_results = &results[i];
            _mm512_mask_store_epi32(reinterpret_cast<int*>(target_results), 
                                   mask_aggressive, 
                                   _mm512_set1_epi32(static_cast<int>(BehaviorState::AGGRESSIVE)));
            _mm512_mask_store_epi32(reinterpret_cast<int*>(target_results),
                                   mask_defensive,
                                   _mm512_set1_epi32(static_cast<int>(BehaviorState::DEFENSIVE)));
        }
    }
    
    static void processWithAVX2(const EntityState* entities, size_t count, 
                               BehaviorState* results) {
        // AVX2 implementation (similar to AVX512 but with 8-wide vectors)
        for (size_t i = 0; i + 8 <= count; i += 8) {
            // Process 8 entities at once
            __m256 health_vec = _mm256_load_ps(reinterpret_cast<const float*>(&entities[i].health));
            
            // Decision logic for 8 entities
            // ... similar logic with AVX2 intrinsics
        }
    }
    
    static void processScalar(const EntityState* entities, size_t count, 
                             BehaviorState* results) {
        // Fallback scalar implementation
        for (size_t i = 0; i < count; ++i) {
            if (entities[i].health > 0.5f * entities[i].maxHealth) {
                results[i] = BehaviorState::AGGRESSIVE;
            } else if (entities[i].health > 0.2f * entities[i].maxHealth) {
                results[i] = BehaviorState::DEFENSIVE;
            } else {
                results[i] = BehaviorState::FLEEING;
            }
        }
    }
};

// ===== Performance Monitoring =====

class SIMDBenchmark {
public:
    struct BenchmarkResult {
        double avg_time_ns;
        double throughput_ops_per_sec;
        size_t processed_elements;
        std::string architecture;
    };
    
    /**
     * @brief Benchmark SIMD performance
     */
    template<typename Func>
    static BenchmarkResult benchmarkSIMDOperation(Func&& operation, size_t iterations, 
                                                 size_t element_count, 
                                                 const std::string& description) {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < iterations; ++i) {
            operation();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        double total_time = static_cast<double>(duration.count());
        double avg_time = total_time / iterations;
        double throughput = element_count / (avg_time / 1e9);
        
        return {
            avg_time_ns: avg_time,
            throughput_ops_per_sec: throughput,
            processed_elements: element_count,
            architecture: detectArchitectureName()
        };
    }
    
private:
    static std::string detectArchitectureName() {
        auto arch = SIMDDetector::detectArchitecture();
        if (arch & SIMDArchitecture::AVX512F) return "AVX-512";
        if (arch & SIMDArchitecture::AVX2) return "AVX2";
        if (arch & SIMDArchitecture::AVX) return "AVX";
        if (arch & SIMDArchitecture::SSE4_2) return "SSE4.2";
        return "Scalar";
    }
};

} // namespace lattice::simd

// ===== Convenience Type Aliases =====

namespace lattice::simd {
    using f32x4 = SIMDVector<float, 4>;
    using f32x8 = SIMDVector<float, 8>;
    using f32x16 = SIMDVector<float, 16>;
    using f64x4 = SIMDVector<double, 4>;
    using f64x8 = SIMDVector<double, 8>;
    using f64x16 = SIMDVector<double, 16>;
}