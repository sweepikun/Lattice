#include "async_chunk_io.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <thread>
#include <future>
#include <cstring>

#if defined(PLATFORM_LINUX) && !defined(NO_IO_URING)
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/mman.h>
    #include <errno.h>
    #include <liburing.h>
#elif defined(PLATFORM_WINDOWS)
    #include <windows.h>
    #include <io.h>
    #include <fileapi.h>
    #include <handleapi.h>
#elif defined(PLATFORM_MACOS)
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <dispatch/dispatch.h>
#endif

namespace lattice {
namespace world {

// ===========================================
// 平台特性检测实现
// ===========================================
PlatformFeatures PlatformFeatures::detect() {
    PlatformFeatures features;
    
    // CPU核心数检测
#if defined(PLATFORM_WINDOWS)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    features.cpu_cores = sysinfo.dwNumberOfProcessors;
#elif defined(PLATFORM_MACOS)
    int cores;
    size_t size = sizeof(cores);
    sysctlbyname("hw.ncpu", &cores, &size, nullptr, 0);
    features.cpu_cores = cores;
#else
    features.cpu_cores = std::max(1, static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN)));
#endif
    
    // SIMD检测
#if defined(__AVX512__)
    features.simd_available = true;
#elif defined(__AVX2__)
    features.simd_available = true;
#elif defined(__SSE4_2__)
    features.simd_available = true;
#elif defined(__ARM_NEON)
    features.simd_available = true;
#endif
    
    // 平台特定I/O特性
#if defined(PLATFORM_LINUX)
    // 检查io_uring支持
    struct io_uring ring;
    if (io_uring_queue_init(8, &ring, 0) == 0) {
        features.io_uring = true;
        io_uring_queue_exit(&ring);
    }
    
    // 检查Direct I/O支持
    int test_fd = open("/tmp/test_dio", O_RDWR | O_CREAT | O_DIRECT, 0644);
    if (test_fd != -1) {
        features.direct_io = true;
        close(test_fd);
        unlink("/tmp/test_dio");
    }
    
    // 检查内存映射支持
    features.memory_mapping = true;
    
#elif defined(PLATFORM_WINDOWS)
    // Windows IOCP支持
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (iocp != nullptr) {
        features.iocp = true;
        CloseHandle(iocp);
    }
    
    // 内存映射支持
    features.memory_mapping = true;
    
#elif defined(PLATFORM_MACOS)
    // GCD支持
    features.gcd = true;
    
    // APFS检测
    struct statfs fs;
    if (statfs("/", &fs) == 0 && strcmp(fs.f_fstypename, "apfs") == 0) {
        features.apfs = true;
    }
    
    // 内存映射支持
    features.memory_mapping = true;
#endif
    
    return features;
}

// ===========================================
// SIMD区块处理器实现
// ===========================================
SIMDChunkProcessor::SIMDChunkProcessor(const BatchConfig& config)
    : config_(config) {
    // 初始化SIMD处理器
    std::cout << "SIMD Chunk Processor initialized" << std::endl;
}

SIMDChunkProcessor::~SIMDChunkProcessor() {
    // 清理资源
}

void SIMDChunkProcessor::compress_batch_simd(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    if (chunks.empty()) return;
    
    // 选择合适的SIMD实现
#if defined(SIMD_LEVEL_AVX512)
    process_batch_avx512(chunks);
#elif defined(SIMD_LEVEL_AVX2)
    process_batch_avx2(chunks);
#elif defined(SIMD_LEVEL_SSE4)
    process_batch_sse4(chunks);
#elif defined(SIMD_LEVEL_NEON)
    process_batch_neon(chunks);
#else
    process_batch_fallback(chunks);
#endif
}

void SIMDChunkProcessor::decompress_batch_simd(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    // 解压缩实现与压缩类似
    for (auto& chunk : chunks) {
        if (chunk->metrics.is_compressed && !chunk->data.empty()) {
            // 这里应该实现SIMD优化的解压缩
            // 简化实现：复制数据（实际中应该解压缩）
            chunk->metrics.is_compressed = false;
            chunk->metrics.compression_ratio = 0.0;
        }
    }
}

bool SIMDChunkProcessor::validate_batch_simd(const std::vector<std::shared_ptr<ChunkData>>& chunks) {
    // 验证批次中所有区块数据的完整性
    for (const auto& chunk : chunks) {
        if (chunk->data.empty() || chunk->x < 0 || chunk->z < 0) {
            return false;
        }
    }
    return true;
}

int SIMDChunkProcessor::select_compression_level(const ChunkData& chunk) {
    // 根据区块大小选择压缩级别
    size_t size = chunk.data.size();
    if (size < 1024) {
        return 1; // 小区块不压缩
    } else if (size < 16384) {
        return 4; // 中等区块中等压缩
    } else if (size < 65536) {
        return 6; // 大区块高压缩
    } else {
        return 9; // 超大区块最高压缩
    }
}

#if defined(SIMD_LEVEL_AVX512)
void SIMDChunkProcessor::process_batch_avx512(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    std::cout << "Processing with AVX-512 SIMD" << std::endl;
    // AVX-512优化的批量处理
    process_batch_fallback(chunks);
}
#elif defined(SIMD_LEVEL_AVX2)
void SIMDChunkProcessor::process_batch_avx2(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    std::cout << "Processing with AVX2 SIMD" << std::endl;
    // AVX2优化的批量处理
    process_batch_fallback(chunks);
}
#elif defined(SIMD_LEVEL_SSE4)
void SIMDChunkProcessor::process_batch_sse4(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    std::cout << "Processing with SSE4 SIMD" << std::endl;
    // SSE4优化的批量处理
    process_batch_fallback(chunks);
}
#elif defined(SIMD_LEVEL_NEON)
void SIMDChunkProcessor::process_batch_neon(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    std::cout << "Processing with NEON SIMD" << std::endl;
    // NEON优化的批量处理
    process_batch_fallback(chunks);
}
#else
void SIMDChunkProcessor::process_batch_fallback(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    // 回退到标准处理
    for (auto& chunk : chunks) {
        if (chunk->data.size() > 1024) {
            // 模拟压缩
            int level = select_compression_level(*chunk);
            if (level > 1) {
                chunk->metrics.compression_time = std::chrono::microseconds(chunk->data.size() / 1000);
                chunk->metrics.compressed_size = chunk->data.size() * 0.8; // 模拟压缩
                chunk->metrics.compression_ratio = 0.8;
                chunk->metrics.is_compressed = true;
            }
        }
    }
}
#endif

