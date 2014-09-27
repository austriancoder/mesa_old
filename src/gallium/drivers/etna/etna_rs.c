/*
 * Copyright (c) 2012-2014 Etnaviv Project
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

#include "etna_rs.h"
#include "etna_internal.h"
#include "state_3d.xml.h"

#include <assert.h>

/* compile RS state struct */
#define SET_STATE(addr, value) cs->addr = (value)

void etna_compile_rs_state(struct etna_screen *restrict screen, struct compiled_rs_state *cs, const struct rs_state *rs)
{
    /* TILED and SUPERTILED layout have their strides multiplied with 4 in RS */
    unsigned source_stride_shift = (rs->source_tiling != ETNA_LAYOUT_LINEAR) ? 2 : 0;
    unsigned dest_stride_shift = (rs->dest_tiling != ETNA_LAYOUT_LINEAR) ? 2 : 0;

    /* tiling == ETNA_LAYOUT_MULTI_TILED or ETNA_LAYOUT_MULTI_SUPERTILED? */
    bool source_multi = (rs->source_tiling & 0x4)?true:false;
    bool dest_multi = (rs->dest_tiling & 0x4)?true:false;

    /* TODO could just pre-generate command buffer, would simply submit to one memcpy */
    SET_STATE(RS_CONFIG, VIVS_RS_CONFIG_SOURCE_FORMAT(rs->source_format) |
                            (rs->downsample_x?VIVS_RS_CONFIG_DOWNSAMPLE_X:0) |
                            (rs->downsample_y?VIVS_RS_CONFIG_DOWNSAMPLE_Y:0) |
                            ((rs->source_tiling&1)?VIVS_RS_CONFIG_SOURCE_TILED:0) |
                            VIVS_RS_CONFIG_DEST_FORMAT(rs->dest_format) |
                            ((rs->dest_tiling&1)?VIVS_RS_CONFIG_DEST_TILED:0) |
                            ((rs->swap_rb)?VIVS_RS_CONFIG_SWAP_RB:0) |
                            ((rs->flip)?VIVS_RS_CONFIG_FLIP:0));
#if 0 /* TODO */
    SET_STATE(RS_SOURCE_ADDR, rs->source[0]);
    SET_STATE(RS_PIPE_SOURCE_ADDR[0], rs->source[0]);
#endif
    SET_STATE(RS_SOURCE_STRIDE, (rs->source_stride << source_stride_shift) |
                            ((rs->source_tiling&2)?VIVS_RS_SOURCE_STRIDE_TILING:0) |
                            ((source_multi)?VIVS_RS_SOURCE_STRIDE_MULTI:0));
#if 0 /* TODO */
    SET_STATE(RS_DEST_ADDR, rs->dest[0]);
    SET_STATE(RS_PIPE_DEST_ADDR[0], rs->dest[0]);
#endif
    SET_STATE(RS_DEST_STRIDE, (rs->dest_stride << dest_stride_shift) |
                            ((rs->dest_tiling&2)?VIVS_RS_DEST_STRIDE_TILING:0) |
                            ((dest_multi)?VIVS_RS_DEST_STRIDE_MULTI:0));
    if (screen->specs.pixel_pipes == 1)
    {
        SET_STATE(RS_WINDOW_SIZE, VIVS_RS_WINDOW_SIZE_WIDTH(rs->width) | VIVS_RS_WINDOW_SIZE_HEIGHT(rs->height));
    }
    else if (screen->specs.pixel_pipes == 2)
    {
        assert((rs->height&7) == 0); /* GPU hangs happen if height not 8-aligned */
        if (source_multi)
        {
            SET_STATE(RS_PIPE_SOURCE[1], rs->source[1]);
        }
        if (dest_multi)
        {
            SET_STATE(RS_PIPE_DEST[1], rs->dest[1]);
        }
        SET_STATE(RS_WINDOW_SIZE, VIVS_RS_WINDOW_SIZE_WIDTH(rs->width) | VIVS_RS_WINDOW_SIZE_HEIGHT(rs->height / 2));
    }
    SET_STATE(RS_PIPE_OFFSET[0], VIVS_RS_PIPE_OFFSET_X(0) | VIVS_RS_PIPE_OFFSET_Y(0));
    SET_STATE(RS_PIPE_OFFSET[1], VIVS_RS_PIPE_OFFSET_X(0) | VIVS_RS_PIPE_OFFSET_Y(rs->height / 2));
    SET_STATE(RS_DITHER[0], rs->dither[0]);
    SET_STATE(RS_DITHER[1], rs->dither[1]);
    SET_STATE(RS_CLEAR_CONTROL, VIVS_RS_CLEAR_CONTROL_BITS(rs->clear_bits) | rs->clear_mode);
    SET_STATE(RS_FILL_VALUE[0], rs->clear_value[0]);
    SET_STATE(RS_FILL_VALUE[1], rs->clear_value[1]);
    SET_STATE(RS_FILL_VALUE[2], rs->clear_value[2]);
    SET_STATE(RS_FILL_VALUE[3], rs->clear_value[3]);
    SET_STATE(RS_EXTRA_CONFIG, VIVS_RS_EXTRA_CONFIG_AA(rs->aa) | VIVS_RS_EXTRA_CONFIG_ENDIAN(rs->endian_mode));
}
