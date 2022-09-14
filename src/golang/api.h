#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void moeHttpDecodeContinue(unsigned long long int filterHolder, int end);
void moeHttpGetRequestHeader(unsigned long long int filterHolder, void* key, void *value);
void moeHttpCopyRequestHeaders(unsigned long long int filterHolder, void* strs, void *buf);

#ifdef __cplusplus
} // extern "C"
#endif