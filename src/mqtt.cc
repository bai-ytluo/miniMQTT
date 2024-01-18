#include <vector>
#include <functional>
#include <array>
#include <cstring>   // For std::memcpy and std::strlen

#include "mqtt.h" 
#include "pack.h" 

namespace mqtt {

    int testing( Tester a, Tester b ) {
        int val_a = a.get_val(); 
        int val_b = b.get_val(); 
        return val_a + val_b; 
    }
    
    namespace {
        //////////////////////////////
        // MQTT unpacking functions //
        //////////////////////////////
        size_t unpackConnect(uint8_t *buf, Header *hdr, Packet *pkt){
            Connect connect; connect.header = *hdr; 
            pkt->content = connect; 
            Connect& connectRef = std::get<Connect>(pkt->content); 
            uint8_t *init = buf; 
            // Second byte of the fixed header, contains the length of 
            // remaining byte of the connect packet
            size_t len = decodeLength(&buf); 
            // For now we ignore checks on protocol name and reserved bits, just skip to the 8th byte
            buf = init + 8; 
            // Read variable header byte flags 
            connectRef.bytes = pack::unpack_u8(&buf); 
            // Read keepalive MSB and LSB (2 bytes word) 
            connectRef.payload.keepalive = pack::unpack_u16(&buf); 
            // Read CID length (2 bytes word) 
            uint16_t cid_len = pack::unpack_u16(&buf); 
            // Read the client id 
            if (cid_len > 0) {
                connectRef.payload.client_id = new uint8_t[cid_len + 1]; 
                pack::unpack_bytes(&buf, cid_len, connectRef.payload.client_id); 
            }
            // Read the will topic and message if will is set on flags 
            if (connectRef.bits.will == 1) {
                pack::unpack_string16(&buf, &connectRef.payload.will_topic); 
                pack::unpack_string16(&buf, &connectRef.payload.will_message); 
            } 
            // Read the username if username flag is set 
            if (connectRef.bits.username == 1) {
                pack::unpack_string16(&buf, &connectRef.payload.username); 
            } 
            // Read the password if password flag is set 
            if (connectRef.bits.password == 1) {
                pack::unpack_string16(&buf, &connectRef.payload.password); 
            }
            return len; 
        }

        size_t unpackPublish(uint8_t *buf, Header *hdr, Packet *pkt) {
            Publish publish; publish.header = *hdr;
            pkt->content = publish; 
            Publish& publishRef = std::get<Publish>(pkt->content); 
            // Second byte of the fixed header, contains the length of 
            // remaining bytes of the connect packet
            size_t len = decodeLength(&buf); 
            // Read topic length and topic of the soon-to-be-published message 
            uint16_t topic_len = pack::unpack_string16(&buf, &publishRef.topic); 
            uint16_t message_len = len; 
            // Read packet id 
            if (publish.header.bits.qos > static_cast<unsigned int>(qos_level::AT_MOST_ONCE) ) { 
                publishRef.pkt_id = pack::unpack_u16(&buf); 
                message_len -= sizeof(uint16_t);
            } 
            // Message len is calculated subtracting the length of 
            // the variable header from the Remaining Length field that is in the Fixed Header
            message_len -= (sizeof(uint16_t) + topic_len);
            publishRef.payload_len = message_len; 
            publishRef.payload = new uint8_t[message_len + 1];
            pack::unpack_bytes(&buf, message_len, publishRef.payload); 
            return len;
        } 

        size_t unpackSubscribe(uint8_t *buf, Header *hdr, Packet *pkt) {
            Subscribe subscribe; subscribe.header = *hdr; 
            // Second byte of the fixed header, contains the length of 
            // remaining bytes of the connect packet
            size_t len = decodeLength(&buf);
            size_t remaining_bytes = len; 
            // Read packet id 
            subscribe.pkt_id = pack::unpack_u16(&buf);
            remaining_bytes -= sizeof(uint16_t); 
            // Read in a loop all remaining bytes specified by len of the Fixed Header.
            // From now on the payload consists of 3-tuples formed by:
            //  - topic length
            //  - topic filter (string)
            //  - qos
            std::vector<Subscribe::Tuple> tempTuples;
            while (remaining_bytes > 0) {
                Subscribe::Tuple tuple; 
                // Read length bytes of the first topic filter 
                tuple.topic_len = pack::unpack_string16(&buf, &tuple.topic);
                remaining_bytes -= (sizeof(uint16_t) + tuple.topic_len); 
                tuple.qos = pack::unpack_u8(&buf);
                tempTuples.push_back(tuple);
                len -= sizeof(uint8_t);
            }
            subscribe.tuples_len = tempTuples.size();
            subscribe.tuples = new Subscribe::Tuple[subscribe.tuples_len]; 
            std::copy(tempTuples.begin(), tempTuples.end(), subscribe.tuples); 
            pkt->content = subscribe; 
            return len;
        } 
        
