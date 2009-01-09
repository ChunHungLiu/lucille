#include <stdio.h>
#include <stdlib.h>

#include "hbuffer.h"
#include "cachelib.h"

ri_hbuffer_t *g_hbuffer;

void
lse_init(int w, int h)
{
    g_hbuffer           = ri_hbuffer_new();
    g_hbuffer->id       = 0;
    g_hbuffer->width    = w;
    g_hbuffer->height   = h;
    g_hbuffer->buffer   = malloc(4 * sizeof(float) * w * h);

}

void
lse_save_cache_iiic(int layer, int x, int y, float *val)
{
    float *buf;

    buf = (float *)g_hbuffer->buffer;

    buf[4 * (y * g_hbuffer->width + x)] = val[0];
    buf[4 * (y * g_hbuffer->width + x)] = val[1];
    buf[4 * (y * g_hbuffer->width + x)] = val[2];
    buf[4 * (y * g_hbuffer->width + x)] = val[3];

}

