#pragma once

#include <stdint.h>
#include <stdlib.h>

struct NonConstString {
  char* data;
  unsigned long long int len;
};
typedef struct NonConstString NonConstString;

struct ConstString {
  const char* data;
  unsigned long long int len;
};
typedef struct ConstString ConstString;

// cgo request struct
typedef struct {
  ConstString req_body;
  ConstString filter_chain;
  ConstString upstream_address;
  ConstString downstream_address;
  ConstString* headers;
  size_t header_size;
  ConstString* trailers;
  size_t trailer_size;
  void* filter;
  uint64_t cid;
  uint64_t sid;
} Request;

// cgo response struct
typedef struct {
  NonConstString resp_body;
  NonConstString* headers;
  NonConstString* trailers;
  int status;
  int need_async;
  int direct_response;
} Response;

// post callback function type
typedef void (*fc)(void* filter, Response resp);

extern void runPostCallback(fc f, void* filter, Response resp);

void moeConnectionWrite(unsigned long long int envoyFilter, unsigned long long int buffers,
                        int buffersNum);

void moeConnectionClose(unsigned long long int envoyFilter, int closeType);

void moeFinalizer(unsigned long long int envoyFilter);

void moeConnectUpstream(unsigned long long int golangConn, unsigned long long int addrData,
                        unsigned long long int addrLen, unsigned long long int clusterNameData,
                        unsigned long long int clusterNameLen, unsigned long long int soIdData,
                        unsigned long long int soIdLen);

void moeUpstreamWrite(unsigned long long int upstreamConn, unsigned long long int buffers,
                      int buffersNum);

void moeUpstreamClose(unsigned long long int upstreamConn, int closeType);

void moeUpstreamFinalizer(unsigned long long int upstreamConn);

struct TlsInfo {
  uint64_t cipherSuiteId;
  const char* tlsVersionPtr;
  int tlsVersionLen;
  const char* urlEncodedPemEncodedPeerCertificatePtr;
  int urlEncodedPemEncodedPeerCertificateLen;
  const char* nextProtocolPtr;
  int nextProtocolLen;
};
typedef struct TlsInfo TlsInfo;

void moeTlsConnectionSelectCert(unsigned long long int envoyTlsHandshaker, int respType,
                                unsigned long long int certNameData, int certNameLen);
