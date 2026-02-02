#pragma once

#define HEADER_TYPE_MASK 0xF0 // 1111 0000
#define HEADER_CODE_MASK 0x0F // 0000 1111

// ESP_NOW_MAX_TOTAL_PEER_NUM - 20

enum PacketType
{
    INVALID,
    CONNECT,
    CONNACK,
    PUBLISH,
};

struct packet_header
{
    uint8_t type; // type | flags
    uint8_t len;  // ESP_NOW_MAX_DATA_LEN = 250
} __attribute__((packed));

enum ConnackResponseCode
{
    INVALID,
    SUCCESS,
    FAILURE,
};

uint8_t inline encode_header(uint8_t type, uint8_t code)
{
    return ((type & 0x0F) << 4) | (code & 0x0F);
}

uint8_t inline get_type(uint8_t msg)
{
    return (msg & HEADER_TYPE_MASK) >> 4;
}

uint8_t inline get_code(uint8_t msg)
{
    return msg & HEADER_CODE_MASK;
}