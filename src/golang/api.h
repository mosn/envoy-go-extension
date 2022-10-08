#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned long long int configId;
    int phase;
} Request;

void moeHttpContinue(unsigned long long int filterHolder, int status);

void moeHttpGetHeader(unsigned long long int filterHolder, void* key, void *value);
void moeHttpCopyHeaders(unsigned long long int filterHolder, void* strs, void *buf);

void moeHttpSetHeader(unsigned long long int filterHolder, void* key, void *value);

void moeHttpGetBuffer(unsigned long long int filterHolder, unsigned long long int buffer, void *value);
void moeHttpSetBuffer(unsigned long long int filterHolder, unsigned long long int buffer, void *data, int length);

#ifdef __cplusplus
} // extern "C"
#endif