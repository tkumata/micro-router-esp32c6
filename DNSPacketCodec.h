/*
 * DNSPacketCodec.h - DNS パケット処理補助
 */

#ifndef DNS_PACKET_CODEC_H
#define DNS_PACKET_CODEC_H

#include <Arduino.h>
#include <IPAddress.h>

namespace DNSPacketCodec {
String extractDomainFromQuery(const uint8_t* packet, size_t len);
size_t buildIPv4Response(uint8_t* response, const uint8_t* query, size_t queryLen, IPAddress responseIP);
}

#endif // DNS_PACKET_CODEC_H
