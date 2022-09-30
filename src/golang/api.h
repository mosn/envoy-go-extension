#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void moeHttpContinue(unsigned long long int filterHolder, int status);

void moeHttpGetRequestHeader(unsigned long long int filterHolder, void* key, void *value);
void moeHttpCopyRequestHeaders(unsigned long long int filterHolder, void* strs, void *buf);

void moeHttpGetResponseHeader(unsigned long long int filterHolder, void* key, void *value);
void moeHttpCopyResponseHeaders(unsigned long long int filterHolder, void* strs, void *buf);

void moeHttpSetResponseHeader(unsigned long long int filterHolder, void* key, void *value);

void moeHttpGetBuffer(unsigned long long int filterHolder, unsigned long long int buffer, void *value);
void moeHttpSetBuffer(unsigned long long int filterHolder, unsigned long long int buffer, void *data, int length);

#ifdef __cplusplus
} // extern "C"
#endif