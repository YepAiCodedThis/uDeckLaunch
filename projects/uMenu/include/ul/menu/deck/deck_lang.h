#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void dh_lang_apply(const char *pref);
int dh_lang_count(void);
int dh_lang_index(const char *pref);
void dh_lang_code(int index, char *out, size_t n);
const char *dh_lang_name(int index);
const char *dh_lang(const char *key);

#ifdef __cplusplus
}
#endif
