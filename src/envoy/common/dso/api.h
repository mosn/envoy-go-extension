#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// This describes the return value of a C Api from Go.
#define CAPIOK 0
#define CAPIFilterIsGone -1
#define CAPIFilterIsDestroy -2
#define CAPINotInGo -3
#define CAPIInvalidPhase -4
#define CAPIYield -5

typedef struct {
  unsigned long long int configId;
  // TODO: combine these fields into a single int, for save memory
  int phase;
  int waitSema; // :1
} httpRequest;

typedef enum {
  Set,
  Append,
  Prepend,
} bufferAction;

typedef enum {
  HeaderSet,
  HeaderAdd,
} headerAction;

int moeHttpContinue(void* r, int status);
int moeHttpSendLocalReply(void* r, int response_code, void* body_text, void* headers,
                          long long int grpc_status, void* details);

int moeHttpGetHeader(void* r, void* key, void* value);
int moeHttpCopyHeaders(void* r, void* strs, void* buf);
int moeHttpSetHeaderHelper(void* r, void* key, void* value, headerAction action);
int moeHttpRemoveHeader(void* r, void* key);

int moeHttpGetBuffer(void* r, unsigned long long int buffer, void* value);
int moeHttpSetBufferHelper(void* r, unsigned long long int buffer, void* data, int length,
                           bufferAction action);

int moeHttpCopyTrailers(void* r, void* strs, void* buf);
int moeHttpSetTrailer(void* r, void* key, void* value);

int moeHttpGetStringValue(void* r, int id, void* value);

void moeHttpFinalize(void* r, int reason);

int moeHttpGetDynamicMetadata(void* r, void* name, void* buf);
int moeHttpSetDynamicMetadata(void* r, void* name, void* key, void* buf);

#ifdef __cplusplus
} // extern "C"
#endif
