#pragma once

#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

void dh_sfx_init(void);
void dh_sfx_exit(void);
void dh_sfx_bind(const DhSettings *s);
void dh_sfx_nav(void);
void dh_sfx_launch(void);
void dh_sfx_startup(void);

#ifdef __cplusplus
}
#endif
