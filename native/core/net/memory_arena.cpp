#include "memory_arena.hpp"
#include <algorithm>
#include <iostream>

namespace lattice {
namespace net {

// 初始化静态成员变量
std::atomic<size_t> MemoryArena::global_total_allocated_{0};
std::atomic<size_t> MemoryArena::global_total_used_{0};
std::atomic<size_t> MemoryArena::global_thread_count_{0};

// 获取线程局部分配器实例
MemoryArena& MemoryArena::forThread() {
    thread_local static MemoryArena instance;
    return instance;
}

// 获取当前线程的块列表
MemoryArena::ThreadBlocks& MemoryArena::getThreadBlocks() {
    if (!current_thread_blocks_) {
        current_thread_blocks_ = new ThreadBlocks();
        global_thread_count_.fetch_add(1, std::memory_order_relaxed);
    }
    return *current_thread_blocks_;
}

// 创建新的内存块
std::unique_ptr<MemoryArena::MemoryBlock> MemoryArena::createNewBlock(size_t blockSize) {
    try {
        auto block = std::make_unique<MemoryBlock>(blockSize);
        global_total_allocated_.fetch_add(blockSize, std::memory_order_relaxed);
        return block;
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

// 寻找有足够空间的块
MemoryArena::MemoryBlock* MemoryArena::findSuitableBlock(size_t requiredSize) {
    auto& blocks = getThreadBlocks();
    
    // 首先尝试使用当前块（最后一个块）
    if (!blocks.empty()) {
        MemoryBlock* currentBlock = blocks.back().get();
        if (currentBlock->remaining >= ((requiredSize + DEFAULT_ALIGNMENT - 1) & ~(DEFAULT_ALIGNMENT - 1))) {
            return currentBlock;
        }
    }
    
    // 在现有块中寻找合适的块
    for (auto& block : blocks) {
        if (block->remaining >= ((requiredSize + DEFAULT_ALIGNMENT - 1) & ~(DEFAULT_ALIGNMENT - 1))) {
            return block.get();
        }
    }
    
    return nullptr;
}

// 分配内存
void* MemoryArena::allocate(size_t size) {
    if (size == 0) {
        return nullptr;
    }
    
    // 对齐请求大小
    size_t alignedSize = (size + DEFAULT_ALIGNMENT - 1) & ~(DEFAULT_ALIGNMENT - 1);
    
    // 寻找合适的块
    MemoryBlock* targetBlock = findSuitableBlock(alignedSize);
    
    // 如果没有找到合适的块，创建新块
    if (!targetBlock) {
        auto& blocks = getThreadBlocks();
        
        // 检查是否超过最大块数限制
        if (blocks.size() >= MAX_BLOCKS_PER_THREAD) {
            // 尝试在现有块中找到任何有剩余空间的块
            for (auto& block : blocks) {
                if (block->remaining >= alignedSize) {
                    targetBlock = block.get();
                    break;
                }
            }
            
            // 如果仍然找不到，返回nullptr（无法分配）
            if (!targetBlock) {
                return nullptr;
            }
        } else {
            // 创建新块
            size_t newBlockSize = DEFAULT_BLOCK_SIZE;
            
            // 如果请求的内存较大，分配更大的块
            if (alignedSize > DEFAULT_BLOCK_SIZE / 2) {
                newBlockSize = std::max(alignedSize * 2, DEFAULT_BLOCK_SIZE);
            }
            
            auto newBlock = createNewBlock(newBlockSize);
            if (!newBlock) {
                return nullptr;
            }
            
            targetBlock = newBlock.get();
            blocks.push_back(std::move(newBlock));
        }
    }
    
    // 在目标块中分配内存
    void* result = targetBlock->allocate(alignedSize);
    if (result) {
        global_total_used_.fetch_add(alignedSize, std::memory_order_relaxed);
    }
    
    return result;
}

// 释放整个arena
void MemoryArena::clear() {
    auto& blocks = getThreadBlocks();
    
    // 统计要释放的内存
    size_t freedSize = 0;
    for (auto& block : blocks) {
        freedSize += block->getTotalSize();
    }
    
    // 更新全局统计
    global_total_allocated_.fetch_sub(freedSize, std::memory_order_relaxed);
    
    // 清理所有块
    blocks.clear();
    
    // 重置内存使用统计
    global_total_used_.fetch_sub(freedSize, std::memory_order_relaxed);
}

// 获取内存使用统计
MemoryArena::MemoryStats MemoryArena::getMemoryStats() const {
    MemoryStats stats;
    stats.total_allocated = global_total_allocated_.load(std::memory_order_relaxed);
    stats.total_used = global_total_used_.load(std::memory_order_relaxed);
    stats.block_count = 0;
    stats.thread_count = global_thread_count_.load(std::memory_order_relaxed);
    stats.average_utilization = 0.0f;
    
    // 如果当前线程有块，计算其统计
    if (current_thread_blocks_) {
        auto threadStats = calculateThreadStats(*current_thread_blocks_);
        stats.block_count = threadStats.block_count;
        stats.average_utilization = threadStats.average_utilization;
    }
    
    return stats;
}

// 计算当前线程的内存统计
MemoryArena::MemoryStats MemoryArena::calculateThreadStats(const ThreadBlocks& blocks) const {
    MemoryStats stats;
    stats.total_allocated = 0;
    stats.total_used = 0;
    stats.block_count = blocks.size();
    stats.thread_count = 1;
    stats.average_utilization = 0.0f;
    
    if (blocks.empty()) {
        return stats;
    }
    
    size_t totalUtilization = 0;
    for (const auto& block : blocks) {
        stats.total_allocated += block->getTotalSize();
        stats.total_used += block->getMemoryUsage();
        totalUtilization += static_cast<size_t>(block->getUtilization() * 1000); // 使用千分比避免浮点精度问题
    }
    
    stats.average_utilization = static_cast<float>(totalUtilization) / (blocks.size() * 1000);
    
    return stats;
}

// 强制清理当前线程的arena
void MemoryArena::clearCurrentThread() {
    if (current_thread_blocks_) {
        // 统计要释放的内存
        size_t freedSize = 0;
        for (auto& block : *current_thread_blocks_) {
            freedSize += block->getTotalSize();
        }
        
        // 更新全局统计
        global_total_allocated_.fetch_sub(freedSize, std::memory_order_relaxed);
        global_thread_count_.fetch_sub(1, std::memory_order_relaxed);
        
        // 清理线程局部存储
        delete current_thread_blocks_;
        current_thread_blocks_ = nullptr;
        
        // 重置内存使用统计
        global_total_used_.fetch_sub(freedSize, std::memory_order_relaxed);
    }
}

} // namespace net
} // namespace lattice