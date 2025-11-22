/*
 * Test file for Lattice Optimization Module
 */

#include "lattice_optimization.hpp"
#include <iostream>
#include <chrono>
#include <cstring>

using namespace lattice::optimization;

void test_basic_functionality() {
    std::cout << "Testing basic optimization functionality..." << std::endl;
    
    // Test fast_memcpy
    const size_t test_size = 1024;
    char src[test_size];
    char dst[test_size];
    
    // Fill source with pattern
    for (size_t i = 0; i < test_size; i++) {
        src[i] = static_cast<char>(i % 256);
    }
    
    // Test fast_memcpy
    auto start = std::chrono::high_resolution_clock::now();
    fast_memcpy(dst, src, test_size);
    auto end = std::chrono::high_resolution_clock::now();
    
    // Verify data integrity
    bool correct = true;
    for (size_t i = 0; i < test_size; i++) {
        if (dst[i] != src[i]) {
            correct = false;
            break;
        }
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    
    std::cout << "  Fast memcpy test: " << (correct ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Time: " << duration.count() << " ns" << std::endl;
    std::cout << "  Throughput: " << (test_size * 1000.0 / duration.count()) << " GB/s" << std::endl;
}

void test_memory_pool() {
    std::cout << "\nTesting memory pool..." << std::endl;
    
    // Test small allocations
    void* ptr1 = globalMemoryPool.allocate(1024);
    void* ptr2 = globalMemoryPool.allocate(2048);
    void* ptr3 = globalMemoryPool.allocate(4096);
    
    if (ptr1 && ptr2 && ptr3) {
        std::cout << "  Memory allocation: PASS" << std::endl;
        
        // Fill with data
        memset(ptr1, 0xAA, 1024);
        memset(ptr2, 0xBB, 2048);
        memset(ptr3, 0xCC, 4096);
        
        // Deallocate
        globalMemoryPool.deallocate(ptr1, 1024);
        globalMemoryPool.deallocate(ptr2, 2048);
        globalMemoryPool.deallocate(ptr3, 4096);
        
        std::cout << "  Memory deallocation: PASS" << std::endl;
    } else {
        std::cout << "  Memory allocation: FAIL" << std::endl;
    }
}

void test_mmap_manager() {
    std::cout << "\nTesting mmap manager..." << std::endl;
    
    const size_t test_size = 1024 * 1024; // 1MB
    void* mapped = globalMMAPManager.createSharedBuffer(test_size);
    
    if (mapped) {
        std::cout << "  mmap allocation: PASS" << std::endl;
        
        // Test data writing/reading
        char* buffer = static_cast<char*>(mapped);
        for (size_t i = 0; i < 1024; i++) {
            buffer[i] = static_cast<char>(i % 256);
        }
        
        bool correct = true;
        for (size_t i = 0; i < 1024; i++) {
            if (buffer[i] != static_cast<char>(i % 256)) {
                correct = false;
                break;
            }
        }
        
        std::cout << "  Data integrity: " << (correct ? "PASS" : "FAIL") << std::endl;
        
        // Cleanup
        globalMMAPManager.releaseSharedBuffer(mapped);
        std::cout << "  mmap deallocation: PASS" << std::endl;
    } else {
        std::cout << "  mmap allocation: FAIL" << std::endl;
    }
}

void test_cpu_detection() {
    std::cout << "\nTesting CPU detection..." << std::endl;
    
    SIMDFeatures features;
    CPUArchitecture arch;
    detectCPUArchitecture(features, arch);
    
    std::cout << "  CPU Architecture: ";
    switch (arch) {
        case CPUArchitecture::X86_64_AVX512:
            std::cout << "x86-64-AVX512";
            break;
        case CPUArchitecture::X86_64_AVX2:
            std::cout << "x86-64-AVX2";
            break;
        case CPUArchitecture::X86_64_AVX:
            std::cout << "x86-64-AVX";
            break;
        case CPUArchitecture::X86_64_SSE2:
            std::cout << "x86-64-SSE2";
            break;
        case CPUArchitecture::ARM64_SVE:
            std::cout << "ARM64-SVE";
            break;
        case CPUArchitecture::ARM64_NEON:
            std::cout << "ARM64-NEON";
            break;
        default:
            std::cout << "Unknown";
            break;
    }
    std::cout << std::endl;
    
    std::cout << "  SIMD Features:" << std::endl;
#ifdef __AVX512F__
    if (features.avx512f_supported) std::cout << "    ✓ AVX-512F" << std::endl;
#endif
#ifdef __AVX2__
    if (features.avx2_supported) std::cout << "    ✓ AVX2" << std::endl;
#endif
#ifdef __AVX__
    if (features.avx_supported) std::cout << "    ✓ AVX" << std::endl;
#endif
#ifdef __SSE2__
    if (features.sse2_supported) std::cout << "    ✓ SSE2" << std::endl;
#endif
}

void test_batch_processing() {
    std::cout << "\nTesting batch processing..." << std::endl;
    
    const int batch_count = 10;
    const size_t block_size = 1024;
    
    // Allocate test data
    void** srcs = (void**)malloc(batch_count * sizeof(void*));
    void** dsts = (void**)malloc(batch_count * sizeof(void*));
    size_t* sizes = (size_t*)malloc(batch_count * sizeof(size_t));
    
    if (!srcs || !dsts || !sizes) {
        std::cout << "  Batch allocation: FAIL" << std::endl;
        return;
    }
    
    // Initialize test data
    for (int i = 0; i < batch_count; i++) {
        srcs[i] = malloc(block_size);
        dsts[i] = malloc(block_size);
        sizes[i] = block_size;
        
        if (srcs[i] && dsts[i]) {
            // Fill source with pattern
            memset(srcs[i], static_cast<char>(i % 256), block_size);
        }
    }
    
    // Test batch processing
    auto start = std::chrono::high_resolution_clock::now();
    batch_contiguous_copy(dsts, srcs, sizes, batch_count);
    auto end = std::chrono::high_resolution_clock::now();
    
    // Verify data integrity
    bool correct = true;
    for (int i = 0; i < batch_count; i++) {
        if (srcs[i] && dsts[i]) {
            if (memcmp(srcs[i], dsts[i], block_size) != 0) {
                correct = false;
                break;
            }
        }
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "  Batch processing: " << (correct ? "PASS" : "FAIL") << std::endl;
    std::cout << "  Time: " << duration.count() << " μs" << std::endl;
    std::cout << "  Throughput: " << (batch_count * block_size * 1000.0 / duration.count()) / 1024.0 << " MB/s" << std::endl;
    
    // Cleanup
    for (int i = 0; i < batch_count; i++) {
        if (srcs[i]) free(srcs[i]);
        if (dsts[i]) free(dsts[i]);
    }
    free(srcs);
    free(dsts);
    free(sizes);
}

int main() {
    std::cout << "Lattice Optimization Module Test Suite" << std::endl;
    std::cout << "======================================" << std::endl;
    
    try {
        test_basic_functionality();
        test_memory_pool();
        test_mmap_manager();
        test_cpu_detection();
        test_batch_processing();
        
        std::cout << "\n✅ All tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}