        size_t unpackUnsubscribe(uint8_t *buf, Header *hdr, Packet *pkt) {
            Unsubscribe unsubscribe; unsubscribe.header = *hdr; 
            // Second byte of the fixed header, contains the length of 
            // remaining bytes of the connect packet
            size_t len = decodeLength(&buf);
            size_t remaining_bytes = len; 
            // Read packet id 
            unsubscribe.pkt_id = pack::unpack_u16(&buf);
            remaining_bytes -= sizeof(uint16_t); 
            // Read in a loop all remaining bytes specified by len of the Fixed Header.
            // From now on the payload consists of 2-tuples formed by:
            //  - topic length
            //  - topic filter (string) 
            std::vector<Unsubscribe::Tuple> tempTuples; 
            while (remaining_bytes > 0) {
                Unsubscribe::Tuple tuple; 
                // Read length bytes of the first topic filter 
                tuple.topic_len = pack::unpack_string16(&buf, &tuple.topic); 
                remaining_bytes -= (sizeof(uint16_t) + tuple.topic_len); 
                tempTuples.push_back(tuple); 
            }
            unsubscribe.tuples_len = tempTuples.size(); 
            unsubscribe.tuples = new Unsubscribe::Tuple[unsubscribe.tuples_len];
            std::copy(tempTuples.begin(), tempTuples.end(), unsubscribe.tuples);
            pkt->content = unsubscribe; 
            return len;
        } 

        size_t unpackAck(uint8_t *buf, Header *hdr, Packet *pkt) {
            Ack ack; ack.header = *hdr; 
            // Second byte of the fixed header, contains the length of 
            // remaining bytes of the connect packet
            size_t len = decodeLength(&buf);
            ack.pkt_id = pack::unpack_u16(&buf);
            pkt->content = ack; 
            return len; 
        }

        ////////////////////////////////////
        // MQTT packets packing functions //
        ////////////////////////////////////
        uint8_t * packHeader(const Header &hdr) {
            uint8_t *packed = new uint8_t[MQTT_HEADER_LEN];
            uint8_t *ptr = packed;
            pack::pack_u8(&ptr, hdr.byte); 
            encodeLength(ptr, 0); 
            return packed;
        }

        uint8_t * packAck(const Packet &pkt) {
            const Ack& ack = std::get<Ack>(pkt.content);
            uint8_t *packed = new uint8_t[MQTT_ACK_LEN];
            uint8_t *ptr = packed;
            pack::pack_u8(&ptr, ack.header.byte);
            encodeLength(ptr, MQTT_HEADER_LEN); 
            ++ptr; 
            pack::pack_u16(&ptr, ack.pkt_id);
            return packed;
        }

        uint8_t * packConnack(const Packet &pkt) {
            const Connack& connack = std::get<Connack>(pkt.content);
            uint8_t *packed = new uint8_t[MQTT_ACK_LEN];
            uint8_t *ptr = packed;
            pack::pack_u8(&ptr, connack.header.byte);
            encodeLength(ptr, MQTT_HEADER_LEN); 
            ++ptr; 
            pack::pack_u8(&ptr, connack.byte);
            pack::pack_u8(&ptr, connack.rc);
            return packed;
        }

        uint8_t * packSuback(const Packet &pkt) {
            const Suback& suback = std::get<Suback>(pkt.content);
            size_t pkt_len = MQTT_HEADER_LEN + sizeof(uint16_t) + suback.rcs_len;
            uint8_t *packed = new uint8_t[pkt_len];
            uint8_t *ptr = packed;
            pack::pack_u8(&ptr, suback.header.byte);
            size_t len = sizeof(uint16_t) + suback.rcs_len;
            ptr += encodeLength(ptr, len); 
            pack::pack_u16(&ptr, suback.pkt_id);
            for (int i = 0; i < suback.rcs_len; ++i) { pack::pack_u8(&ptr, suback.rcs[i]); }
            return packed;
        }