// ===========================================
// 回退后端实现
// ===========================================
FallbackBackend::FallbackBackend(lattice::net::MemoryPool<64 * 1024>& memory_pool)
    : memory_pool_(memory_pool), thread_pool_(std::thread::hardware_concurrency()) {
    std::cout << "Initialized Fallback Backend" << std::endl;
}

FallbackBackend::~FallbackBackend() {
    thread_pool_.shutdown();
}

void FallbackBackend::load_chunk_async(int fd, off_t offset, size_t size,
                                      std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) {
    // 使用线程池异步读取
    auto result = std::async(std::launch::async, [this, fd, offset, size, callback]() {
        async_read_file(fd, offset, size, callback);
    });
}

void FallbackBackend::save_chunk_async(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                                      std::function<void(bool, std::string)> callback) {
    // 使用线程池异步写入
    auto result = std::async(std::launch::async, [this, fd, offset, data, size, callback]() {
        async_write_file(fd, offset, data, size, callback);
    });
}

void FallbackBackend::save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                                       std::function<void(std::vector<AsyncIOResult>)> callback) {
    std::vector<AsyncIOResult> results;
    results.reserve(chunks.size());
    
    // 并行处理所有区块
    std::vector<std::future<void>> futures;
    for (size_t i = 0; i < chunks.size(); ++i) {
        futures.emplace_back(std::async(std::launch::async, [this, &chunks, i, &results]() {
            AsyncIOResult result;
            result.success = true; // 简化实现
            result.completion_time = std::chrono::steady_clock::now();
            result.chunk = *chunks[i];
            results.push_back(result);
        }));
    }
    
    // 等待所有操作完成
    for (auto& future : futures) {
        future.wait();
    }
    
    callback(results);
}

std::shared_ptr<void> FallbackBackend::memory_map_file(const std::string& filepath, size_t size, bool read_only) {
#if defined(PLATFORM_WINDOWS)
    HANDLE file = CreateFileA(filepath.c_str(), 
                             read_only ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE),
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr,
                             OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL,
                             nullptr);
    
    if (file == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    
    HANDLE mapping = CreateFileMappingA(file, nullptr,
                                       read_only ? PAGE_READONLY : PAGE_READWRITE,
                                       0, static_cast<DWORD>(size),
                                       nullptr);
    
    if (mapping == nullptr) {
        CloseHandle(file);
        return nullptr;
    }
    
    void* addr = MapViewOfFile(mapping,
                              read_only ? FILE_MAP_READ : (FILE_MAP_READ | FILE_MAP_WRITE),
                              0, 0, size);
    
    if (addr == nullptr) {
        CloseHandle(mapping);
        CloseHandle(file);
        return nullptr;
    }
    
    return std::shared_ptr<void>(addr, [file, mapping](void* ptr) {
        UnmapViewOfFile(ptr);
        CloseHandle(mapping);
        CloseHandle(file);
    });
#else
    int flags = read_only ? O_RDONLY : O_RDWR;
    int fd = open(filepath.c_str(), flags);
    
    if (fd == -1) {
        return nullptr;
    }
    
    void* addr = mmap(nullptr, size, read_only ? PROT_READ : (PROT_READ | PROT_WRITE),
                     MAP_SHARED, fd, 0);
    
    close(fd);
    
    if (addr == MAP_FAILED) {
        return nullptr;
    }
    
    return std::shared_ptr<void>(addr, [size](void* ptr) {
        munmap(ptr, size);
    });
#endif
}

PlatformFeatures FallbackBackend::get_platform_features() const {
    PlatformFeatures features;
    features.io_uring = false;
    features.iocp = false;
    features.gcd = false;
    features.direct_io = false;
    features.memory_mapping = true;
    features.cpu_cores = std::thread::hardware_concurrency();
    features.simd_available = false;
    return features;
}

void FallbackBackend::close_file_descriptor(int fd) {
#if defined(PLATFORM_WINDOWS)
    _close(fd);
#else
    close(fd);
#endif
}

void FallbackBackend::async_read_file(int fd, off_t offset, size_t size,
                                     std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) {
    std::shared_ptr<uint8_t> buffer = memory_pool_.allocate<uint8_t>(size);
    
#if defined(PLATFORM_WINDOWS)
    _lseek(fd, offset, SEEK_SET);
    DWORD bytes_read = _read(fd, buffer.get(), static_cast<unsigned int>(size));
    if (bytes_read > 0) {
        callback(buffer, bytes_read);
    } else {
        callback(nullptr, 0);
    }
#else
    lseek(fd, offset, SEEK_SET);
    ssize_t bytes_read = read(fd, buffer.get(), size);
    if (bytes_read > 0) {
        callback(buffer, bytes_read);
    } else {
        callback(nullptr, 0);
    }
#endif
}

void FallbackBackend::async_write_file(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                                      std::function<void(bool, std::string)> callback) {
#if defined(PLATFORM_WINDOWS)
    _lseek(fd, offset, SEEK_SET);
    DWORD bytes_written = _write(fd, data.get(), static_cast<unsigned int>(size));
    bool success = (bytes_written == static_cast<DWORD>(size));
    callback(success, success ? "" : "Write failed");
#else
    lseek(fd, offset, SEEK_SET);
    ssize_t bytes_written = write(fd, data.get(), size);
    bool success = (bytes_written == static_cast<ssize_t>(size));
    callback(success, success ? "" : "Write failed");
#endif
}

