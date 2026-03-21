/*
 * DNSPacketCodec.cpp - DNS パケット処理補助の実装
 */

#include "DNSPacketCodec.h"
#include "Config.h"

namespace {
constexpr size_t kDnsHeaderSize = 12;
constexpr size_t kDnsMaxPacketSize = 512;
constexpr uint8_t kDnsAnswerCountHighByte = 0x00;
constexpr uint8_t kDnsAnswerCountLowByte = 0x01;
constexpr uint8_t kDnsDataLengthHighByte = 0x00;
}

String DNSPacketCodec::extractDomainFromQuery(uint8_t* packet, size_t len) {
  if (len < kDnsHeaderSize) {
    return "";
  }

  String domain;
  uint8_t* ptr = packet + kDnsHeaderSize;
  uint8_t* end = packet + len;

  while (ptr < end && *ptr != 0) {
    uint8_t labelLen = *ptr++;

    if ((labelLen & DNS_COMPRESSION_POINTER_MASK) == DNS_COMPRESSION_POINTER_MASK) {
      break;
    }

    if (labelLen > DNS_LABEL_MAX_LENGTH || ptr + labelLen > end) {
      return "";
    }

    if (domain.length() > 0) {
      domain += ".";
    }

    for (int i = 0; i < labelLen; i++) {
      domain += static_cast<char>(*ptr++);
    }
  }

  domain.toLowerCase();
  return domain;
}

size_t DNSPacketCodec::buildIPv4Response(uint8_t* response, const uint8_t* query, size_t queryLen, IPAddress responseIP) {
  if (queryLen > kDnsMaxPacketSize) {
    return 0;
  }

  memcpy(response, query, queryLen);
  response[2] = DNS_RESPONSE_FLAGS_BYTE2;
  response[3] = DNS_RESPONSE_FLAGS_BYTE3;
  response[6] = kDnsAnswerCountHighByte;
  response[7] = kDnsAnswerCountLowByte;

  size_t answerOffset = queryLen;
  response[answerOffset++] = (DNS_COMPRESSION_POINTER_QUERY >> 8) & 0xFF;
  response[answerOffset++] = DNS_COMPRESSION_POINTER_QUERY & 0xFF;
  response[answerOffset++] = (DNS_TYPE_A >> 8) & 0xFF;
  response[answerOffset++] = DNS_TYPE_A & 0xFF;
  response[answerOffset++] = (DNS_CLASS_IN >> 8) & 0xFF;
  response[answerOffset++] = DNS_CLASS_IN & 0xFF;
  response[answerOffset++] = (DNS_TTL_SECONDS >> 24) & 0xFF;
  response[answerOffset++] = (DNS_TTL_SECONDS >> 16) & 0xFF;
  response[answerOffset++] = (DNS_TTL_SECONDS >> 8) & 0xFF;
  response[answerOffset++] = DNS_TTL_SECONDS & 0xFF;
  response[answerOffset++] = kDnsDataLengthHighByte;
  response[answerOffset++] = DNS_IPV4_ADDRESS_LENGTH;
  response[answerOffset++] = responseIP[0];
  response[answerOffset++] = responseIP[1];
  response[answerOffset++] = responseIP[2];
  response[answerOffset++] = responseIP[3];

  return answerOffset;
}
