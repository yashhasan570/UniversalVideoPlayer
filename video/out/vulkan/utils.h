#pragma once
#include "common.h"
#include "video/out/gpu/context.h"

bool universalvideoplayerk_init(struct universalvideoplayerk_ctx *vk, struct ra_ctx *ctx, const char *surface_ext);
void universalvideoplayerk_uninit(struct universalvideoplayerk_ctx *vk);
