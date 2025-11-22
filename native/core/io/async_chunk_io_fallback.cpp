#include "async_chunk_io.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <chrono>
#include <thread>
#include <algorithm>

namespace lattice {
namespace io {

// ===== Fallback 后端实现（标准 POSIX I/O） =====

class AsyncChunkIO::PlatformBackend {
public:
    PlatformBackend(lattice::net::MemoryArena& arena) 
        : memoryArena_(arena), running_(true) {
        
        // 检测基本平台特性
        detectPlatformFeatures();
        
        // 启动工作线程
        startWorkerThreads();
    }
    
    ~PlatformBackend() {
        running_ = false;
        
        // 等待工作线程完成
        for (auto& thread : workerThreads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    // 标准异步读取（使用线程池）
    void loadChunkAsync(const std::string& path, off_t offset, size_t size,
                       std::function<void(char*, size_t)> callback) {
        
        // 将任务提交到线程池
        std::thread([this, path, offset, size, callback]() {
            this->performLoad(path, offset, size, callback);
        }).detach();
    }
    
    // 标准批量保存
    void saveChunksBatch(const std::vector<ChunkSaveRequest>& requests) {
        for (const auto& req : requests) {
            std::thread([this, req]() {
                this->performSave(req);
            }).detach();
        }
    }
    
    const PlatformFeatures& getFeatures() const { return features_; }

private:
    lattice::net::MemoryArena& memoryArena_;
    std::atomic<bool> running_;
    PlatformFeatures features_;
    std::vector<std::thread> workerThreads_;
    
    struct ChunkSaveRequest {
        const std::string path;
        const char* data;
        size_t size;
    };
    
    // 检测平台特性
    void detectPlatformFeatures() {
        // Fallback 实现不检测高级特性
        features_.io_uring = false;
        features_.direct_io = false;
        features_.apfs = false;
        features_.apple_silicon = false;
        features_.iocp = false;
    }
    
    // 启动工作线程
    void startWorkerThreads() {
        int numThreads = std::max(2, std::thread::hardware_concurrency() / 4);
        
        for (int i = 0; i < numThreads; ++i) {
            workerThreads_.emplace_back([this]() {
                this->workerThread();
            });
        }
    }
    
    // 工作线程
    void workerThread() {
        while (running_) {
            // 简单的轮询机制
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    // 执行加载操作
    void performLoad(const std::string& path, off_t offset, size_t size,
                    std::function<void(char*, size_t)> callback) {
        
        // 从内存池分配缓冲区
        char* buffer = static_cast<char*>(memoryArena_.allocate(size));
        if (!buffer) {
            callback(nullptr, 0);
            return;
        }
        
        // 打开文件
        int fd = open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            // MemoryArena分配的内存不能单独释放，交给Arena自动管理
            callback(nullptr, 0);
            return;
        }
        
        // 定位文件指针
        if (lseek(fd, offset, SEEK_SET) == -1) {
            close(fd);
            // MemoryArena分配的内存不能单独释放，交给Arena自动管理
            callback(nullptr, 0);
            return;
        }
        
        // 读取数据
        ssize_t bytesRead = read(fd, buffer, size);
        close(fd);
        
        if (bytesRead > 0) {
            callback(buffer, bytesRead);
        } else {
            // MemoryArena分配的内存不能单独释放，交给Arena自动管理
            callback(nullptr, 0);
        }
    }
    
    // 执行保存操作
    void performSave(const ChunkSaveRequest& req) {
        int fd = open(req.path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd != -1) {
            write(fd, req.data, req.size);
            close(fd);
        }
    }
};

// ===== Memory Mapped Region (Fallback) =====

class MemoryMappedRegion::PlatformImpl {
public:
    PlatformImpl(const std::string& filePath, size_t size, bool readOnly)
        : fd_(-1), address_(nullptr), size_(size) {
        
        // 打开文件
        int flags = readOnly ? O_RDONLY : O_RDWR;
        fd_ = open(filePath.c_str(), flags | O_CREAT, 0644);
        if (fd_ == -1) {
            throw std::runtime_error("Failed to open file: " + std::string(strerror(errno)));
        }
        
        // 确保文件大小
        if (!readOnly && ftruncate(fd_, size) == -1) {
            close(fd_);
            throw std::runtime_error("Failed to truncate file: " + std::string(strerror(errno)));
        }
        
        // 内存映射
        int prot = readOnly ? PROT_READ : (PROT_READ | PROT_WRITE);
        address_ = mmap(nullptr, size, prot, MAP_SHARED, fd_, 0);
        if (address_ == MAP_FAILED) {
            close(fd_);
            throw std::runtime_error("mmap failed: " + std::string(strerror(errno)));
        }
    }
    
    ~PlatformImpl() {
        if (address_) munmap(address_, size_);
        if (fd_ != -1) close(fd_);
    }
    
    void* data() const { return address_; }
    size_t size() const { return size_; }

private:
    int fd_;
    void* address_;
    size_t size_;
};

// ===== 平台特性检测 (Fallback) =====

PlatformFeatures AsyncChunkIO::detectPlatformFeatures() {
    PlatformFeatures features;
    // 所有高级特性都不可用
    features.io_uring = false;
    features.direct_io = false;
    features.apfs = false;
    features.apple_silicon = false;
    features.iocp = false;
    return features;
}

} // namespace io
} // namespace lattice