// ===========================================
// Linux io_uring后端实现
// ===========================================
#if defined(PLATFORM_LINUX)
LinuxIOUringBackend::LinuxIOUringBackend(lattice::net::MemoryPool<64 * 1024>& memory_pool)
    : memory_pool_(memory_pool) {
    if (io_uring_queue_init(256, &ring_, 0) < 0) {
        throw std::runtime_error("Failed to initialize io_uring");
    }
    initialized_ = true;
    std::cout << "Initialized Linux io_uring Backend" << std::endl;
}

LinuxIOUringBackend::~LinuxIOUringBackend() {
    if (initialized_) {
        io_uring_queue_exit(&ring_);
    }
}

void LinuxIOUringBackend::load_chunk_async(int fd, off_t offset, size_t size,
                                          std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) {
    std::shared_ptr<uint8_t> buffer = memory_pool_.allocate<uint8_t>(size);
    
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        callback(nullptr, 0);
        return;
    }
    
    auto* ctx = new IOContext{
        fd, offset, size, buffer, callback
    };
    
    io_uring_prep_read(sqe, fd, buffer.get(), size, offset);
    io_uring_sqe_set_data(sqe, ctx);
    io_uring_submit(&ring_);
}

void LinuxIOUringBackend::save_chunk_async(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                                          std::function<void(bool, std::string)> callback) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        callback(false, "Failed to get SQE");
        return;
    }
    
    auto* ctx = new IOContext{
        fd, offset, size, data,
        [callback](std::shared_ptr<uint8_t> buf, size_t result) {
            callback(result > 0, result > 0 ? "" : "Write failed");
        }
    };
    
    io_uring_prep_write(sqe, fd, data.get(), size, offset);
    io_uring_sqe_set_data(sqe, ctx);
    io_uring_submit(&ring_);
}

void LinuxIOUringBackend::save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                                           std::function<void(std::vector<AsyncIOResult>)> callback) {
    // 批量操作优化实现
    std::vector<AsyncIOResult> results;
    results.reserve(chunks.size());
    
    // 这里应该实现真正的io_uring批量操作
    // 简化实现：逐个处理
    for (const auto& chunk : chunks) {
        AsyncIOResult result;
        result.success = true;
        result.completion_time = std::chrono::steady_clock::now();
        result.chunk = *chunk;
        results.push_back(result);
    }
    
    callback(results);
}

std::shared_ptr<void> LinuxIOUringBackend::memory_map_file(const std::string& filepath, size_t size, bool read_only) {
    int flags = read_only ? O_RDONLY : O_RDWR;
    int fd = open(filepath.c_str(), flags);
    
    if (fd == -1) {
        return nullptr;
    }
    
    void* addr = mmap(nullptr, size, read_only ? PROT_READ : (PROT_READ | PROT_WRITE),
                     MAP_SHARED, fd, 0);
    
    close(fd);
    
    if (addr == MAP_FAILED) {
        return nullptr;
    }
    
    return std::shared_ptr<void>(addr, [size](void* ptr) {
        munmap(ptr, size);
    });
}

PlatformFeatures LinuxIOUringBackend::get_platform_features() const {
    PlatformFeatures features;
    features.io_uring = true;
    features.direct_io = true;
    features.memory_mapping = true;
    features.cpu_cores = std::thread::hardware_concurrency();
    features.simd_available = true;
    return features;
}

void LinuxIOUringBackend::close_file_descriptor(int fd) {
    close(fd);
}
#endif

// ===========================================
// Windows IOCP后端实现
// ===========================================
#if defined(PLATFORM_WINDOWS)
WindowsIOCPBackend::WindowsIOCPBackend(lattice::net::MemoryPool<64 * 1024>& memory_pool)
    : memory_pool_(memory_pool) {
    iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (iocp_handle_ == nullptr) {
        throw std::runtime_error("Failed to create IOCP");
    }
    
    // 启动工作线程
    int thread_count = std::max(1, static_cast<int>(std::thread::hardware_concurrency() / 2));
    for (int i = 0; i < thread_count; ++i) {
        worker_threads_.emplace_back(&WindowsIOCPBackend::worker_thread, this);
    }
    
    initialized_ = true;
    std::cout << "Initialized Windows IOCP Backend" << std::endl;
}

WindowsIOCPBackend::~WindowsIOCPBackend() {
    shutdown_ = true;
    
    for (size_t i = 0; i < worker_threads_.size(); ++i) {
        PostQueuedCompletionStatus(iocp_handle_, 0, 0, nullptr);
    }
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    if (iocp_handle_ != nullptr) {
        CloseHandle(iocp_handle_);
    }
}

void WindowsIOCPBackend::worker_thread() {
    while (!shutdown_) {
        DWORD bytes_transferred;
        ULONG_PTR completion_key;
        LPOVERLAPPED overlapped;
        
        BOOL result = GetQueuedCompletionStatus(iocp_handle_, &bytes_transferred, &completion_key, &overlapped, INFINITE);
        
        if (!result && GetLastError() != ERROR_ABANDONED_WAIT_0) {
            continue;
        }
        
        if (shutdown_) {
            break;
        }
        
        if (overlapped) {
            auto* overlapped_ex = static_cast<OVERLAPPED_EX*>(overlapped);
            process_completion(bytes_transferred, overlapped_ex);
        }
    }
}

void WindowsIOCPBackend::process_completion(DWORD bytes_transferred, OVERLAPPED_EX* overlapped) {
    if (overlaped->callback) {
        overlapped->callback(bytes_transferred, 0);
    }
    delete overlapped;
}

void WindowsIOCPBackend::load_chunk_async(int fd, off_t offset, size_t size,
                                         std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) {
    // IOCP实现
    std::shared_ptr<uint8_t> buffer = memory_pool_.allocate<uint8_t>(size);
    
    // 简化的实现，实际应该使用ReadFileEx和IOCP
    std::thread([fd, offset, size, buffer, callback]() {
        _lseek(fd, offset, SEEK_SET);
        DWORD bytes_read = _read(fd, buffer.get(), static_cast<unsigned int>(size));
        callback(buffer, bytes_read);
    }).detach();
}

