#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ws.h"

uint32_t WS::BuildPacket(enum WebSocketOpCode opcode, const std::string &payload, std::string &msg, bool mask)
{
    WebsocketPacketHeader_t header;

    int payloadIndex = 2;
    
    // Fill in meta.bits
    header.meta.bits.FIN = 1;
    header.meta.bits.RSV = 0;
    header.meta.bits.OPCODE = opcode;
    header.meta.bits.MASK = mask & payload.length() > 0;

    // Calculate length
    if (payload.length() < 126)
    {
        header.meta.bits.PAYLOADLEN = payload.length();
    }
    else if (payload.length() < 0x10000)
    {
        header.meta.bits.PAYLOADLEN = 126;
    }
    else
    {
        header.meta.bits.PAYLOADLEN = 127;
    }

    msg.clear();
    msg.append(1, header.meta.bytes.byte0);
    msg.append(1, header.meta.bytes.byte1);
 
    // Generate mask
    header.mask.maskKey = (uint32_t)rand();
    
    // Fill in payload length
    if(header.meta.bits.PAYLOADLEN == 126)
    {
        msg.append(1, (payload.length() >> 8) & 0xFF);
        msg.append(1, payload.length() & 0xFF);
        payloadIndex = 4;
     }

    if(header.meta.bits.PAYLOADLEN == 127)
    {
        msg.append(4, 0); //(payload.length() >> 56) & 0xFF;
                          //(payload.length() >> 48) & 0xFF;
                          //(payload.length() >> 40) & 0xFF;
                          //(payload.length() >> 32) & 0xFF;
        msg.append(1, (payload.length() >> 24) & 0xFF);
        msg.append(1, (payload.length() >> 16) & 0xFF);
        msg.append(1, (payload.length() >> 8)  & 0xFF);
        msg.append(1, payload.length() & 0xFF);
        payloadIndex = 10;
    }

    // Insert masking key
    if(header.meta.bits.MASK)
    {
        msg.append((const char *)&header.mask.maskBytes, 4);
        payloadIndex += 4;
    }

    // Copy in payload
    msg.append(payload);

    // Mask payload if needed
    if(header.meta.bits.MASK)
    {
        for(uint32_t i = 0; i < payload.length(); i++)
        {
            msg[payloadIndex + i] = msg[payloadIndex + i] ^ header.mask.maskBytes[i%4];
        }
    }

    return (msg.length());
}

int WS::ParsePacket(WebsocketPacketHeader_t *header, std::string &packet)
{
    if (packet.length() < 2)
    {
        return WEBSOCKET_FAIL;
    }
    header->meta.bytes.byte0 = (uint8_t) packet[0];
    header->meta.bytes.byte1 = (uint8_t) packet[1];

    // Payload length
    int payloadIndex = 2;
    header->length = header->meta.bits.PAYLOADLEN;

    if (header->meta.bits.PAYLOADLEN == 126)
    {
        if (packet.length() < 4)
        {
            return WEBSOCKET_FAIL;
        }
        header->length = packet[2] << 8 | packet[3];
        payloadIndex = 4;
    }
    
    if (header->meta.bits.PAYLOADLEN == 127)
    {
        if (packet.length() < 10)
        {
            return WEBSOCKET_FAIL;
        }
        header->length = packet[6] << 24 | packet[7] << 16 | packet[8] << 8 | packet[9];
        payloadIndex = 10;
    }

    // Mask
    if (header->meta.bits.MASK)
    {
        if (packet.length() < payloadIndex + 4)
        {
            return WEBSOCKET_FAIL;
        }
        header->mask.maskBytes[0] = packet[payloadIndex + 0];
        header->mask.maskBytes[1] = packet[payloadIndex + 1];
        header->mask.maskBytes[2] = packet[payloadIndex + 2];
        header->mask.maskBytes[3] = packet[payloadIndex + 3];
        payloadIndex = payloadIndex + 4;    
        
        // Decrypt    
        if (packet.length() < payloadIndex + header->length)
        {
            return WEBSOCKET_FAIL;
        }
        for(uint32_t i = 0; i < header->length; i++)
        {
            packet[payloadIndex + i] = packet[payloadIndex + i] ^ header->mask.maskBytes[i%4];
        }
    }

    // Payload start
    if (packet.length() < payloadIndex + header->length)
    {
        return WEBSOCKET_FAIL;
    }
    header->start = payloadIndex;

    return WEBSOCKET_SUCCESS;
}

