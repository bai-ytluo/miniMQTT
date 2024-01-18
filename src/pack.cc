#include <cstring>   // For std::memcpy and std::strlen
#include <cstdint>   // For uint8_t, uint16_t, uint32_t
#include <winsock2.h> // For ntohs, ntohl, htons, htonl
#include "pack.h"

namespace pack {
    // Reading data
    uint8_t unpack_u8(uint8_t **buf) {
        uint8_t val = **buf;
        ++(*buf);
        return val;
    }

    uint16_t unpack_u16(uint8_t **buf) {
        uint16_t val;
        std::memcpy(&val, *buf, sizeof(uint16_t));
        (*buf) += sizeof(uint16_t);
        return ntohs(val);
    }

    uint32_t unpack_u32(uint8_t **buf) {
        uint32_t val;
        std::memcpy(&val, *buf, sizeof(uint32_t));
        (*buf) += sizeof(uint32_t);
        return ntohl(val);
    }

    uint8_t *unpack_bytes(uint8_t **buf, size_t len, uint8_t *str) {
        std::memcpy(str, *buf, len);
        str[len] = '\0';
        (*buf) += len;
        return str;
    }

    uint16_t unpack_string16(uint8_t **buf, uint8_t **dest) {
        uint16_t len = unpack_u16(buf);
        *dest = new uint8_t[len + 1];
        unpack_bytes(buf, len, *dest);
        return len;
    }

    // Writing data
    void pack_u8(uint8_t **buf, uint8_t val) {
        **buf = val;
        (*buf) += sizeof(uint8_t);
    }

    void pack_u16(uint8_t **buf, uint16_t val) {
        uint16_t htonsval = htons(val);
        std::memcpy(*buf, &htonsval, sizeof(uint16_t));
        (*buf) += sizeof(uint16_t);
    }

    void pack_u32(uint8_t **buf, uint32_t val) {
        uint32_t htonlval = htonl(val);
        std::memcpy(*buf, &htonlval, sizeof(uint32_t));
        (*buf) += sizeof(uint32_t);
    }

    void pack_bytes(uint8_t **buf, uint8_t *str) {
        size_t len = std::strlen(reinterpret_cast<char *>(str));
        std::memcpy(*buf, str, len);
        (*buf) += len;
    }

} 