#ifndef __CRC_H
#define __CRC_H

#include <stdint.h>

/* CRC16-CCITT (poly 0x1021, init 由调用者给定, 通常 0xFFFF) */
uint16_t CRC16_Update(uint16_t crc, const uint8_t *data, uint32_t len);
/* CRC32 IEEE 802.3 (poly 0x04C11DB7 反射, init 0xFFFFFFFF, 结果再取反) */
uint32_t CRC32_Update(uint32_t crc, const uint8_t *data, uint32_t len);

#endif
