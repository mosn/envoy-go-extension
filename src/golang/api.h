#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned long long int configId;
    int phase;
} httpRequest;

void moeHttpContinue(void* r, int status);

void moeHttpGetHeader(void* r, void* key, void *value);
void moeHttpCopyHeaders(void* r, void* strs, void *buf);

void moeHttpSetHeader(void* r, void* key, void *value);

void moeHttpGetBuffer(void* r, unsigned long long int buffer, void *value);
void moeHttpSetBuffer(void* r, unsigned long long int buffer, void *data, int length);

void moeHttpFinalize(void* r, int reason);

#ifdef __cplusplus
} // extern "C"
#endif