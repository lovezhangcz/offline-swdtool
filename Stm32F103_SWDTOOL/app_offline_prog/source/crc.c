/**
  * @file  crc.c
  * @brief CRC16-CCITT (通信帧校验) 与 CRC32-IEEE (固件镜像校验)
  */
#include "crc.h"

uint16_t CRC16_Update(uint16_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint8_t  b;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (i = 0; i < 8; i++) {
            b = (uint8_t)(crc >> 15);
            crc <<= 1;
            if (b) crc ^= 0x1021u;
        }
    }
    return crc;
}

uint32_t CRC32_Update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    while (len--) {
        crc ^= *data++;
        for (i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
    }
    return crc;
}
