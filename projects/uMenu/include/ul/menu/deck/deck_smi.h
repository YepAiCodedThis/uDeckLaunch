#pragma once

#include <switch.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int deck_smi_launch_app(u64 app_id);
int deck_smi_resume_app(void);
int deck_smi_terminate_app(void);
u64 deck_smi_get_hb_takeover(void);
int deck_smi_ensure_hb_takeover(u64 app_id);
int deck_smi_launch_hb_app(const char *path);
int deck_smi_launch_hb_applet(const char *path);
int deck_smi_is_suspended(u64 *out_app_id, char *out_hb, size_t hb_len);
int deck_smi_sleep(void);
int deck_smi_reboot(void);
int deck_smi_power_off(void);
int deck_smi_open_album(void);
void deck_smi_request_settings(void);

#ifdef __cplusplus
}
#endif
