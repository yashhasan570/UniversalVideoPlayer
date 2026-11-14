#ifndef UNIVERSALVIDEOPLAYER_UNIVERSALVIDEOPLAYER_H
#define UNIVERSALVIDEOPLAYER_UNIVERSALVIDEOPLAYER_H

// This should be accessed by glue code only, never normal code.
// The only purpose of this is to make universalvideoplayer library-safe.
// Think hard before adding new members.
struct universalvideoplayer_global {
    struct mp_log *log;
    struct m_config_shadow *config;
    struct mp_client_api *client_api;
    char *configdir;
    struct stats_base *stats;
    struct demux_packet_pool *packet_pool;
    struct curl_ctx *curl;
};

#endif
