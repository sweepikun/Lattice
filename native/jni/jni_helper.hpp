#pragma once

#include <jni.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <atomic>
#include <type_traits>

class JniHelper {
public:
    // Convert JNI byte array to std::vector<uint8_t>
    static std::vector<uint8_t> getByteArrayContent(JNIEnv* env, jbyteArray array) {
        if (!array) {
            throw std::invalid_argument("Null array passed to getByteArrayContent");
        }

        jsize length = env->GetArrayLength(array);
        std::vector<uint8_t> buffer(length);
        
        env->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte*>(buffer.data()));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            throw std::runtime_error("Failed to get byte array content");
        }

        return buffer;
    }

    // Create a new JNI byte array from native buffer
    static jbyteArray createByteArray(JNIEnv* env, const uint8_t* data, size_t length) {
        if (!data && length > 0) {
            throw std::invalid_argument("Null data passed to createByteArray");
        }

        jbyteArray result = env->NewByteArray(static_cast<jsize>(length));
        if (!result) {
            throw std::runtime_error("Failed to create byte array");
        }

        if (length > 0) {
            env->SetByteArrayRegion(result, 0, static_cast<jsize>(length), reinterpret_cast<const jbyte*>(data));
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                throw std::runtime_error("Failed to set byte array content");
            }
        }

        return result;
    }

    // Throw a Java exception
    static void throwJavaException(JNIEnv* env, const char* exceptionClass, const char* message) {
        jclass cls = env->FindClass(exceptionClass);
        if (cls) {
            env->ThrowNew(cls, message);
        }
    }
};

// JNI环境检查和异常处理
class JNIExceptionGuard {
private:
    JNIEnv* env_;
    bool hadException_;
    
public:
    explicit JNIExceptionGuard(JNIEnv* env) : env_(env), hadException_(false) {
        if (env->ExceptionCheck()) {
            hadException_ = true;
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
    }
    
    ~JNIExceptionGuard() {
        if (hadException_) {
            // 可以记录日志或执行清理
        }
    }
    
    bool hadException() const { return hadException_; }
};

// JNI字符串转换工具
class JNIString {
private:
    JNIEnv* env_;
    jstring jstr_;
    const char* cstr_;
    jboolean isCopy_;
    
public:
    explicit JNIString(JNIEnv* env, jstring jstr) 
        : env_(env), jstr_(jstr), cstr_(nullptr), isCopy_(false) {
        if (jstr_) {
            cstr_ = env_->GetStringUTFChars(jstr_, &isCopy_);
        }
    }
    
    ~JNIString() {
        if (cstr_ && isCopy_) {
            env_->ReleaseStringUTFChars(jstr_, cstr_);
        }
    }
    
    const char* get() const { return cstr_; }
    bool isValid() const { return cstr_ != nullptr; }
};

// JNI数组转换工具
template<typename T>
class JNIArray {
private:
    JNIEnv* env_;
    jarray array_;
    T* elements_;
    jsize length_;
    jboolean isCopy_;
    
public:
    explicit JNIArray(JNIEnv* env, jarray array) 
        : env_(env), array_(array), elements_(nullptr), length_(0), isCopy_(false) {
        if (array_) {
            length_ = env_->GetArrayLength(array_);
            if constexpr (std::is_same_v<T, jint>) {
                elements_ = static_cast<T*>(env_->GetIntArrayElements(static_cast<jintArray>(array_), &isCopy_));
            } else if constexpr (std::is_same_v<T, jfloat>) {
                elements_ = static_cast<T*>(env_->GetFloatArrayElements(static_cast<jfloatArray>(array_), &isCopy_));
            } else if constexpr (std::is_same_v<T, jbyte>) {
                elements_ = static_cast<T*>(env_->GetByteArrayElements(static_cast<jbyteArray>(array_), &isCopy_));
            }
        }
    }
    
    ~JNIArray() {
        if (elements_ && isCopy_) {
            if constexpr (std::is_same_v<T, jint>) {
                env_->ReleaseIntArrayElements(static_cast<jintArray>(array_), elements_, 0);
            } else if constexpr (std::is_same_v<T, jfloat>) {
                env_->ReleaseFloatArrayElements(static_cast<jfloatArray>(array_), elements_, 0);
            } else if constexpr (std::is_same_v<T, jbyte>) {
                env_->ReleaseByteArrayElements(static_cast<jbyteArray>(array_), elements_, 0);
            }
        }
    }
    
    T* get() const { return elements_; }
    jsize length() const { return length_; }
    bool isValid() const { return elements_ != nullptr; }
};

// 全局JVM引用管理
namespace lattice {
namespace jni {

class JVM {
private:
    static JavaVM* jvm_;
    static std::atomic<int> refCount_;
    
public:
    static void setJVM(JavaVM* jvm) {
        jvm_ = jvm;
        refCount_.fetch_add(1, std::memory_order_relaxed);
    }
    
