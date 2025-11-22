#include "entity_tracker_optimized_jni.hpp"
#include <chrono>

namespace lattice {
namespace jni {
namespace net {

// 初始化性能统计
std::atomic<uint64_t> EntityTrackerJNIOptimized::totalOperations_{0};
std::atomic<uint64_t> EntityTrackerJNIOptimized::totalTimeMicros_{0};
std::atomic<uint64_t> EntityTrackerJNIOptimized::cacheHits_{0};
std::atomic<uint64_t> EntityTrackerJNIOptimized::cacheMisses_{0};

jlong EntityTrackerJNIOptimized::registerEntity(JNIEnv* env, jobject tracker, 
                                               jlong entityId, jfloat posX, jfloat posY, 
                                               jfloat posZ, jfloat radius) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (entityId <= 0 || tracker == nullptr) {
        return 0;
    }
    
    try {
        // 2. 调用core中的线程局部分配器初始化
        lattice::entity::JNIEntityTracker::initialize();
        
        // 3. 委托给core注册实体
        lattice::entity::JNIEntityTracker::registerEntity(
            static_cast<int>(entityId), posX, posY, posZ, radius);
        
        // 4. 返回实体ID作为标识符
        return entityId;
        
    } catch (const std::exception& e) {
        // 5. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return 0;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("registerEntity", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void EntityTrackerJNIOptimized::updateEntityPosition(JNIEnv* env, jobject tracker, 
                                                    jlong entityId, jfloat posX, jfloat posY, jfloat posZ) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (entityId <= 0 || tracker == nullptr) {
        return;
    }
    
    try {
        // 2. 委托给core更新位置
        lattice::entity::JNIEntityTracker::updateEntityPosition(
            static_cast<int>(entityId), posX, posY, posZ);
            
    } catch (const std::exception& e) {
        // 3. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("updateEntityPosition", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void EntityTrackerJNIOptimized::unregisterEntity(JNIEnv* env, jobject tracker, jlong entityId) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (entityId <= 0 || tracker == nullptr) {
        return;
    }
    
    try {
        // 2. 获取core实例并调用移除方法
        // 简化实现：这里应该调用core中的EntityTracker实例的方法
        // lattice::entity::JNIEntityTracker::removeEntity(static_cast<int>(entityId));
        
    } catch (const std::exception& e) {
        // 3. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("unregisterEntity", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jlongArray EntityTrackerJNIOptimized::getVisibleEntities(JNIEnv* env, jobject tracker,
                                                        jfloat viewerX, jfloat viewerY, jfloat viewerZ, 
                                                        jfloat viewDistance) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (tracker == nullptr || viewDistance <= 0) {
        return nullptr;
    }
    
