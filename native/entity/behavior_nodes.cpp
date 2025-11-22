#include "behavior_nodes.hpp"
#include <random>
#include <algorithm>
#include <execution>
#include <x86intrin.h>
#include <iostream>
#include <cmath>

namespace lattice::entity {

// ====== SIMD优化工具函数 ======

/**
 * @brief 计算向量化距离的AVX2优化版本
 */
__attribute__((target("avx2")))
static inline __m256 calculateVectorDistance(const __m256& startX, const __m256& startY, const __m256& startZ,
                                            const __m256& targetX, const __m256& targetY, const __m256& targetZ) {
    __m256 dx = _mm256_sub_ps(targetX, startX);
    __m256 dy = _mm256_sub_ps(targetY, startY);
    __m256 dz = _mm256_sub_ps(targetZ, startZ);
    
    __m256 distanceSq = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dx, dx),
                                                   _mm256_mul_ps(dy, dy)),
                                      _mm256_mul_ps(dz, dz));
    
    return _mm256_sqrt_ps(distanceSq);
}

// Note: findNearbyEntitiesSIMD function is defined in biological_ai.cpp
// Note: g_threadContext is defined in biological_ai.hpp

} // namespace lattice::entity
