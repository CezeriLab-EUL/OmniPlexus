//
// Created by dunamis on 26/01/2026.
//

#ifndef SMARTDRIVE_VALUESOURCE_H
#define SMARTDRIVE_VALUESOURCE_H

#include "../core/platform.h"
#ifndef __AVR__
#include <type_traits>
#endif
#include "opx/shared/utils/Logger.h"
#include "string.hpp"

enum class ValueType : uint8_t {
  EMPTY = 0x0,
  UINT8 = 0X1,
  INT8 = 0x2,
  FLOAT = 0x3,
  UINT16 = 0x4,
  INT16 = 0x5,
  UINT32 = 0x6,
  INT32 = 0x7,
  STRING = 0x8
};

inline const char *typeToString(const ValueType t) {
  switch (t) {
  case ValueType::INT32:
    return "int32";
  case ValueType::UINT16:
    return "uint16";
  case ValueType::FLOAT:
    return "float";
  case ValueType::STRING:
    return "string";
  default:
    return "empty";
  }
}

template <class T_> struct TypeTraits;

template <> struct TypeTraits<uint8_t> {
  static constexpr ValueType type = ValueType::UINT8;
  static constexpr int size = 1;
};

template <> struct TypeTraits<int8_t> {
  static constexpr ValueType type = ValueType::INT8;
  static constexpr int size = 1;
};

template <> struct TypeTraits<float> {
  static constexpr ValueType type = ValueType::FLOAT;
  static constexpr int size = 4;
};

template <> struct TypeTraits<uint16_t> {
  static constexpr ValueType type = ValueType::UINT16;
  static constexpr int size = 2;
};

template <> struct TypeTraits<int16_t> {
  static constexpr ValueType type = ValueType::INT16;
  static constexpr int size = 2;
};

template <> struct TypeTraits<uint32_t> {
  static constexpr ValueType type = ValueType::UINT32;
  static constexpr int size = 4;
};

template <> struct TypeTraits<int32_t> {
  static constexpr ValueType type = ValueType::INT32;
  static constexpr int size = 4;
};

#pragma pack(push, 1)
class ValueSource {
protected:
  uint8_t typeAndSize = 0x00;
  uint8_t data[16]{};

public:
  ValueSource() = default;

  ValueSource(uint8_t value) { pack(value); }

  ValueSource(int8_t value) { pack(value); }

  ValueSource(uint16_t value) { pack(value); }

  ValueSource(int16_t value) { pack(value); }

  ValueSource(uint32_t value) { pack(value); }

  ValueSource(int32_t value) { pack(value); }

  ValueSource(float value) { pack(value); }

  ValueSource(const string_view &value) {
    packString(value.data(), value.size());
  }

  ValueSource(const char *value) {
    packString(value, value ? strlen(value) : 0);
  }

  ValueSource &operator=(uint8_t value) {
    pack(value);
    return *this;
  }

  ValueSource &operator=(int8_t value) {
    pack(value);
    return *this;
  }

  ValueSource &operator=(uint16_t value) {
    pack(value);
    return *this;
  }

  ValueSource &operator=(int16_t value) {
    pack(value);
    return *this;
  }

  ValueSource &operator=(uint32_t value) {
    pack(value);
    return *this;
  }

  ValueSource &operator=(int32_t value) {
    pack(value);
    return *this;
  }

  ValueSource &operator=(float value) {
    pack(value);
    return *this;
  }

  ValueSource &operator=(const char *value) {
    packString(value, value ? strlen(value) : 0);
    return *this;
  }

  ValueSource &operator=(const string_view &value) {
    packString(value.data(), value.size());
    return *this;
  }

  operator uint8_t() const { return unpack<uint8_t>(); }

  operator int8_t() const { return unpack<int8_t>(); }

  operator uint16_t() const { return unpack<uint16_t>(); }

  operator int16_t() const { return unpack<int16_t>(); }

  operator uint32_t() const { return unpack<uint32_t>(); }

  operator int32_t() const { return unpack<int32_t>(); }

  operator float() const { return unpack<float>(); }

  operator const char *() const { return unpackString().data(); }

  operator string_view() const {
    return string_view(reinterpret_cast<const char *>(data), getDataSize());
  }

  void setTypeAndSize(ValueType type, uint8_t sizeValue) {
    typeAndSize = (static_cast<uint8_t>(type) << 4) | (sizeValue & 0x0F);
  }

  size_t getDataSize() const {
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
      return size + 1; // Adding 1 for null terminator when reading
    default:
      return 0;
    }
  }

  uint8_t getTypeAndSize() const { return typeAndSize; }
  const uint8_t *getData() const { return data; }

  void setTypeAndSizeRaw(const uint8_t value) { typeAndSize = value; }
  uint8_t *getDataMutable() { return data; }

  // Numerical pack
  template <typename T> void pack(T value) {
#ifndef __AVR__
    static_assert(std::is_arithmetic<T>::value,
                  "Value must be arithmetic. Use packString() for strings");
    static_assert(std::is_trivially_copyable<T>::value,
                  "T must be trivially copyable");
    static_assert(sizeof(T) <= 16, "Value too big for 16-byte buffer");
#endif
    setTypeAndSize(TypeTraits<T>::type, 0);
    memcpy(data, &value, TypeTraits<T>::size);
  }

  // String pack
  void packString(const char *src, size_t len) {
    if (!src) {
      clear();
      return;
    }

    if (len >= 16) {
      LOG(LogLevel::OP_WARNING, "String truncated to 15 chars + null");
      len = 15;
    }

    setTypeAndSize(ValueType::STRING, static_cast<uint8_t>(len));
    memcpy(data, src, len);
    data[len] = '\0';
  }

  template <typename T> T unpack() const {
#ifndef __AVR__
    static_assert(std::is_arithmetic<T>::value,
                  "Use unpackString() for strings");
#endif
    T result;
    if (getType() != TypeTraits<T>::type) {
      return T{};
    }
    memcpy(&result, data, TypeTraits<T>::size);
    return result;
  }

  string_view unpackString() const {
    return string_view(reinterpret_cast<const char *>(data), getDataSize());
  }

  void clear() {
    typeAndSize = 0x00;
    memset(data, 0, sizeof(data));
  }

  void initDefault(ValueType type) {
    memset(data, 0, sizeof(data));
    switch (type) {
    case ValueType::UINT8:
      setTypeAndSize(ValueType::UINT8, 0);
      break;
    case ValueType::INT8:
      setTypeAndSize(ValueType::INT8, 0);
      break;
    case ValueType::UINT16:
      setTypeAndSize(ValueType::UINT16, 0);
      break;
    case ValueType::INT16:
      setTypeAndSize(ValueType::INT16, 0);
      break;
    case ValueType::UINT32:
      setTypeAndSize(ValueType::UINT32, 0);
      break;
    case ValueType::INT32:
      setTypeAndSize(ValueType::INT32, 0);
      break;
    case ValueType::FLOAT:
      setTypeAndSize(ValueType::FLOAT, 0);
      break;
    case ValueType::STRING:
      setTypeAndSize(ValueType::STRING, 0);
      break;
    default:
      clear();
      break;
    }
  }

  bool isEmpty() const { return getType() == ValueType::EMPTY; }

  ValueType getType() const {
    return static_cast<ValueType>((typeAndSize >> 4) & 0x0F);
  }
};
#pragma pack(pop)

static_assert(sizeof(ValueSource) == 17,
              "ValueSource must be exactly 17 bytes");
#endif // SMARTDRIVE_VALUESOURCE_H