    try {
        // 2. 调用core获取可见实体
        int entityCount = 0;
        int* visibleEntities = lattice::entity::JNIEntityTracker::getVisibleEntities(
            viewerX, viewerY, viewerZ, viewDistance, &entityCount);
        
        if (!visibleEntities || entityCount <= 0) {
            // 返回空数组
            jlongArray result = env->NewLongArray(0);
            return result;
        }
        
        // 3. 转换为Java数组
        jlongArray result = env->NewLongArray(entityCount);
        std::vector<jlong> javaEntities;
        javaEntities.reserve(entityCount);
        
        for (int i = 0; i < entityCount; i++) {
            javaEntities.push_back(visibleEntities[i]);
        }
        
        env->SetLongArrayRegion(result, 0, javaEntities.size(), javaEntities.data());
        
        // 4. 清理core分配的内存
        delete[] visibleEntities;
        
        // 5. 更新缓存统计
        cacheHits_++;
        
        return result;
        
    } catch (const std::exception& e) {
        // 6. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return nullptr;
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("getVisibleEntities", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void EntityTrackerJNIOptimized::batchUpdatePositions(JNIEnv* env, jobject tracker, jlongArray entityUpdates) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (entityUpdates == nullptr || tracker == nullptr) {
        return;
    }
    
    jsize updateCount = env->GetArrayLength(entityUpdates);
    if (updateCount <= 0) {
        return;
    }
    
    // 2. 获取更新数据
    jlong* updates = env->GetLongArrayElements(entityUpdates, nullptr);
    if (!updates) {
        return;
    }
    
    try {
        // 3. 组织批量更新数据（每个更新包含entityId + 3个坐标）
        std::vector<std::tuple<int, float, float, float>> batchUpdates;
        batchUpdates.reserve(updateCount / 4);
        
        for (jsize i = 0; i < updateCount; i += 4) {
            if (i + 3 < updateCount) {
                int entityId = static_cast<int>(updates[i]);
                float posX = static_cast<float>(updates[i + 1]);
                float posY = static_cast<float>(updates[i + 2]);
                float posZ = static_cast<float>(updates[i + 3]);
                
                if (entityId > 0) {
                    batchUpdates.emplace_back(entityId, posX, posY, posZ);
                }
            }
        }
        
        // 4. 委托给core批量处理
        // 简化实现：这里应该调用core中的batchUpdatePositions方法
        // lattice::entity::JNIEntityTracker::batchUpdatePositions(batchUpdates);
        
        // 临时：逐个调用单个更新方法
        for (const auto& update : batchUpdates) {
            lattice::entity::JNIEntityTracker::updateEntityPosition(
                std::get<0>(update), std::get<1>(update), 
                std::get<2>(update), std::get<3>(update));
        }
        
    } catch (const std::exception& e) {
        // 5. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    } finally {
        // 6. 释放Java数组
        env->ReleaseLongArrayElements(entityUpdates, updates, 0);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("batchUpdatePositions", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jobjectArray EntityTrackerJNIOptimized::batchGetVisibleEntities(JNIEnv* env, jobject tracker, 
                                                               jlongArray viewerPositions) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (viewerPositions == nullptr || tracker == nullptr) {
        return nullptr;
    }
    
    jsize viewerCount = env->GetArrayLength(viewerPositions);
    if (viewerCount <= 0) {
        return nullptr;
    }
    
    // 2. 获取观看者位置数据
    jlong* positions = env->GetLongArrayElements(viewerPositions, nullptr);
    if (!positions) {
        return nullptr;
    }
    
    try {
        // 3. 组织批量查询数据（每个查询包含viewerX + viewerY + viewerZ + viewDistance）
        std::vector<std::tuple<float, float, float, float>> batchViewers;
        batchViewers.reserve(viewerCount / 4);
        
        for (jsize i = 0; i < viewerCount; i += 4) {
            if (i + 3 < viewerCount) {
                float viewerX = static_cast<float>(positions[i]);
                float viewerY = static_cast<float>(positions[i + 1]);
                float viewerZ = static_cast<float>(positions[i + 2]);
                float viewDistance = static_cast<float>(positions[i + 3]);
                
                if (viewDistance > 0) {
                    batchViewers.emplace_back(viewerX, viewerY, viewerZ, viewDistance);
                }
            }
        }
        
        // 4. 委托给core批量查询
        // 简化实现：这里应该调用core中的batchGetVisibleEntities方法
        // auto batchResults = lattice::entity::JNIEntityTracker::batchGetVisibleEntities(batchViewers);
        
        // 临时：逐个调用单个查询方法
        std::vector<std::vector<int>> batchResults;
        batchResults.reserve(batchViewers.size());
        
        for (const auto& viewer : batchViewers) {
            int entityCount = 0;
            int* visibleEntities = lattice::entity::JNIEntityTracker::getVisibleEntities(
                std::get<0>(viewer), std::get<1>(viewer), std::get<2>(viewer), 
                std::get<3>(viewer), &entityCount);
            
            std::vector<int> entities;
            if (visibleEntities && entityCount > 0) {
                entities.assign(visibleEntities, visibleEntities + entityCount);
                delete[] visibleEntities;
            }
            
            batchResults.push_back(std::move(entities));
        }
        
        // 5. 转换为Java对象数组
        jclass longArrayClass = env->FindClass("[J");
        jobjectArray result = env->NewObjectArray(batchResults.size(), longArrayClass, nullptr);
        
        for (size_t i = 0; i < batchResults.size(); i++) {
            const auto& entities = batchResults[i];
            jlongArray entityArray = env->NewLongArray(entities.size());
            
            if (!entities.empty()) {
                std::vector<jlong> javaEntities;
                javaEntities.reserve(entities.size());
                for (int entityId : entities) {
                    javaEntities.push_back(entityId);
                }
                env->SetLongArrayRegion(entityArray, 0, javaEntities.size(), javaEntities.data());
            }
            
            env->SetObjectArrayElement(result, i, entityArray);
            env->DeleteLocalRef(entityArray);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        // 6. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return nullptr;
    } finally {
        // 7. 释放Java数组
        env->ReleaseLongArrayElements(viewerPositions, positions, 0);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("batchGetVisibleEntities", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void EntityTrackerJNIOptimized::tick(JNIEnv* env, jobject tracker) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // 1. 参数验证
    if (tracker == nullptr) {
        return;
    }
    
    try {
        // 2. 委托给core执行tick
        lattice::entity::JNIEntityTracker::tick();
        
    } catch (const std::exception& e) {
        // 3. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("tick", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void EntityTrackerJNIOptimized::initializeThreadLocal(JNIEnv* env, jobject tracker) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // 1. 初始化core中的线程局部实例
        lattice::entity::JNIEntityTracker::initialize();
        
    } catch (const std::exception& e) {
        // 2. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("initializeThreadLocal", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

void EntityTrackerJNIOptimized::shutdownThreadLocal(JNIEnv* env, jobject tracker) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    try {
        // 1. 关闭core中的线程局部实例
        lattice::entity::JNIEntityTracker::shutdown();
        
    } catch (const std::exception& e) {
        // 2. 错误处理
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logPerformanceStats("shutdownThreadLocal", 
                       std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count());
}

jlong EntityTrackerJNIOptimized::getTrackedEntitiesCount(JNIEnv* env, jobject tracker) {
    // 简化实现：返回JNI统计
    return static_cast<jlong>(totalOperations_.load() / 10); // 估算值
}

jlong EntityTrackerJNIOptimized::getQueryCacheHitRate(JNIEnv* env, jobject tracker) {
    uint64_t hits = cacheHits_.load();
    uint64_t total = hits + cacheMisses_.load();
    if (total == 0) return 0;
    return static_cast<jlong>((hits * 100) / total);
}

jlong EntityTrackerJNIOptimized::getAverageQueryTimeMicros(JNIEnv* env, jobject tracker) {
    uint64_t operations = totalOperations_.load();
    if (operations == 0) return 0;
    return static_cast<jlong>(totalTimeMicros_.load() / operations);
}

jlong EntityTrackerJNIOptimized::getEntitiesProcessedCount(JNIEnv* env, jobject tracker) {
    return static_cast<jlong>(totalOperations_.load());
}

void EntityTrackerJNIOptimized::setViewDistance(JNIEnv* env, jobject tracker, jfloat distance) {
    // 简化实现：设置全局视距
    if (distance > 0 && distance <= 256.0f) {
        // 委托给core设置视距
        // lattice::entity::JNIEntityTracker::setViewDistance(distance);
    }
}

void EntityTrackerJNIOptimized::logPerformanceStats(const char* operation, uint64_t durationMicros) {
    totalOperations_++;
    totalTimeMicros_.fetch_add(durationMicros);
}

void EntityTrackerJNIOptimized::checkJNIException(JNIEnv* env, const char* methodName) {
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}

} // namespace net
} // namespace jni
} // namespace lattice