    static void detachJVM() {
        if (refCount_.fetch_sub(1, std::memory_order_relaxed) <= 1) {
            // 最后的引用，清理JVM
            jvm_ = nullptr;
        }
    }
    
    static JavaVM* get() { return jvm_; }
    
    static JNIEnv* getEnv() {
        if (!jvm_) return nullptr;
        
        JNIEnv* env;
        int status = jvm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
        
        if (status == JNI_EDETACHED) {
            // 线程未附加，需要附加
            status = jvm_->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
            if (status == 0) {
                // 成功附加，可以在这里跟踪.detachLater()需求
            }
        }
        
        return (status == 0) ? env : nullptr;
    }
};

// 初始化宏
#define JNI_BEGIN() \
    JNIExceptionGuard exceptionGuard(env); \
    if (exceptionGuard.hadException()) return

#define JNI_CHECK_NULL(ptr, msg) \
    if (!(ptr)) { \
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), msg); \
        return; \
    }

#define JNI_CHECK_RANGE(index, size) \
    if ((index) < 0 || (index) >= (size)) { \
        env->ThrowNew(env->FindClass("java/lang/IndexOutOfBoundsException"), "Index out of range"); \
        return; \
    }

#define JNI_RETURN_VOID() return

#define JNI_RETURN_INT(value) return (value)

#define JNI_RETURN_LONG(value) return (value)

#define JNI_RETURN_FLOAT(value) return (value)

#define JNI_RETURN_DOUBLE(value) return (value)

#define JNI_RETURN_BOOLEAN(value) return (value)

#define JNI_RETURN_OBJECT(object) return (object)

#define JNI_RETURN_STRING(str) \
    return env->NewStringUTF((str).c_str())

#define JNI_RETURN_INT_ARRAY(arr, size) \
    jintArray result = env->NewIntArray(size); \
    if (result) { \
        env->SetIntArrayRegion(result, 0, size, (jint*)(arr)); \
    } \
    return result

#define JNI_RETURN_FLOAT_ARRAY(arr, size) \
    jfloatArray result = env->NewFloatArray(size); \
    if (result) { \
        env->SetFloatArrayRegion(result, 0, size, (jfloat*)(arr)); \
    } \
    return result

#define JNI_RETURN_BYTE_ARRAY(arr, size) \
    jbyteArray result = env->NewByteArray(size); \
    if (result) { \
        env->SetByteArrayRegion(result, 0, size, (jbyte*)(arr)); \
    } \
    return result

} // namespace jni
} // namespace lattice

// ========================================
// Additional JNI helper functions for the new systems
// ========================================

namespace jni {

// Simple string conversion helpers
inline std::string jstring_to_string(JNIEnv* env, jstring jstr) {
    if (!jstr) return "";
    const char* cstr = env->GetStringUTFChars(jstr, nullptr);
    std::string str(cstr);
    env->ReleaseStringUTFChars(jstr, cstr);
    return str;
}

inline jstring string_to_jstring(JNIEnv* env, const std::string& str) {
    return env->NewStringUTF(str.c_str());
}

inline std::string jbytearray_to_string(JNIEnv* env, jbyteArray arr) {
    if (!arr) return "";
    jsize len = env->GetArrayLength(arr);
    std::vector<jbyte> buffer(len);
    env->GetByteArrayRegion(arr, 0, len, buffer.data());
    return std::string(reinterpret_cast<char*>(buffer.data()), len);
}

inline jbyteArray string_to_jbytearray(JNIEnv* env, const std::string& str) {
    jbyteArray arr = env->NewByteArray(static_cast<jsize>(str.size()));
    if (arr && !str.empty()) {
        env->SetByteArrayRegion(arr, 0, static_cast<jsize>(str.size()), 
                               reinterpret_cast<const jbyte*>(str.data()));
    }
    return arr;
}

// Exception handling
inline void throw_java_exception(JNIEnv* env, const char* className, const char* message) {
    jclass cls = env->FindClass(className);
    if (cls) {
        env->ThrowNew(cls, message);
        env->DeleteLocalRef(cls);
    }
}

// Object creation helpers
inline jobject create_cache_stats(JNIEnv* env, int totalRequests, int l1Hits, int l2Hits, 
                                 int l3Hits, int misses, int puts, int evictions) {
    jclass cacheStatsClass = env->FindClass("io/lattice/cache/CacheStats");
    if (!cacheStatsClass) return nullptr;
    
    jmethodID constructor = env->GetMethodID(cacheStatsClass, "<init>", "(IIIIIII)V");
    if (!constructor) return nullptr;
    
    jobject statsObj = env->NewObject(cacheStatsClass, constructor,
        totalRequests, l1Hits, l2Hits, l3Hits, misses, puts, evictions);
    
    env->DeleteLocalRef(cacheStatsClass);
    return statsObj;
}

} // namespace jni
