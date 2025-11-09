#include <jni.h>
#include "../jni_helper.hpp"
#include "../../core/net/native_compressor.hpp"
#include <memory>
#include <vector>
#include <cstring>

using lattice::net::NativeCompressor;

extern "C" {

JNIEXPORT jbyteArray JNICALL Java_io_lattice_network_NativeCompression_compressDeflate
  (JNIEnv* env, jclass, jbyteArray data, jint compressionLevel) {
    if (!data) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "data is null");
        return nullptr;
    }

    try {
        auto input = JniHelper::getByteArrayContent(env, data);
        NativeCompressor* compressor = NativeCompressor::forThread(static_cast<int>(compressionLevel));

        size_t inputLen = input.size();
        // conservative bound
        size_t bound = inputLen + inputLen / 8 + 64;
        std::vector<uint8_t> out(bound);

        size_t written = compressor->compressDeflate(reinterpret_cast<const char*>(input.data()), inputLen,
                                                     reinterpret_cast<char*>(out.data()), out.size());
        if (written == (size_t)-1) {
            JniHelper::throwJavaException(env, "java/io/IOException", "zlib compress failed");
            return nullptr;
        }

        return JniHelper::createByteArray(env, out.data(), written);
    } catch (const std::exception& e) {
        JniHelper::throwJavaException(env, "java/lang/RuntimeException", e.what());
        return nullptr;
    }
}

JNIEXPORT jbyteArray JNICALL Java_io_lattice_network_NativeCompression_decompressDeflate
  (JNIEnv* env, jclass, jbyteArray data, jint decompressedSize) {
    if (!data) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "data is null");
        return nullptr;
    }
    if (decompressedSize <= 0) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "invalid decompressedSize");
        return nullptr;
    }

    try {
        auto input = JniHelper::getByteArrayContent(env, data);
        NativeCompressor* compressor = NativeCompressor::forThread(3);

        std::vector<uint8_t> out(static_cast<size_t>(decompressedSize));
        size_t written = compressor->decompressDeflate(reinterpret_cast<const char*>(input.data()), input.size(),
                                                       reinterpret_cast<char*>(out.data()), out.size());
        if (written == (size_t)-1 || written != static_cast<size_t>(decompressedSize)) {
            JniHelper::throwJavaException(env, "java/io/IOException", "zlib decompression failed or size mismatch");
            return nullptr;
        }

        return JniHelper::createByteArray(env, out.data(), written);
    } catch (const std::exception& e) {
        JniHelper::throwJavaException(env, "java/lang/RuntimeException", e.what());
        return nullptr;
    }
}

// High-performance direct buffer APIs (zero-copy). Return compressed size as jint, or -1 on error.
JNIEXPORT jint JNICALL Java_io_lattice_network_NativeCompression_compressDeflateDirect
  (JNIEnv* env, jclass, jobject srcDirect, jint srcLen, jobject dstDirect, jint dstCapacity, jint compressionLevel) {
    if (!srcDirect || !dstDirect) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "direct buffer is null");
        return -1;
    }

    void* srcPtr = env->GetDirectBufferAddress(srcDirect);
    void* dstPtr = env->GetDirectBufferAddress(dstDirect);
    if (!srcPtr || !dstPtr) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "buffer is not direct or inaccessible");
        return -1;
    }

    if (srcLen <= 0 || dstCapacity <= 0) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "invalid lengths");
        return -1;
    }

    try {
        NativeCompressor* compressor = NativeCompressor::forThread(static_cast<int>(compressionLevel));
        size_t written = compressor->compressDeflate(reinterpret_cast<const char*>(srcPtr), static_cast<size_t>(srcLen),
                                                     reinterpret_cast<char*>(dstPtr), static_cast<size_t>(dstCapacity));
        if (written == (size_t)-1) {
            JniHelper::throwJavaException(env, "java/io/IOException", "zlib compression failed");
            return -1;
        }
        return static_cast<jint>(written);
    } catch (const std::exception& e) {
        JniHelper::throwJavaException(env, "java/lang/RuntimeException", e.what());
        return -1;
    }
}

JNIEXPORT jint JNICALL Java_io_lattice_network_NativeCompression_decompressDeflateDirect
  (JNIEnv* env, jclass, jobject srcDirect, jint srcLen, jobject dstDirect, jint dstCapacity) {
    if (!srcDirect || !dstDirect) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "direct buffer is null");
        return -1;
    }

    void* srcPtr = env->GetDirectBufferAddress(srcDirect);
    void* dstPtr = env->GetDirectBufferAddress(dstDirect);
    if (!srcPtr || !dstPtr) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "buffer is not direct or inaccessible");
        return -1;
    }

    if (srcLen <= 0 || dstCapacity <= 0) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "invalid lengths");
        return -1;
    }

    jint maxAllowed = 8 * 1024 * 1024; // fallback
    jclass cls = env->FindClass("io/lattice/network/NativeCompression");
    if (cls) {
        jfieldID fid = env->GetStaticFieldID(cls, "MAX_DECOMPRESSED_SIZE", "I");
        if (fid) {
            maxAllowed = env->GetStaticIntField(cls, fid);
        }
        env->DeleteLocalRef(cls);
    }

    if (dstCapacity > maxAllowed) {
        JniHelper::throwJavaException(env, "java/lang/IllegalArgumentException", "dstCapacity exceeds allowed maximum");
        return -1;
    }

    try {
        NativeCompressor* compressor = NativeCompressor::forThread(3);
        size_t written = compressor->decompressDeflate(reinterpret_cast<const char*>(srcPtr), static_cast<size_t>(srcLen),
                                                       reinterpret_cast<char*>(dstPtr), static_cast<size_t>(dstCapacity));
        if (written == (size_t)-1) {
            JniHelper::throwJavaException(env, "java/io/IOException", "zlib decompression failed");
            return -1;
        }
        return static_cast<jint>(written);
    } catch (const std::exception& e) {
        JniHelper::throwJavaException(env, "java/lang/RuntimeException", e.what());
        return -1;
    }
}

JNIEXPORT jint JNICALL Java_io_lattice_network_NativeCompression_deflateCompressBound(JNIEnv*, jclass, jint srcLen) {
    if (srcLen <= 0) return 0;
    // zlib does not provide a single-shot bound function; return conservative estimate
    size_t bound = static_cast<size_t>(srcLen) + static_cast<size_t>(srcLen) / 8 + 64;
    if (bound > INT32_MAX) return INT32_MAX;
    return static_cast<jint>(bound);
}

} // extern "C"