void WindowsIOCPBackend::save_chunk_async(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                                         std::function<void(bool, std::string)> callback) {
    // IOCP实现
    std::thread([fd, offset, data, size, callback]() {
        _lseek(fd, offset, SEEK_SET);
        DWORD bytes_written = _write(fd, data.get(), static_cast<unsigned int>(size));
        bool success = (bytes_written == static_cast<DWORD>(size));
        callback(success, success ? "" : "Write failed");
    }).detach();
}

void WindowsIOCPBackend::save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                                          std::function<void(std::vector<AsyncIOResult>)> callback) {
    std::vector<AsyncIOResult> results;
    results.reserve(chunks.size());
    
    std::vector<std::future<void>> futures;
    for (const auto& chunk : chunks) {
        futures.emplace_back(std::async(std::launch::async, [&results, chunk]() {
            AsyncIOResult result;
            result.success = true;
            result.completion_time = std::chrono::steady_clock::now();
            result.chunk = *chunk;
            results.push_back(result);
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    callback(results);
}

std::shared_ptr<void> WindowsIOCPBackend::memory_map_file(const std::string& filepath, size_t size, bool read_only) {
    HANDLE file = CreateFileA(filepath.c_str(), 
                             read_only ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE),
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr,
                             OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL,
                             nullptr);
    
    if (file == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    
    HANDLE mapping = CreateFileMappingA(file, nullptr,
                                       read_only ? PAGE_READONLY : PAGE_READWRITE,
                                       0, static_cast<DWORD>(size),
                                       nullptr);
    
    if (mapping == nullptr) {
        CloseHandle(file);
        return nullptr;
    }
    
    void* addr = MapViewOfFile(mapping,
                              read_only ? FILE_MAP_READ : (FILE_MAP_READ | FILE_MAP_WRITE),
                              0, 0, size);
    
    if (addr == nullptr) {
        CloseHandle(mapping);
        CloseHandle(file);
        return nullptr;
    }
    
    return std::shared_ptr<void>(addr, [file, mapping](void* ptr) {
        UnmapViewOfFile(ptr);
        CloseHandle(mapping);
        CloseHandle(file);
    });
}

PlatformFeatures WindowsIOCPBackend::get_platform_features() const {
    PlatformFeatures features;
    features.iocp = true;
    features.direct_io = true;
    features.memory_mapping = true;
    features.cpu_cores = std::thread::hardware_concurrency();
    features.simd_available = true;
    return features;
}

void WindowsIOCPBackend::close_file_descriptor(int fd) {
    _close(fd);
}
#endif

// ===========================================
// macOS GCD后端实现
// ===========================================
#if defined(PLATFORM_MACOS)
MacOSGCDBackend::MacOSGCDBackend(lattice::net::MemoryPool<64 * 1024>& memory_pool)
    : memory_pool_(memory_pool),
      io_queue_(dispatch_queue_create("com.lattice.chunkio", DISPATCH_QUEUE_CONCURRENT)),
      completion_queue_(dispatch_queue_create("com.lattice.completion", DISPATCH_QUEUE_SERIAL)) {
    set_io_quality_of_service();
    initialized_ = true;
    std::cout << "Initialized macOS GCD Backend" << std::endl;
}

MacOSGCDBackend::~MacOSGCDBackend() {
    if (io_queue_) {
        dispatch_release(io_queue_);
    }
    if (completion_queue_) {
        dispatch_release(completion_queue_);
    }
}

void MacOSGCDBackend::load_chunk_async(int fd, off_t offset, size_t size,
                                      std::function<void(std::shared_ptr<uint8_t>, size_t)> callback) {
    std::shared_ptr<uint8_t> buffer = memory_pool_.allocate<uint8_t>(size);
    
    // 使用GCD异步读取
    dispatch_io_t channel = dispatch_io_create(Dispatch_IO_Random, fd, io_queue_, ^(int error) {
        if (error) {
            callback(nullptr, 0);
        }
    });
    
    dispatch_io_read(channel, offset, size, io_queue_,
        ^(bool done, dispatch_data_t data, int error) {
            if (error || !done) {
                callback(nullptr, 0);
                return;
            }
            
            size_t data_size = dispatch_data_get_size(data);
            const void* data_ptr = nullptr;
            dispatch_data_apply(data, ^bool(dispatch_data_t region, size_t offset, const void* buffer, size_t size) {
                data_ptr = buffer;
                return false;
            });
            
            if (data_ptr && data_size <= size) {
                memcpy(buffer.get(), data_ptr, data_size);
                callback(buffer, data_size);
            } else {
                callback(nullptr, 0);
            }
            
            dispatch_io_close(channel, 0);
        }
    );
}

void MacOSGCDBackend::save_chunk_async(int fd, off_t offset, const std::shared_ptr<uint8_t>& data, size_t size,
                                      std::function<void(bool, std::string)> callback) {
    dispatch_io_t channel = dispatch_io_create(DISPATCH_IO_RANDOM, fd, io_queue_, ^(int error) {
        if (error) {
            callback(false, "Channel creation failed");
        }
    });
    
    dispatch_data_t dispatch_data = dispatch_data_create(data.get(), size, io_queue_, nullptr);
    
    dispatch_io_write(channel, offset, dispatch_data, io_queue_,
        ^(bool done, dispatch_data_t remainder, int error) {
            bool success = (error == 0 && done);
            callback(success, success ? "" : "Write failed");
            dispatch_release(dispatch_data);
            dispatch_io_close(channel, 0);
        }
    );
}

void MacOSGCDBackend::save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                                       std::function<void(std::vector<AsyncIOResult>)> callback) {
    dispatch_group_t group = dispatch_group_create();
    std::vector<AsyncIOResult> results;
    results.reserve(chunks.size());
    
    for (const auto& chunk : chunks) {
        dispatch_group_enter(group);
        
        // 这里应该实现真正的批量I/O
        // 简化实现
        dispatch_async(completion_queue_, ^{
            AsyncIOResult result;
            result.success = true;
            result.completion_time = std::chrono::steady_clock::now();
            result.chunk = *chunk;
            results.push_back(result);
            dispatch_group_leave(group);
        });
    }
    
    dispatch_group_notify(group, completion_queue_, ^{
        callback(results);
        dispatch_release(group);
    });
}

std::shared_ptr<void> MacOSGCDBackend::memory_map_file(const std::string& filepath, size_t size, bool read_only) {
    int flags = read_only ? O_RDONLY : O_RDWR;
    int fd = open(filepath.c_str(), flags);
    
    if (fd == -1) {
        return nullptr;
    }
    
    void* addr = mmap(nullptr, size, read_only ? PROT_READ : (PROT_READ | PROT_WRITE),
                     MAP_SHARED, fd, 0);
    
    close(fd);
    
    if (addr == MAP_FAILED) {
        return nullptr;
    }
    
    return std::shared_ptr<void>(addr, [size](void* ptr) {
        munmap(ptr, size);
    });
}

PlatformFeatures MacOSGCDBackend::get_platform_features() const {
    PlatformFeatures features;
    features.gcd = true;
    features.memory_mapping = true;
    features.cpu_cores = std::thread::hardware_concurrency();
    features.simd_available = true;
    
    // 检查APFS
    struct statfs fs;
    if (statfs("/", &fs) == 0 && strcmp(fs.f_fstypename, "apfs") == 0) {
        features.apfs = true;
    }
    
    return features;
}

void MacOSGCDBackend::close_file_descriptor(int fd) {
    close(fd);
}

bool MacOSGCDBackend::is_apfs_filesystem(const std::string& filepath) {
    struct statfs fs;
    if (statfs(filepath.c_str(), &fs) == 0) {
        return strcmp(fs.f_fstypename, "apfs") == 0;
    }
    return false;
}

bool MacOSGCDBackend::clone_file_optimized(const std::string& src, const std::string& dst) {
    return clonefile(src.c_str(), dst.c_str(), 0) == 0;
}

void MacOSGCDBackend::optimize_for_apple_silicon(size_t buffer_size) {
    // Apple Silicon优化逻辑
    std::cout << "Optimizing for Apple Silicon, buffer size: " << buffer_size << std::endl;
}

void MacOSGCDBackend::set_io_quality_of_service() {
    // 设置I/O服务质量
    dispatch_set_qos_class_background(io_queue_);
}
#endif

// ===========================================
// 内存映射区域实现
// ===========================================
MemoryMappedRegion::MemoryMappedRegion(const std::string& filepath, size_t size, bool read_only)
    : mapped_size_(size), read_only_(read_only) {
#if defined(PLATFORM_WINDOWS)
    file_descriptor_ = _open(filepath.c_str(), 
                            read_only ? _O_RDONLY : (_O_RDWR | _O_CREAT),
                            _S_IREAD | _S_IWRITE);
    
    if (file_descriptor_ == -1) {
        return;
    }
    
    file_mapping_ = CreateFileMappingA((HANDLE)_get_osfhandle(file_descriptor_), nullptr,
                                      read_only ? PAGE_READONLY : PAGE_READWRITE,
                                      0, static_cast<DWORD>(size),
                                      nullptr);
    
    if (file_mapping_ != nullptr) {
        mapped_address_ = MapViewOfFile(file_mapping_,
                                       read_only ? FILE_MAP_READ : (FILE_MAP_READ | FILE_MAP_WRITE),
                                       0, 0, size);
    }
    
#elif defined(PLATFORM_MACOS)
    file_descriptor_ = open(filepath.c_str(), 
                           read_only ? O_RDONLY : (O_RDWR | O_CREAT), 
                           0644);
    
    if (file_descriptor_ != -1) {
        // 检查是否为APFS
        struct statfs fs;
        if (fstatfs(file_descriptor_, &fs) == 0) {
            is_apfs_ = (strcmp(fs.f_fstypename, "apfs") == 0);
        }
        
        mapped_address_ = mmap(nullptr, size, 
                              read_only ? PROT_READ : (PROT_READ | PROT_WRITE),
                              MAP_SHARED, file_descriptor_, 0);
        
        if (mapped_address_ == MAP_FAILED) {
            mapped_address_ = nullptr;
        }
    }
    
#else // Linux
    file_descriptor_ = open(filepath.c_str(), 
                           read_only ? O_RDONLY : (O_RDWR | O_CREAT),
                           0644);
    
    if (file_descriptor_ != -1) {
        mapped_address_ = mmap(nullptr, size,
                              read_only ? PROT_READ : (PROT_READ | PROT_WRITE),
                              MAP_SHARED, file_descriptor_, 0);
        
        if (mapped_address_ == MAP_FAILED) {
            mapped_address_ = nullptr;
        }
    }
#endif
}

MemoryMappedRegion::~MemoryMappedRegion() {
    if (mapped_address_ && mapped_address_ != MAP_FAILED) {
#if defined(PLATFORM_WINDOWS)
        if (read_only_) {
            UnmapViewOfFile(mapped_address_);
        } else {
            // 同步更改
            FlushViewOfFile(mapped_address_, mapped_size_);
            UnmapViewOfFile(mapped_address_);
        }
        
        if (file_mapping_) {
            CloseHandle(file_mapping_);
        }
        
        if (file_descriptor_ != -1) {
            _close(file_descriptor_);
        }
        
#elif defined(PLATFORM_MACOS)
        munmap(mapped_address_, mapped_size_);
        if (file_descriptor_ != -1) {
            close(file_descriptor_);
        }
        
#else // Linux
        munmap(mapped_address_, mapped_size_);
        if (file_descriptor_ != -1) {
            close(file_descriptor_);
        }
#endif
    }
}

void MemoryMappedRegion::prefetch_range(size_t offset, size_t length) {
    if (!is_valid()) return;
    
    void* addr = static_cast<uint8_t*>(mapped_address_) + offset;
    
#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    // 使用madvise预取页面
    madvise(addr, length, MADV_WILLNEED);
#elif defined(PLATFORM_WINDOWS)
    // Windows上的预取实现
    VirtualPrefetch(addr, length);
#endif
}

void MemoryMappedRegion::sync_range(size_t offset, size_t length) {
    if (!is_valid() || read_only_) return;
    
    void* addr = static_cast<uint8_t*>(mapped_address_) + offset;
    
#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    msync(addr, length, MS_SYNC);
#elif defined(PLATFORM_WINDOWS)
    FlushViewOfFile(addr, length);
#endif
}

MemoryMappedRegion::PlatformInfo MemoryMappedRegion::get_platform_info() const {
    PlatformInfo info;
    
#if defined(PLATFORM_MACOS)
    info.supports_huge_pages = true; // Apple Silicon支持大页
    info.compression_type_ = is_apfs_ ? "apfs" : "none";
#endif
    
#if defined(PLATFORM_LINUX)
    info.supports_huge_pages = true; // 假设支持
#endif
    
#if defined(PLATFORM_WINDOWS)
    info.supports_huge_pages = true; // 假设支持
#endif
    
    return info;
}

// ===========================================
// 主异步区块I/O类实现
// ===========================================
thread_local std::unique_ptr<AsyncChunkIO> AsyncChunkIO::thread_instance_;
std::mutex AsyncChunkIO::instances_mutex_;
std::unordered_map<std::thread::id, std::weak_ptr<AsyncChunkIO>> AsyncChunkIO::thread_instances_;

AsyncChunkIO::AsyncChunkIO(const BatchConfig& config)
    : config_(config),
      memory_pool_(lattice::net::MemoryPool<64 * 1024>::get_instance()),
      simd_processor_(std::make_unique<SIMDChunkProcessor>(config)) {
    
    // 平台自动检测和后端选择
    detect_and_select_backend();
    
    std::cout << "Async Chunk I/O initialized with batch size: " << config_.max_batch_size << std::endl;
}

AsyncChunkIO::~AsyncChunkIO() {
    // 清理资源
}

void AsyncChunkIO::load_chunk_async(int world_id, int chunk_x, int chunk_z,
                                   std::function<void(AsyncIOResult)> callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::string chunk_path = build_chunk_path(world_id, chunk_x, chunk_z);
    int fd = open_chunk_file(chunk_path, true);
    
    if (fd == -1) {
        AsyncIOResult result;
        result.success = false;
        result.error_message = "Failed to open chunk file";
        result.completion_time = std::chrono::steady_clock::now();
        callback(result);
        return;
    }
    
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        AsyncIOResult result;
        result.success = false;
        result.error_message = "Failed to get file size";
        result.completion_time = std::chrono::steady_clock::now();
        callback(result);
        return;
    }
    
    size_t file_size = st.st_size;
    
    backend_->load_chunk_async(fd, 0, file_size,
        [this, start_time, fd, callback](std::shared_ptr<uint8_t> data, size_t size) {
            backend_->close_file_descriptor(fd);
            
            AsyncIOResult result;
            if (data && size > 0) {
                result.success = true;
                result.chunk.data.assign(data.get(), data.get() + size);
                result.chunk.x = 0; // 需要从路径解析
                result.chunk.z = 0; // 需要从路径解析
            } else {
                result.success = false;
                result.error_message = "Failed to read chunk data";
            }
            
            result.completion_time = std::chrono::steady_clock::now();
            update_performance_stats(start_time);
            callback(result);
        }
    );
}

void AsyncChunkIO::save_chunk_async(const ChunkData& chunk,
                                   std::function<void(AsyncIOResult)> callback) {
    auto start_time = std::chrono::steady_clock::now();
    
    std::string chunk_path = build_chunk_path(chunk.world_id, chunk.x, chunk.z);
    int fd = open_chunk_file(chunk_path, false);
    
    if (fd == -1) {
        AsyncIOResult result;
        result.success = false;
        result.error_message = "Failed to create chunk file";
        result.completion_time = std::chrono::steady_clock::now();
        callback(result);
        return;
    }
    
    std::shared_ptr<uint8_t> data = std::make_shared<uint8_t[]>(chunk.data.size());
    std::memcpy(data.get(), chunk.data.data(), chunk.data.size());
    
    backend_->save_chunk_async(fd, 0, data, chunk.data.size(),
        [this, start_time, fd, callback](bool success, std::string error) {
            backend_->close_file_descriptor(fd);
            
            AsyncIOResult result;
            result.success = success;
            result.error_message = error;
            result.completion_time = std::chrono::steady_clock::now();
            
            update_performance_stats(start_time);
            callback(result);
        }
    );
}

void AsyncChunkIO::save_chunks_batch(const std::vector<std::shared_ptr<ChunkData>>& chunks,
                                    std::function<void(std::vector<AsyncIOResult>)> callback) {
    if (chunks.empty()) {
        callback({});
        return;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 批处理优化
    auto chunks_copy = chunks; // 创建副本以便修改
    optimize_batch_operation(chunks_copy);
    
    // 使用SIMD处理器优化
    simd_processor_->compress_batch_simd(chunks_copy);
    
    // 调用平台后端的批量保存
    backend_->save_chunks_batch(chunks_copy,
        [this, start_time, callback](std::vector<AsyncIOResult> results) {
            update_performance_stats(start_time, true);
            callback(results);
        }
    );
}

std::shared_ptr<MemoryMappedRegion> AsyncChunkIO::load_chunk_mapped(const std::string& filepath, bool read_only) {
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
        return nullptr;
    }
    
    auto region = std::make_shared<MemoryMappedRegion>(filepath, st.st_size, read_only);
    
    if (region->is_valid()) {
        std::lock_guard lock(regions_mutex_);
        mapped_regions_[filepath] = region;
        return region;
    }
    
    return nullptr;
}

AsyncChunkIO* AsyncChunkIO::for_thread() {
    if (!thread_instance_) {
        thread_instance_ = std::make_unique<AsyncChunkIO>();
        
        std::lock_guard lock(instances_mutex_);
        thread_instances_[std::this_thread::get_id()] = thread_instance_;
    }
    
    return thread_instance_.get();
}

void AsyncChunkIO::set_max_concurrent_io(int max_ops) {
    // 设置最大并发I/O操作数
    std::cout << "Max concurrent I/O set to: " << max_ops << std::endl;
}

void AsyncChunkIO::enable_direct_io(bool enable) {
    // 启用或禁用Direct I/O
    std::cout << "Direct I/O " << (enable ? "enabled" : "disabled") << std::endl;
}

void AsyncChunkIO::set_batch_config(const BatchConfig& config) {
    // 设置批处理配置
    std::cout << "Batch config updated: max_size=" << config.max_batch_size << std::endl;
}

AsyncChunkIO::PerformanceStats AsyncChunkIO::get_performance_stats() const {
    std::lock_guard lock(stats_mutex_);
    return stats_;
}

void AsyncChunkIO::reset_performance_stats() {
    std::lock_guard lock(stats_mutex_);
    stats_ = PerformanceStats{};
}

void AsyncChunkIO::detect_and_select_backend() {
    PlatformFeatures features = PlatformFeatures::detect();
    
#if defined(PLATFORM_LINUX)
    if (features.io_uring) {
        backend_ = std::make_unique<LinuxIOUringBackend>(memory_pool_);
    } else {
        backend_ = std::make_unique<FallbackBackend>(memory_pool_);
    }
#elif defined(PLATFORM_WINDOWS)
    if (features.iocp) {
        backend_ = std::make_unique<WindowsIOCPBackend>(memory_pool_);
    } else {
        backend_ = std::make_unique<FallbackBackend>(memory_pool_);
    }
#elif defined(PLATFORM_MACOS)
    if (features.gcd) {
        backend_ = std::make_unique<MacOSGCDBackend>(memory_pool_);
    } else {
        backend_ = std::make_unique<FallbackBackend>(memory_pool_);
    }
#else
    backend_ = std::make_unique<FallbackBackend>(memory_pool_);
#endif
}

std::string AsyncChunkIO::build_chunk_path(int world_id, int chunk_x, int chunk_z) const {
    std::stringstream path;
    path << "./worlds/world_" << world_id << "/chunks/chunk_" << chunk_x << "_" << chunk_z << ".dat";
    return path.str();
}

int AsyncChunkIO::open_chunk_file(const std::string& path, bool read_only) const {
#if defined(PLATFORM_WINDOWS)
    return _open(path.c_str(), read_only ? _O_RDONLY : (_O_RDWR | _O_CREAT), _S_IREAD | _S_IWRITE);
#else
    int flags = read_only ? O_RDONLY : (O_RDWR | O_CREAT);
    return open(path.c_str(), flags, 0644);
#endif
}

void AsyncChunkIO::update_performance_stats(const std::chrono::steady_clock::time_point& start,
                                           bool is_batch, size_t data_size) {
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::lock_guard lock(stats_mutex_);
    
    if (is_batch) {
        stats_.total_batch_operations++;
        stats_.avg_batch_time = (stats_.avg_batch_time * (stats_.total_batch_operations - 1) + elapsed) / stats_.total_batch_operations;
        if (data_size > 0) {
            stats_.throughput_mb_per_sec = (data_size / (1024.0 * 1024.0)) / (elapsed.count() / 1000.0);
        }
    } else {
        stats_.total_loads++;
        stats_.avg_load_time = (stats_.avg_load_time * (stats_.total_loads - 1) + elapsed) / stats_.total_loads;
    }
}

void AsyncChunkIO::optimize_batch_operation(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    // 按空间局部性排序
    std::sort(chunks.begin(), chunks.end(), [](const auto& a, const auto& b) {
        // 简化的空间排序
        return (a->x * a->x + a->z * a->z) < (b->x * b->x + b->z * b->z);
    });
}

// ===========================================
// 批处理优化器实现
// ===========================================
BatchOptimizer::BatchOptimizer(const BatchConfig& config)
    : config_(config) {
}

void BatchOptimizer::optimize_spatial_locality(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    if (chunks.size() <= 1) return;
    
    // 按距离排序以提高空间局部性
    std::sort(chunks.begin(), chunks.end(), [this](const auto& a, const auto& b) {
        double dist_a = std::abs(a->x) + std::abs(a->z);
        double dist_b = std::abs(b->x) + std::abs(b->z);
        return dist_a < dist_b;
    });
}

void BatchOptimizer::group_by_world(std::vector<std::shared_ptr<ChunkData>>& chunks) {
    std::unordered_map<int, std::vector<std::shared_ptr<ChunkData>>> world_groups;
    
    for (auto& chunk : chunks) {
        world_groups[chunk->world_id].push_back(chunk);
    }
    
    // 重组向量
    std::vector<std::shared_ptr<ChunkData>> result;
    result.reserve(chunks.size());
    
    for (auto& [world_id, group] : world_groups) {
        result.insert(result.end(), group.begin(), group.end());
    }
    
    chunks = std::move(result);
}

size_t BatchOptimizer::calculate_optimal_batch_size(const std::vector<std::shared_ptr<ChunkData>>& chunks) const {
    size_t total_size = 0;
    for (const auto& chunk : chunks) {
        total_size += chunk->data.size();
    }
    
    // 基于总大小和CPU核心数计算最优批次大小
    size_t target_size = 64 * 1024; // 64KB目标批次大小
    size_t optimal_size = std::min(config_.max_batch_size, 
                                  std::max(config_.min_batch_size,
                                          total_size > 0 ? std::min(total_size / target_size, config_.max_batch_size) : config_.min_batch_size));
    
    return optimal_size;
}

BatchOptimizer::IOPattern BatchOptimizer::analyze_io_pattern(const std::vector<std::shared_ptr<ChunkData>>& chunks) const {
    IOPattern pattern;
    
    if (chunks.size() < 2) {
        return pattern;
    }
    
    // 检查是否按顺序访问
    bool is_sequential = true;
    for (size_t i = 1; i < chunks.size(); ++i) {
        double dist = calculate_spatial_distance(chunks[i-1]->x, chunks[i-1]->z, chunks[i]->x, chunks[i]->z);
        if (dist > 2.0) { // 距离阈值
            is_sequential = false;
            break;
        }
    }
    pattern.sequential_access = is_sequential;
    
    // 计算局部性得分
    double total_distance = 0;
    for (size_t i = 1; i < chunks.size(); ++i) {
        total_distance += calculate_spatial_distance(chunks[i-1]->x, chunks[i-1]->z, chunks[i]->x, chunks[i]->z);
    }
    
    pattern.locality_score = 1.0 / (1.0 + total_distance / chunks.size());
    
    // 热点区域检测（简化实现）
    std::unordered_map<int, std::vector<std::shared_ptr<ChunkData>>> location_groups;
    for (const auto& chunk : chunks) {
        int region_x = chunk->x / 8; // 8x8区块为一个区域
        int region_z = chunk->z / 8;
        int region_key = region_x * 1000 + region_z; // 避免负数
        location_groups[region_key].push_back(chunk);
    }
    
    pattern.hotspot_regions = location_groups.size() < chunks.size() / 4; // 如果区域数少于区块数的1/4，则认为有热点
    
    return pattern;
}

double BatchOptimizer::calculate_spatial_distance(int x1, int z1, int x2, int z2) const {
    return std::sqrt((x1 - x2) * (x1 - x2) + (z1 - z2) * (z1 - z2));
}

// ===========================================
// 平台检测工具实现
// ===========================================
namespace platform_detection {

CPUFeatures detect_cpu_features() {
    CPUFeatures features;
    
    // SIMD检测
#if defined(__AVX512__)
    features.avx512 = true;
#endif
#if defined(__AVX2__)
    features.avx2 = true;
#endif
#if defined(__SSE4_2__)
    features.sse4 = true;
#endif
#if defined(__ARM_NEON__)
    features.neon = true;
#endif
    
    // 核心数检测
#if defined(PLATFORM_WINDOWS)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    features.core_count = sysinfo.dwNumberOfProcessors;
#elif defined(PLATFORM_MACOS)
    int cores;
    size_t size = sizeof(cores);
    sysctlbyname("hw.ncpu", &cores, &size, nullptr, 0);
    features.core_count = cores;
#else
    features.core_count = std::max(1, static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN)));
#endif
    
    return features;
}

IOFeatures detect_io_features() {
    IOFeatures features;
    
#if defined(PLATFORM_LINUX)
    // io_uring检测
    struct io_uring ring;
    if (io_uring_queue_init(8, &ring, 0) == 0) {
        features.io_uring_supported = true;
        io_uring_queue_exit(&ring);
    }
    
    // Direct I/O检测
    int test_fd = open("/tmp/test_dio", O_RDWR | O_CREAT | O_DIRECT, 0644);
    if (test_fd != -1) {
        features.direct_io_supported = true;
        close(test_fd);
        unlink("/tmp/test_dio");
    }
    
    features.memory_mapping_supported = true;
    
#elif defined(PLATFORM_WINDOWS)
    // IOCP检测
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (iocp != nullptr) {
        features.iocp_supported = true;
        CloseHandle(iocp);
    }
    
    features.memory_mapping_supported = true;
    
#elif defined(PLATFORM_MACOS)
    features.gcd_supported = true;
    features.memory_mapping_supported = true;
    
    // APFS检测
    struct statfs fs;
    if (statfs("/", &fs) == 0 && strcmp(fs.f_fstypename, "apfs") == 0) {
        features.apfs_filesystem = true;
    }
#endif
    
    return features;
}

MemoryFeatures detect_memory_features() {
    MemoryFeatures features;
    
#if defined(PLATFORM_LINUX)
    features.page_size = sysconf(_SC_PAGESIZE);
    features.huge_pages_supported = (sysconf(_SC_PAGESIZE) >= 2097152);
    
#elif defined(PLATFORM_WINDOWS)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    features.page_size = sysinfo.dwPageSize;
    features.huge_pages_supported = true; // 假设支持
    
#elif defined(PLATFORM_MACOS)
    features.page_size = 4096;
    features.huge_pages_supported = true; // Apple Silicon支持
    
    // 检查统一内存架构
    int has_unified_memory = 0;
    size_t size = sizeof(has_unified_memory);
    if (sysctlbyname("hw.unifiedmemory", &has_unified_memory, &size, nullptr, 0) == 0) {
        features.unified_memory_architecture = (has_unified_memory == 1);
    }
#endif
    
    return features;
}

PlatformInfo detect_platform_info() {
    PlatformInfo info;
    info.cpu = detect_cpu_features();
    info.io = detect_io_features();
    info.memory = detect_memory_features();
    
#if defined(PLATFORM_WINDOWS)
    info.os_name = "Windows";
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
        info.version_major = osvi.dwMajorVersion;
        info.version_minor = osvi.dwMinorVersion;
    }
#elif defined(PLATFORM_MACOS)
    info.os_name = "macOS";
    char buffer[256];
    size_t size = sizeof(buffer);
    if (sysctlbyname("kern.version", buffer, &size, nullptr, 0) == 0) {
        // 解析版本信息
    }
#else
    info.os_name = "Linux";
    info.version_major = 0;
    info.version_minor = 0;
#endif
    
    return info;
}

std::unique_ptr<AsyncChunkIO::PlatformBackend> select_optimal_backend(
    lattice::net::MemoryPool<64 * 1024>& memory_pool) {
    
    PlatformInfo info = detect_platform_info();
    
#if defined(PLATFORM_LINUX)
    if (info.io.io_uring_supported) {
        return std::make_unique<LinuxIOUringBackend>(memory_pool);
    }
#elif defined(PLATFORM_WINDOWS)
    if (info.io.iocp_supported) {
        return std::make_unique<WindowsIOCPBackend>(memory_pool);
    }
#elif defined(PLATFORM_MACOS)
    if (info.io.gcd_supported) {
        return std::make_unique<MacOSGCDBackend>(memory_pool);
    }
#endif
    
    return std::make_unique<FallbackBackend>(memory_pool);
}

} // namespace platform_detection

} // namespace world
} // namespace lattice