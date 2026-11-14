#ifndef MP_AV_LOG_H
#define MP_AV_LOG_H

#include <stdbool.h>

struct universalvideoplayer_global;
struct mp_log;
void init_libav(struct universalvideoplayer_global *global);
void uninit_libav(struct universalvideoplayer_global *global);
void check_library_versions(struct mp_log *log, int v);
#endif
