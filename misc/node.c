#include "node.h"

#include <universalvideoplayer/client.h>

#include "common/common.h"
#include "bstr.h"

// Init a node with the given format. If parent is not NULL, it is set as
// parent allocation according to m_option_type_node rules (which means
// the universalvideoplayer_node_list allocs are used for chaining the TA allocations).
// format == UNIVERSALVIDEOPLAYER_FORMAT_NONE will simply initialize it with all-0.
void node_init(struct universalvideoplayer_node *dst, int format, struct universalvideoplayer_node *parent)
{
    // Other formats need to be initialized manually.
    mp_assert(format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP || format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_ARRAY ||
           format == UNIVERSALVIDEOPLAYER_FORMAT_FLAG || format == UNIVERSALVIDEOPLAYER_FORMAT_INT64 ||
           format == UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE || format == UNIVERSALVIDEOPLAYER_FORMAT_BYTE_ARRAY ||
           format == UNIVERSALVIDEOPLAYER_FORMAT_NONE);

    void *ta_parent = NULL;
    if (parent) {
        mp_assert(parent->format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP ||
               parent->format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_ARRAY);
        ta_parent = parent->u.list;
    }

    *dst = (struct universalvideoplayer_node){ .format = format };
    if (format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP || format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_ARRAY)
        dst->u.list = talloc_zero(ta_parent, struct universalvideoplayer_node_list);
    if (format == UNIVERSALVIDEOPLAYER_FORMAT_BYTE_ARRAY)
        dst->u.ba = talloc_zero(ta_parent, struct universalvideoplayer_byte_array);
}

// Add an entry to a UNIVERSALVIDEOPLAYER_FORMAT_NODE_ARRAY.
// m_option_type_node memory management rules apply.
struct universalvideoplayer_node *node_array_add(struct universalvideoplayer_node *dst, int format)
{
    struct universalvideoplayer_node_list *list = dst->u.list;
    mp_assert(dst->format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_ARRAY && dst->u.list);
    MP_TARRAY_GROW(list, list->values, list->num);
    node_init(&list->values[list->num], format, dst);
    return &list->values[list->num++];
}

// Add an entry to a UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP. Keep in mind that this does
// not check for already existing entries under the same key.
// m_option_type_node memory management rules apply.
struct universalvideoplayer_node *node_map_add(struct universalvideoplayer_node *dst, const char *key, int format)
{
    mp_assert(key);
    return node_map_badd(dst, bstr0(key), format);
}

struct universalvideoplayer_node *node_map_badd(struct universalvideoplayer_node *dst, struct bstr key, int format)
{
    mp_assert(key.start);

    struct universalvideoplayer_node_list *list = dst->u.list;
    mp_assert(dst->format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP && dst->u.list);
    MP_TARRAY_GROW(list, list->values, list->num);
    MP_TARRAY_GROW(list, list->keys, list->num);
    list->keys[list->num] = bstrdup0(list, key);
    node_init(&list->values[list->num], format, dst);
    return &list->values[list->num++];
}

// Add a string entry to a UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP. Keep in mind that this does
// not check for already existing entries under the same key.
// m_option_type_node memory management rules apply.
void node_map_add_string(struct universalvideoplayer_node *dst, const char *key, const char *val)
{
    mp_assert(val);

    struct universalvideoplayer_node *entry = node_map_add(dst, key, UNIVERSALVIDEOPLAYER_FORMAT_NONE);
    entry->format = UNIVERSALVIDEOPLAYER_FORMAT_STRING;
    entry->u.string = talloc_strdup(dst->u.list, val);
}

void node_map_add_bstr(struct universalvideoplayer_node *dst, const char *key, bstr val)
{
    mp_assert(val.start);

    struct universalvideoplayer_node *entry = node_map_add(dst, key, UNIVERSALVIDEOPLAYER_FORMAT_NONE);
    entry->format = UNIVERSALVIDEOPLAYER_FORMAT_STRING;
    entry->u.string = bstrto0(dst->u.list, val);
}

void node_map_add_int64(struct universalvideoplayer_node *dst, const char *key, int64_t v)
{
    node_map_add(dst, key, UNIVERSALVIDEOPLAYER_FORMAT_INT64)->u.int64 = v;
}

void node_map_add_double(struct universalvideoplayer_node *dst, const char *key, double v)
{
    node_map_add(dst, key, UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE)->u.double_ = v;
}

void node_map_add_flag(struct universalvideoplayer_node *dst, const char *key, bool v)
{
    node_map_add(dst, key, UNIVERSALVIDEOPLAYER_FORMAT_FLAG)->u.flag = v;
}

universalvideoplayer_node *node_map_get(universalvideoplayer_node *src, const char *key)
{
    return node_map_bget(src, bstr0(key));
}

universalvideoplayer_node *node_map_bget(universalvideoplayer_node *src, struct bstr key)
{
    if (src->format != UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP)
        return NULL;

    for (int i = 0; i < src->u.list->num; i++) {
        if (bstr_equals0(key, src->u.list->keys[i]))
            return &src->u.list->values[i];
    }

    return NULL;
}

// Note: for UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP, this (incorrectly) takes the order into
//       account, instead of treating it as set.
bool equal_universalvideoplayer_value(const void *a, const void *b, int format)
{
    switch (format) {
    case UNIVERSALVIDEOPLAYER_FORMAT_NONE:
        return true;
    case UNIVERSALVIDEOPLAYER_FORMAT_STRING:
    case UNIVERSALVIDEOPLAYER_FORMAT_OSD_STRING:
        return strcmp(*(char **)a, *(char **)b) == 0;
    case UNIVERSALVIDEOPLAYER_FORMAT_FLAG:
        return *(int *)a == *(int *)b;
    case UNIVERSALVIDEOPLAYER_FORMAT_INT64:
        return *(int64_t *)a == *(int64_t *)b;
    case UNIVERSALVIDEOPLAYER_FORMAT_DOUBLE:
        return *(double *)a == *(double *)b;
    case UNIVERSALVIDEOPLAYER_FORMAT_NODE:
        return equal_universalvideoplayer_node(a, b);
    case UNIVERSALVIDEOPLAYER_FORMAT_BYTE_ARRAY: {
        const struct universalvideoplayer_byte_array *a_r = a, *b_r = b;
        if (a_r->size != b_r->size)
            return false;
        return memcmp(a_r->data, b_r->data, a_r->size) == 0;
    }
    case UNIVERSALVIDEOPLAYER_FORMAT_NODE_ARRAY:
    case UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP:
    {
        universalvideoplayer_node_list *l_a = *(universalvideoplayer_node_list **)a, *l_b = *(universalvideoplayer_node_list **)b;
        if (l_a->num != l_b->num)
            return false;
        for (int n = 0; n < l_a->num; n++) {
            if (format == UNIVERSALVIDEOPLAYER_FORMAT_NODE_MAP) {
                if (strcmp(l_a->keys[n], l_b->keys[n]) != 0)
                    return false;
            }
            if (!equal_universalvideoplayer_node(&l_a->values[n], &l_b->values[n]))
                return false;
        }
        return true;
    }
    }
    MP_ASSERT_UNREACHABLE(); // supposed to be able to handle all defined types
}

// Remarks see equal_universalvideoplayer_value().
bool equal_universalvideoplayer_node(const struct universalvideoplayer_node *a, const struct universalvideoplayer_node *b)
{
    if (a->format != b->format)
        return false;
    return equal_universalvideoplayer_value(&a->u, &b->u, a->format);
}