        uint8_t * packPublish(const Packet &pkt) {
            // We must calculate the total length of the packet including header 
            // and length field of the fixed header part
            const Publish& publish = std::get<Publish>(pkt.content);
            size_t pkt_len = MQTT_HEADER_LEN + sizeof(uint16_t) + publish.topic_len + publish.payload_len; 
            if( publish.header.bits.qos > static_cast<unsigned int>(qos_level::AT_MOST_ONCE) ) {
                pkt_len += sizeof(uint16_t); 
            } 
            int remaininglen_offset = 0; 
            if ( (pkt_len-1) > 0x200000 ) { remaininglen_offset = 3; } 
            else if ( (pkt_len-1) > 0x4000 ) { remaininglen_offset = 2; } 
            else if ( (pkt_len-1) > 0x80 ) { remaininglen_offset = 1; }
            pkt_len += remaininglen_offset; 
            uint8_t *packed = new uint8_t[pkt_len]; 
            uint8_t *ptr = packed; 
            pack::pack_u8(&ptr, publish.header.byte); 
            // Total len of the packet excluding fixed header len 
            size_t len = (pkt_len - MQTT_HEADER_LEN - remaininglen_offset); 
            // TODO handle case where step is >1, e.g. when a message longer than 128 bytes is published
            ptr += encodeLength(ptr, len); 
            // Topic len followed by topic name in bytes 
            pack::pack_u16(&ptr, publish.topic_len);
            pack::pack_bytes(&ptr, publish.topic); 
            // Packet id 
            if( publish.header.bits.qos > static_cast<unsigned int>(qos_level::AT_MOST_ONCE) ) {
                pack::pack_u16(&ptr, publish.pkt_id); 
            }
            pack::pack_bytes(&ptr, publish.payload); 
            return packed; 
        }
    } 

    using UnpackHandler = std::function<size_t(uint8_t*, Header*, Packet*)>;
    // Unpack functions mapping unpacking_handlers positioned in the array based on message type
    std::array<UnpackHandler,11> unpack_handlers = {
        NULL,
        unpackConnect, 
        NULL,
        unpackPublish,
        unpackAck,
        unpackAck,
        unpackAck,
        unpackAck,
        unpackSubscribe,
        NULL,
        unpackUnsubscribe
    }; 

    using PackHandler = std::function<uint8_t *(const Packet &)>; 
    std::array<PackHandler,13> pack_handlers = {
        NULL,
        NULL,
        packConnack,
        packPublish,
        packAck,
        packAck,
        packAck,
        packAck,
        NULL,
        packSuback,
        NULL,
        packAck,
        NULL
    };


    // MQTT v3.1.1 standard, Remaining length field on the fixed header can be at most 4 bytes. 
    static const int MAX_LEN_BYTES = 4; 

    // Encode Remaining Length on a MQTT packet header, comprised of Variable Header and Payload 
    // if present. It does not take into account the bytes required to store itself. 
    // Refer to MQTT v3.1.1 algorithm for the implementation. 
    int encodeLength(uint8_t *buf, size_t len) {
        int bytes = 0;
        do {
            if ( bytes+1 > MAX_LEN_BYTES ) { return bytes; }
            uint8_t d = static_cast<uint8_t>( len & 0x7f ); 
            len = len >> 7; 
            // if there are more digits to encode, set the top bit of this digit 
            if ( len > 0 ) { d |= 0x80; } 
            buf[bytes++] = d; 
        } while (len > 0); 
        return bytes;
    }

    // Decode Remaining Length comprised of Variable Header and Payload if present. 
    // It does not take into account the bytes for storing length. 
    // Refer to MQTT v3.1.1 algorithm for the implementation suggestion. 
    ///////////////////////////////////////////////////////////
    // TODO Handle case where multiplier > 128 * 128 * 128
    unsigned long long decodeLength(uint8_t **buf) {
        uint8_t c;
        int multiplier = 1; 
        unsigned long long value = 0ULL; 
        do {
            c = **buf; 
            value += (c & 0x7f) * multiplier; 
            multiplier = multiplier << 7; 
            (*buf)++; 
        } while ( (c & 0x80) != 0 ); 
        return value; 
    }

    int unpackPacket(uint8_t *buf, Packet *pkt) {
        int rc = 0; 
        // Read first byte of the fixed header 
        uint8_t type = *buf; 
        Header header; header.byte = type;
        if (header.bits.type == static_cast<uint8_t>(packet_type::DISCONNECT) ||
            header.bits.type == static_cast<uint8_t>(packet_type::PINGREQ) ||
            header.bits.type == static_cast<uint8_t>(packet_type::PINGRESP) ) {
            pkt->content = header;
        } 
        else { // Call the appropriate unpack handler based on the message type 
            auto unpack_handler = unpack_handlers[header.bits.type]; 
            if ( unpack_handler ) { rc = unpack_handler(++buf, &header, pkt); } 
        } 
        return rc; 
    }

