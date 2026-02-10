//
// Created by dunamis on 26/01/2026.
//

#ifndef SMARTDRIVE_VALUESOURCE_H
#define SMARTDRIVE_VALUESOURCE_H

#include <cstdint>
#include <cstring>
#include <type_traits>
#include "../utils/Logger.h"

enum class ValueType : uint8_t {
    EMPTY = 0x0,
    INT8 = 0x1,
    INT16 = 0x2,
    INT32 = 0x3,
    UINT8 = 0X4,
    UINT16 = 0x5,
    UINT32 = 0x6,
    FLOAT = 0x7,
    STRING = 0x8
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
    uint8_t typeAndSize = 0x00;
    uint8_t data[16]{};

public:
    ValueSource() = default;

    constexpr void setTypeAndSize(ValueType type, uint8_t sizeValue) {
        typeAndSize = (static_cast<uint8_t>(type) << 4) | (sizeValue & 0x0F);
    }

    constexpr size_t getDataSize() const {
        const uint8_t type = (typeAndSize >> 4) & 0x0F;
        const uint8_t size = typeAndSize & 0x0F;

        switch (type) {
            case static_cast<uint8_t>(ValueType::EMPTY):
                return 0;
            case static_cast<uint8_t>(ValueType::INT8):
            case static_cast<uint8_t>(ValueType::UINT8):
                return 1 * (size + 1);
            case static_cast<uint8_t>(ValueType::INT16):
            case static_cast<uint8_t>(ValueType::UINT16):
                return 2 * (size + 1);
            case static_cast<uint8_t>(ValueType::INT32):
            case static_cast<uint8_t>(ValueType::UINT32):
            case static_cast<uint8_t>(ValueType::FLOAT):
                return 4 * (size + 1);
            case static_cast<uint8_t>(ValueType::STRING):
                return size; //string should be the exact length
            default:
                return 0;
        }
    }

    constexpr uint8_t getTypeAndSize() const { return typeAndSize; }
    constexpr const uint8_t *getData() const { return data; }

    void setTypeAndSizeRaw(const uint8_t value) { typeAndSize = value; }
    uint8_t *getDataMutable() { return data; }

    //Numerical pack
    template<typename T>
    void pack(T value) {
        static_assert(std::is_arithmetic_v<T>, "Value must be arithmetic. Use packString() for strings");
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        static_assert(sizeof(T) <= 16, "Value too big for 16-byte buffer");

        ValueType vtype;
        if constexpr (std::is_same_v<T, uint8_t>) vtype = ValueType::UINT8;
        else if constexpr (std::is_same_v<T, int8_t>) vtype = ValueType::INT8;
        else if constexpr (std::is_same_v<T, float>) vtype = ValueType::FLOAT;
        else if constexpr (std::is_same_v<T, uint16_t>) vtype = ValueType::UINT16;
        else if constexpr (std::is_same_v<T, int16_t>) vtype = ValueType::INT16;
        else if constexpr (std::is_same_v<T, uint32_t>) vtype = ValueType::UINT32;
        else if constexpr (std::is_same_v<T, int32_t>) vtype = ValueType::INT32;
        else return;

        setTypeAndSize(vtype, 0); //0 means single value.
        std::memcpy(data, &value, sizeof(T));
    }

    //String pack
    void packString(const char *src) {
        if (!src) {
            clear();
            return;
        }

        std::size_t len = std::strlen(src);
        if (len >= 16) {
            LOG(LogLevel::WARNING, "String truncated to 15 chars + null");
            len = 15;
        }

        setTypeAndSize(ValueType::STRING, static_cast<uint8_t>(len+1)); //Added one for the null character
        std::memcpy(data, src, len);
        data[len] = '\0';
    }

    template<typename T>
    T unpack() const {
        static_assert(std::is_arithmetic_v<T>, "Use unpackString() for strings");

        const uint8_t typeField = (typeAndSize >> 4) & 0x0F;

        if constexpr (std::is_same_v<T, uint8_t>) {
            if (typeField != static_cast<uint8_t>(ValueType::UINT8)) return 0;
        } else if constexpr (std::is_same_v<T, int8_t>) {
            if (typeField != static_cast<uint8_t>(ValueType::INT8)) return 0;
        }else if constexpr (std::is_same_v<T, float>) {
            if (typeField != static_cast<uint8_t>(ValueType::FLOAT)) return 0.0f;
        } else if constexpr (std::is_same_v<T, uint16_t>) {
            if (typeField != static_cast<uint8_t>(ValueType::UINT16)) return 0;
        } else if constexpr (std::is_same_v<T, int16_t>) {
            if (typeField != static_cast<uint8_t>(ValueType::INT16)) return 0;
        }else if constexpr (std::is_same_v<T, uint32_t>) {
            if (typeField != static_cast<uint8_t>(ValueType::UINT32)) return 0;
        }else if constexpr (std::is_same_v<T, int32_t>) {
            if (typeField != static_cast<uint8_t>(ValueType::INT32)) return 0;
        }

        T result;
        std::memcpy(&result, data, sizeof(T));
        return result;
    }

    const char *unpackString() const { return reinterpret_cast<const char *>(data); }

    void clear() {
        typeAndSize = 0x00;
        std::memset(data, 0, sizeof(data));
    }

    ValueType getType() const {
        return static_cast<ValueType>((typeAndSize >> 4) & 0x0F);
    }
};
#pragma pack(pop)

static_assert(sizeof(ValueSource) == 17, "ValueSource must be exactly 17 bytes");
#endif //SMARTDRIVE_VALUESOURCE_H
