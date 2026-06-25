//
// Created by dunamis on 02/03/2026.
//

#ifndef SMARTDRIVE_CRC8_H
#define SMARTDRIVE_CRC8_H

#include "opx/shared/types/ProtocolTypes.h"

namespace CRC8 {
inline uint8_t compute(const RawData &rawData) {
  uint8_t crc = 0x00;

  for (size_t i = 0; i < rawData.size; ++i) {
    crc ^= rawData.data[i];

    for (uint8_t bit = 0; bit < 8; ++bit) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x07; // CRC8-CCITT polynomial
      } else {
        crc <<= 1;
      }
    }
  }
  return crc;
}
} // namespace CRC8

#endif // SMARTDRIVE_CRC8_H