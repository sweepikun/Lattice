#pragma once

// Minimal fmt header for formatting (simple implementation)
#include <iostream>
#include <string>
#include <cstdio>
#include <cstdarg>

// Simple fmt implementation using printf
namespace fmt {
    
    // Variadic macro for printf-like formatting
    #define FMT_PRINT(fmt_str, ...) \
        printf((fmt_str), ##__VA_ARGS__)
    
    #define FMT_PRINT_STDERR(fmt_str, ...) \
        fprintf(stderr, (fmt_str), ##__VA_ARGS__)
    
    // Keep the old function signatures for compatibility but delegate to macros
    inline void print(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
    
    inline void print_stderr(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
    }
    
    // fmt::chrono integration  
    namespace chrono {
        template<typename Rep, typename Period>
        struct duration_values {};
    }
}

// Add namespace aliases for easier use
namespace lattice {
    namespace fmt = ::fmt;
}