    //////////////////////////////////
    // MQTT packets building functions
    //////////////////////////////////
    Header *packetHeader(uint8_t byte) {
        Header* header = new Header(); 
        header->byte = byte; 
        return header; 
    } 

    Ack *packetAck(uint8_t byte, unsigned short pkt_id) {
        Ack* ack = new Ack(); 
        ack->pkt_id = pkt_id; 
        ack->header.byte = byte; 
        return ack; 
    }

    Connack *packetConnack(uint8_t byte, uint8_t cflags, uint8_t rc) {
        Connack* connack = new Connack(); 
        connack->rc = rc; 
        connack->byte = cflags; 
        connack->header.byte = byte; 
        return connack; 
    }

    Suback *packetSuback(uint8_t byte, unsigned short pkt_id, uint8_t *rcs, unsigned short rcs_len) {
        Suback* suback = new Suback(); 
        suback->rcs_len = rcs_len; 
        suback->pkt_id = pkt_id; 
        suback->header.byte = byte; 
        if( rcs_len > 0 ) {
            suback->rcs = new uint8_t[rcs_len]; 
            std::memcpy(suback->rcs, rcs, rcs_len); 
        }
        return suback; 
    }

    Publish *packetPublish(uint8_t byte, unsigned short pkt_id, size_t topic_len, uint8_t *topic, size_t payload_len, uint8_t *payload) {
        Publish* publish = new Publish(); 
        publish->pkt_id = pkt_id; 
        publish->header.byte = byte; 
        publish->topic_len = topic_len; 
        if( topic_len > 0 ){
            publish->topic = new uint8_t[topic_len]; 
            std::memcpy(publish->topic, topic, topic_len); 
        }
        publish->payload_len = payload_len; 
        if( payload_len > 0 ){
            publish->payload = new uint8_t[payload_len]; 
            std::memcpy(publish->payload, payload, payload_len); 
        }
        return publish; 
    }

    void packetRelease(Packet *pkt, unsigned type) {
        packet_type packetType = static_cast<packet_type>(type); 
        switch( packetType ) {
            case packet_type::CONNECT: {
                auto& connect = std::get<Connect>(pkt->content);
                delete[] connect.payload.client_id; 
                if ( connect.bits.username == 1 ) { delete[] connect.payload.username; } 
                if ( connect.bits.password == 1) { delete[] connect.payload.password; } 
                if ( connect.bits.will == 1 ) {
                    delete[] connect.payload.will_message;
                    delete[] connect.payload.will_topic;
                }
                break;
            }
            case packet_type::SUBSCRIBE: {
                auto& subscribe = std::get<Subscribe>(pkt->content);
                for (unsigned i = 0; i < subscribe.tuples_len; i++){
                    delete[] subscribe.tuples[i].topic;
                }
                delete[] subscribe.tuples;
                break;
            }
            case packet_type::UNSUBSCRIBE: {
                auto& unsubscribe = std::get<Unsubscribe>(pkt->content);
                for (unsigned i = 0; i < unsubscribe.tuples_len; i++){
                    delete[] unsubscribe.tuples[i].topic;
                }
                delete[] unsubscribe.tuples;
                break;
            }
            case packet_type::SUBACK: {
                auto& suback = std::get<Suback>(pkt->content);
                delete[] suback.rcs;
                break;
            } 
            case packet_type::PUBLISH: {
                auto& publish = std::get<Publish>(pkt->content);
                delete[] publish.topic;
                delete[] publish.payload;
                break;
            } 
            default: break; 
        }
    }

    uint8_t *packPacket(const Packet& pkt, unsigned type) {
        // Special handling for PINGREQ and PINGRESP
        if( type == static_cast<unsigned>(packet_type::PINGREQ) || 
            type == static_cast<unsigned>(packet_type::PINGRESP)) {
            const Header& headerRef = std::get<Header>(pkt.content); 
            return packHeader(headerRef); 
        }
        // Use the pack_handlers array to handle other types
        if( type < pack_handlers.size() && pack_handlers[type] ) {
            return pack_handlers[type](pkt); 
        }
        return nullptr; // Return nullptr for unsupported types
    }

}
