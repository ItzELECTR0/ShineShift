#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ShineResult { SHINE_OK = 0, SHINE_FAILED = 1, SHINE_UNSUPPORTED = 2 };
enum ShineCompositor { SHINE_COMPOSITOR_NONE = 0, SHINE_COMPOSITOR_HYPRLAND = 1, SHINE_COMPOSITOR_FAKE = 2 };
enum ShineEvent { SHINE_EVENT_RELOADED = 1, SHINE_EVENT_OUTPUTS = 2 };
enum { SHINE_MAX_DISPLAYS = 16 };

typedef struct ShineDisplay {
    char id[64];
    char mfg[8];
    char model[32];
    char serial[32];
    char connector[32];
    int bus;
} ShineDisplay;

int shine_init(char *error, size_t error_capacity);
void shine_shutdown(void);
int shine_is_fake(void);
int shine_detect(ShineDisplay *displays, int capacity, int rescan);
int shine_capabilities(int display, char *out, size_t capacity);
int shine_get(int display, int feature, int *value, int *maximum);
int shine_set(int display, int feature, int value);
int shine_drift(int display, int feature, int value);
int shine_to_raw(int percent, int minimum, int maximum);
int shine_to_percent(int raw, int minimum, int maximum);

int shine_compositor(void);
int shine_outputs(char *out, size_t capacity, int rescan_config);
int shine_set_sdr(const char *selector, double value);
int shine_events_open(void);
int shine_events_read(int fd);
int shine_hotplug_open(void);
int shine_hotplug_read(int fd);
int shine_screens_off(void);
int shine_float_window(const char *title, int width, int height);
int shine_config_status(void);
int shine_config_install(void);
int shine_config_write(const char *entries);

#ifdef __cplusplus
}
#endif
