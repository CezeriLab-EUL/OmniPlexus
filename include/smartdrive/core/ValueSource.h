//
// Created by dunamis on 26/01/2026.
//

#ifndef SMARTDRIVE_VALUESOURCE_H
#define SMARTDRIVE_VALUESOURCE_H

#include <cstdint>
#include <cstring>
#include <type_traits>
#include "../utils/Logger.h"

enum class ValueType : uint16_t {
    EMPTY = 0,
    INT32 = 1,
    UINT16 = 2,
    FLOAT = 3,
    STRING = 4
};

constexpr const char *typeToString(const ValueType t) {
    switch (t) {
        case ValueType::INT32: return "int32";
        case ValueType::UINT16: return "uint16";
        case ValueType::FLOAT: return "float";
        case ValueType::STRING: return "string";
        default: return "empty";
    }
}

#pragma pack(push, 1)
class ValueSource {
protected:
    ValueType type = ValueType::EMPTY;
    uint8_t data[16]{};

public:
    ValueSource() = default;

    //Numerical pack
    template<typename T>
    void pack(T value) {
        static_assert(std::is_arithmetic_v<T>, "Value must be arithmetic. Use string overload for non-numeric types");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        static_assert(std::is_same_v<T, int32_t> || std::is_same_v<T, uint16_t> || std::is_same_v<T, float>,
                      "Invalid type");
        static_assert(sizeof(T) <= 16, "Value too big for 16-byte buffer");

        if constexpr (std::is_same_v<T, int32_t>) type = ValueType::INT32;
        else if constexpr (std::is_same_v<T, uint16_t>) type = ValueType::UINT16;
        else if constexpr (std::is_same_v<T, float>) type = ValueType::FLOAT;

        std::memcpy(data, &value, sizeof(T));
    }

    //String pack
    void packString(const char *src) {
        if (!src) {
            clear();
            return;
        }
        type = ValueType::STRING;
        size_t len = std::strlen(src);
        if (len >= 16) {
            LOG(LogLevel::WARNING, "String truncated to 15 chars + null");
            len = 15;
        }
        std::memcpy(data, src, len);
        data[len] = '\0';
    }

    template<typename T>
    T unpack() const {
        static_assert(std::is_arithmetic_v<T>, "Use unpackString() for strings");

        if constexpr (std::is_same_v<T, float>) {
            if (type != ValueType::FLOAT) return 0.0f;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            if (type != ValueType::INT32) return 0;
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            if (type != ValueType::UINT16) return 0;
        }

        T result;
        std::memcpy(&result, data, sizeof(T));
        return result;
    }

    const char *unpackString() const { return reinterpret_cast<const char *>(data); }

    void clear() {
        type = ValueType::EMPTY;
        std::memset(data, 0, sizeof(data));
    }

    ValueType getType() const { return type; }
};
#pragma pack(pop)

static_assert(sizeof(ValueSource) == 18, "ValueSource must be exactly 18 bytes");
#endif //SMARTDRIVE_VALUESOURCE_H
