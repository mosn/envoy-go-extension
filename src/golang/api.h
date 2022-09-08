#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void moeHttpDecodeContinue(unsigned long long int filterHolder, int end);
void moeHttpGetRequestHeader(unsigned long long int filterHolder, void* key, void *value);

#ifdef __cplusplus
} // extern "C"
#endif