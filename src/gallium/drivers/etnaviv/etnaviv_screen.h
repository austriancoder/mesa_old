/*
 * Copyright (c) 2012-2013 Etnaviv Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef H_ETNAVIV_SCREEN
#define H_ETNAVIV_SCREEN

#include <etnaviv_drmif.h>
#include "etnaviv_internal.h"

#include "pipe/p_screen.h"
#include "os/os_thread.h"

/* Enum with indices for each of the feature words */
enum viv_features_word
{
    viv_chipFeatures = 0,
    viv_chipMinorFeatures0 = 1,
    viv_chipMinorFeatures1 = 2,
    viv_chipMinorFeatures2 = 3,
    viv_chipMinorFeatures3 = 4,
    VIV_FEATURES_WORD_COUNT /* Must be last */
};

/** Convenience macro to probe features from state.xml.h:
 * VIV_FEATURE(chipFeatures, FAST_CLEAR)
 * VIV_FEATURE(chipMinorFeatures1, AUTO_DISABLE)
 */
#define VIV_FEATURE(screen, word, feature) ((screen->features[viv_ ## word] & (word ## _ ## feature))!=0)

struct etna_screen {
    struct pipe_screen base;

    struct etna_device *dev;
    struct etna_pipe *pipe;

    uint32_t model;
    uint32_t revision;
    uint32_t features[5];

    int drm_fd;

    struct etna_pipe_specs specs;
};

/* Resolve target.
 * Used by etna_screen_flush_frontbuffer
 */
struct etna_rs_target
{
   unsigned rs_format;
   bool swap_rb;
   unsigned width, height;
   struct etna_bo *bo;
   size_t stride;
   struct pipe_fence_handle *fence;
};

static INLINE struct etna_screen *
etna_screen(struct pipe_screen *pscreen)
{
    return (struct etna_screen *)pscreen;
}

boolean etna_screen_bo_get_handle(struct pipe_screen *pscreen,
	struct etna_bo *bo,
	unsigned stride,
	struct winsys_handle *whandle);

struct etna_bo *
etna_screen_bo_from_handle(struct pipe_screen *pscreen,
	struct winsys_handle *whandle,
	unsigned *out_stride);

struct pipe_screen *
etna_screen_create(struct etna_device *dev);

#endif

