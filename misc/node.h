#ifndef MP_MISC_NODE_H_
#define MP_MISC_NODE_H_

#include <stdbool.h>
#include <stdint.h>

struct bstr;
struct universalvideoplayer_node;

void node_init(struct universalvideoplayer_node *dst, int format, struct universalvideoplayer_node *parent);
struct universalvideoplayer_node *node_array_add(struct universalvideoplayer_node *dst, int format);
struct universalvideoplayer_node *node_map_add(struct universalvideoplayer_node *dst, const char *key, int format);
struct universalvideoplayer_node *node_map_badd(struct universalvideoplayer_node *dst, struct bstr key, int format);
void node_map_add_string(struct universalvideoplayer_node *dst, const char *key, const char *val);
void node_map_add_bstr(struct universalvideoplayer_node *dst, const char *key, struct bstr val);
void node_map_add_int64(struct universalvideoplayer_node *dst, const char *key, int64_t v);
void node_map_add_double(struct universalvideoplayer_node *dst, const char *key, double v);
void node_map_add_flag(struct universalvideoplayer_node *dst, const char *key, bool v);
struct universalvideoplayer_node *node_map_get(struct universalvideoplayer_node *src, const char *key);
struct universalvideoplayer_node *node_map_bget(struct universalvideoplayer_node *src, struct bstr key);
bool equal_universalvideoplayer_value(const void *a, const void *b, int format);
bool equal_universalvideoplayer_node(const struct universalvideoplayer_node *a, const struct universalvideoplayer_node *b);

#endif
