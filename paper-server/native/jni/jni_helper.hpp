#pragma once

#include <jni.h>
#include <vector>
#include <string>
#include <stdexcept>

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
