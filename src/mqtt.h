#ifndef MQTT_H
#define MQTT_H

#include <variant> 
#include <cstdint>   // For uint8_t, uint16_t, uint32_t


namespace mqtt{
    
    class Tester { 
    private: 
        int testval; 
    public:
        Tester(): testval(0) {} 
        ~Tester() {} 
        int get_val() { return testval; } 
        void set_val(int val) { testval = val; }
    }; 
    int testing( Tester a, Tester b ); 

    constexpr int MQTT_HEADER_LEN = 2; 
    constexpr int MQTT_ACK_LEN = 4; 

    // Stub bytes, useful for generic replies, these represent the first byte in the fixed header
    constexpr uint8_t CONNACK_BYTE = 0x20;
    constexpr uint8_t PUBLISH_BYTE = 0x30;
    constexpr uint8_t PUBACK_BYTE = 0x40;
    constexpr uint8_t PUBREC_BYTE = 0x50;
    constexpr uint8_t PUBREL_BYTE = 0x60;
    constexpr uint8_t PUBCOMP_BYTE = 0x70;
    constexpr uint8_t SUBACK_BYTE = 0x90;
    constexpr uint8_t UNSUBACK_BYTE = 0xB0;
    constexpr uint8_t PINGRESP_BYTE = 0xD0;

    // Message types 
    enum class packet_type {
        CONNECT = 1,
        CONNACK = 2,
        PUBLISH = 3,
        PUBACK = 4,
        PUBREC = 5,
        PUBREL = 6,
        PUBCOMP = 7,
        SUBSCRIBE = 8,
        SUBACK = 9,
        UNSUBSCRIBE = 10,
        UNSUBACK = 11,
        PINGREQ = 12,
        PINGRESP = 13,
        DISCONNECT = 14
    }; 

    enum class qos_level { AT_MOST_ONCE, AT_LEAST_ONCE, EXACTLY_ONCE };

    class Header { 
    public:
        Header() {} 
        ~Header() {} 
        union
        {
            uint8_t byte;
            struct
            {
                unsigned retain : 1;
                unsigned qos : 2;
                unsigned dup : 1;
                unsigned type : 4;
            } bits;
        };
    }; 

    class Connect {
    public: 
        Connect() {}
        ~Connect() {}
        Header header; 
        union {
            uint8_t bytes; 
            struct {
                unsigned int reserved: 1; 
                unsigned int clean_session : 1;
                unsigned int will : 1;
                unsigned int will_qos : 2;
                unsigned int will_retain : 1;
                unsigned int password : 1;
                unsigned int username : 1;
            } bits; 
        } ; 
        struct {
            unsigned short keepalive;
            uint8_t *client_id;
            uint8_t *username;
            uint8_t *password;
            uint8_t *will_topic;
            uint8_t *will_message;
        } payload; 
    }; 

    class Connack {
    public: 
        Connack() {}
        ~Connack() {}
        Header header;
        union {
            uint8_t byte;
            struct {
                unsigned session_present : 1;
                unsigned reserved : 7;
            } bits;
        };
        uint8_t rc;
    }; 

    class Subscribe {
    public: 
        Subscribe() : pkt_id(0), tuples_len(0), tuples(nullptr) {}
        ~Subscribe() {
            delete[] tuples; 
        } 
        Header header; 

        unsigned short pkt_id; 
        unsigned short tuples_len; 
        struct Tuple {
            unsigned short topic_len;
            uint8_t *topic;
            unsigned qos;
        } *tuples; 
    }; 

    class Unsubscribe {
    public: 
        Unsubscribe() : pkt_id(0), tuples_len(0), tuples(nullptr) {}
        ~Unsubscribe() {
            delete[] tuples; 
        } 
        Header header; 

        unsigned short pkt_id; 
        unsigned short tuples_len; 
        struct Tuple {
            unsigned short topic_len;
            uint8_t *topic; 
        } *tuples; 
    }; 

    class Suback {
    public: 
        Suback() : pkt_id(0), rcs_len(0), rcs(nullptr) {}
        ~Suback() {
            delete[] rcs; 
        } 
        Header header; 

        unsigned short pkt_id; 
        unsigned short rcs_len; 
        uint8_t *rcs; 
    }; 

    class Publish {
    public: 
        Publish() : pkt_id(0), topic_len(0), topic(nullptr), payload_len(0), payload(nullptr) {}
        ~Publish() {
            delete[] topic; 
            delete[] payload; 
        } 
        Header header; 

        unsigned short pkt_id; 
        unsigned short topic_len; 
        uint8_t *topic; 
        unsigned short payload_len; 
        uint8_t *payload; 
    }; 

    class Ack {
    public: 
        Ack() : pkt_id(0) {}
        ~Ack() {} 
        Header header; 

        unsigned short pkt_id; 
    }; 

    using Puback = Ack;
    using Pubrec = Ack;
    using Pubrel = Ack;
    using Pubcomp = Ack;
    using Unsuback = Ack;

    using Pingreq = Header;
    using Pingresp = Header;
    using Disconnect = Header; 

    class Packet {
    public:
        Packet() {} 
        ~Packet() {}
        std::variant<Ack, Header, Connect, Connack, Suback, Publish, Subscribe, Unsubscribe> content; 
    }; 

    int encodeLength(uint8_t *, size_t);
    unsigned long long decodeLength(uint8_t **);
    int unpackPacket(const uint8_t *, Packet &); 
    Header *packetHeader(uint8_t);
    Ack *packetAck(uint8_t, unsigned short);
    Connack *packetConnack(uint8_t, uint8_t, uint8_t);
    Suback *packetSuback(uint8_t, unsigned short, uint8_t *, unsigned short);
    Publish *packetPublish(uint8_t, unsigned short, size_t, uint8_t *, size_t, uint8_t *);
    void packetRelease(Packet &, unsigned); 
    uint8_t *packPacket(const Packet &, unsigned); 
    
} 

#endif
