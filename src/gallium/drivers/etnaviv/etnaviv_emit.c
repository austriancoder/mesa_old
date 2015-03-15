/*
 * Copyright (c) 2014 Etnaviv Project
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
/* Low level emit functions */
#include "etnaviv_emit.h"

#include "common.xml.h"
#include "state.xml.h"

#include "etnaviv_pipe.h"
#include "etnaviv_rs.h"

/* Queue a STALL command (queues 2 words) */
static inline void CMD_STALL(struct etna_cmd_stream *stream, uint32_t from, uint32_t to)
{
	etna_cmd_stream_emit(stream, VIV_FE_STALL_HEADER_OP_STALL);
	etna_cmd_stream_emit(stream, VIV_FE_STALL_TOKEN_FROM(from) | VIV_FE_STALL_TOKEN_TO(to));
}

void etna_stall(struct etna_cmd_stream *stream, uint32_t from, uint32_t to)
{
	etna_cmd_stream_reserve(stream, 4);

	etna_emit_load_state(stream, VIVS_GL_SEMAPHORE_TOKEN>>2, 1, 0);
	etna_cmd_stream_emit(stream, VIVS_GL_SEMAPHORE_TOKEN_FROM(from) | VIVS_GL_SEMAPHORE_TOKEN_TO(to));

	if (from == SYNC_RECIPIENT_FE)
	{
		/* if the frontend is to be stalled, queue a STALL frontend command */
		CMD_STALL(stream, from, to);
	} else {
		/* otherwise, load the STALL token state */
		etna_emit_load_state(stream, VIVS_GL_STALL_TOKEN>>2, 1, 0);
		etna_cmd_stream_emit(stream, VIVS_GL_STALL_TOKEN_FROM(from) | VIVS_GL_STALL_TOKEN_TO(to));
	}
}

static void etna_emit_reloc(struct etna_cmd_stream *restrict stream, const struct etna_reloc *reloc)
{
	if (!reloc->bo)
		etna_cmd_stream_emit(stream, 0x0);
	else
		etna_cmd_stream_reloc(stream, reloc);
}

/* submit RS state, without any processing and no dependence on context
 * except TS if this is a source-to-destination blit. */
void etna_submit_rs_state(struct etna_context *restrict ctx, const struct compiled_rs_state *cs)
{
    struct etna_screen *restrict screen = etna_screen(ctx->base.screen);
    struct etna_cmd_stream *restrict stream = ctx->stream;

    if (screen->specs.pixel_pipes == 1)
    {
        etna_cmd_stream_reserve(stream, 22);
        /*0 */ etna_emit_load_state(stream, VIVS_RS_CONFIG>>2, 5, 0);
        /*1 */ etna_cmd_stream_emit(stream, cs->RS_CONFIG);
        /*2 */ etna_emit_reloc(stream, &cs->RS_SOURCE);
        /*3 */ etna_cmd_stream_emit(stream, cs->RS_SOURCE_STRIDE);
        /*4 */ etna_emit_reloc(stream, &cs->RS_DEST);
        /*5 */ etna_cmd_stream_emit(stream, cs->RS_DEST_STRIDE);
        /*6 */ etna_emit_load_state(stream, VIVS_RS_WINDOW_SIZE>>2, 1, 0);
        /*7 */ etna_cmd_stream_emit(stream, cs->RS_WINDOW_SIZE);
        /*8 */ etna_emit_load_state(stream, VIVS_RS_DITHER(0)>>2, 2, 0);
        /*9 */ etna_cmd_stream_emit(stream, cs->RS_DITHER[0]);
        /*10*/ etna_cmd_stream_emit(stream, cs->RS_DITHER[1]);
        /*11*/ etna_cmd_stream_emit(stream, 0xbabb1e); /* pad */
        /*12*/ etna_emit_load_state(stream, VIVS_RS_CLEAR_CONTROL>>2, 5, 0);
        /*13*/ etna_cmd_stream_emit(stream, cs->RS_CLEAR_CONTROL);
        /*14*/ etna_cmd_stream_emit(stream, cs->RS_FILL_VALUE[0]);
        /*15*/ etna_cmd_stream_emit(stream, cs->RS_FILL_VALUE[1]);
        /*16*/ etna_cmd_stream_emit(stream, cs->RS_FILL_VALUE[2]);
        /*17*/ etna_cmd_stream_emit(stream, cs->RS_FILL_VALUE[3]);
        /*18*/ etna_emit_load_state(stream, VIVS_RS_EXTRA_CONFIG>>2, 1, 0);
        /*19*/ etna_cmd_stream_emit(stream, cs->RS_EXTRA_CONFIG);
        /*20*/ etna_emit_load_state(stream, VIVS_RS_KICKER>>2, 1, 0);
        /*21*/ etna_cmd_stream_emit(stream, 0xbeebbeeb);
    }
    else if (screen->specs.pixel_pipes == 2)
    {
        etna_cmd_stream_reserve(stream, 34); /* worst case - both pipes multi=1 */
        /*0 */ etna_emit_load_state(stream, VIVS_RS_CONFIG>>2, 1, 0);
        /*1 */ etna_cmd_stream_emit(stream, cs->RS_CONFIG);
        /*2 */ etna_emit_load_state(stream, VIVS_RS_SOURCE_STRIDE>>2, 1, 0);
        /*3 */ etna_cmd_stream_emit(stream, cs->RS_SOURCE_STRIDE);
        /*4 */ etna_emit_load_state(stream, VIVS_RS_DEST_STRIDE>>2, 1, 0);
        /*5 */ etna_cmd_stream_emit(stream, cs->RS_DEST_STRIDE);
        if (cs->RS_SOURCE_STRIDE & VIVS_RS_SOURCE_STRIDE_MULTI)
        {
            /*6 */ etna_emit_load_state(stream, VIVS_RS_PIPE_SOURCE_ADDR(0)>>2, 2, 0);
            /*7 */ etna_emit_reloc(stream, &cs->RS_PIPE_SOURCE[0]);
            /*8 */ etna_emit_reloc(stream, &cs->RS_PIPE_SOURCE[1]);
            /*9 */ etna_cmd_stream_emit(stream, 0x00000000); /* pad */
        }
        else
        {
            /*6 */ etna_emit_load_state(stream, VIVS_RS_PIPE_SOURCE_ADDR(0)>>2, 1, 0);
            /*7 */ etna_emit_reloc(stream, &cs->RS_PIPE_SOURCE[0]);
        }
        if (cs->RS_DEST_STRIDE & VIVS_RS_DEST_STRIDE_MULTI)
        {
            /*10*/ etna_emit_load_state(stream, VIVS_RS_PIPE_DEST_ADDR(0)>>2, 2, 0);
            /*11*/ etna_emit_reloc(stream, &cs->RS_PIPE_DEST[0]);
            /*12*/ etna_emit_reloc(stream, &cs->RS_PIPE_DEST[1]);
            /*13*/ etna_cmd_stream_emit(stream, 0x00000000); /* pad */
        }
        else
        {
            /*10 */ etna_emit_load_state(stream, VIVS_RS_PIPE_DEST_ADDR(0)>>2, 1, 0);
            /*11 */ etna_emit_reloc(stream, &cs->RS_PIPE_DEST[0]);
        }
        /*14*/ etna_emit_load_state(stream, VIVS_RS_PIPE_OFFSET(0)>>2, 2, 0);
        /*15*/ etna_cmd_stream_emit(stream, cs->RS_PIPE_OFFSET[0]);
        /*16*/ etna_cmd_stream_emit(stream, cs->RS_PIPE_OFFSET[1]);
        /*17*/ etna_cmd_stream_emit(stream, 0x00000000); /* pad */
        /*18*/ etna_emit_load_state(stream, VIVS_RS_WINDOW_SIZE>>2, 1, 0);
        /*19*/ etna_cmd_stream_emit(stream, cs->RS_WINDOW_SIZE);
        /*20*/ etna_emit_load_state(stream, VIVS_RS_DITHER(0)>>2, 2, 0);
        /*21*/ etna_cmd_stream_emit(stream, cs->RS_DITHER[0]);
        /*22*/ etna_cmd_stream_emit(stream, cs->RS_DITHER[1]);
        /*23*/ etna_cmd_stream_emit(stream, 0xbabb1e); /* pad */
        /*24*/ etna_emit_load_state(stream, VIVS_RS_CLEAR_CONTROL>>2, 5, 0);
        /*25*/ etna_cmd_stream_emit(stream, cs->RS_CLEAR_CONTROL);
        /*26*/ etna_cmd_stream_emit(stream, cs->RS_FILL_VALUE[0]);
        /*27*/ etna_cmd_stream_emit(stream, cs->RS_FILL_VALUE[1]);
        /*28*/ etna_cmd_stream_emit(stream, cs->RS_FILL_VALUE[2]);
        /*29*/ etna_cmd_stream_emit(stream, cs->RS_FILL_VALUE[3]);
        /*30*/ etna_emit_load_state(stream, VIVS_RS_EXTRA_CONFIG>>2, 1, 0);
        /*31*/ etna_cmd_stream_emit(stream, cs->RS_EXTRA_CONFIG);
        /*32*/ etna_emit_load_state(stream, VIVS_RS_KICKER>>2, 1, 0);
        /*33*/ etna_cmd_stream_emit(stream, 0xbeebbeeb);
    }
}

void etna_coalesce_start(struct etna_cmd_stream *stream, struct etna_coalesce *coalesce,
        uint32_t max)
{
    etna_cmd_stream_reserve(stream, max);
    coalesce->start = etna_cmd_stream_offset(stream);
    coalesce->last_reg = 0;
    coalesce->last_fixp = 0;
}

static inline void check_coalsence(struct etna_cmd_stream *stream,
        struct etna_coalesce *coalesce, uint32_t reg, uint32_t fixp)
{
    if (coalesce->last_reg != 0)
    {
        if (((coalesce->last_reg + 4)!= reg) || (coalesce->last_fixp != fixp))
        {
            etna_coalesce_end(stream, coalesce);
            etna_emit_load_state(stream, reg >> 2, 0, fixp);
            coalesce->start = etna_cmd_stream_offset(stream);
        }
    } else {
        etna_emit_load_state(stream, reg >> 2, 0, fixp);
        coalesce->start = etna_cmd_stream_offset(stream);
    }

    coalesce->last_reg = reg;
    coalesce->last_fixp = fixp;
}

void etna_coalsence_emit(struct etna_cmd_stream *stream, struct etna_coalesce *coalesce,
        uint32_t reg, uint32_t value)
{
    check_coalsence(stream, coalesce, reg, 0);
    etna_cmd_stream_emit(stream, value);
}

void etna_coalsence_emit_fixp(struct etna_cmd_stream *stream, struct etna_coalesce *coalesce,
        uint32_t reg, uint32_t value)
{
    check_coalsence(stream, coalesce, reg, 1);
    etna_cmd_stream_emit(stream, value);
}

void etna_coalsence_emit_reloc(struct etna_cmd_stream *stream, struct etna_coalesce *coalesce,
        uint32_t reg, const struct etna_reloc *r)
{
    check_coalsence(stream, coalesce, reg, 0);
    etna_emit_reloc(stream, r);
}

void etna_coalesce_end(struct etna_cmd_stream *stream, struct etna_coalesce *coalesce)
{
    uint32_t end = etna_cmd_stream_offset(stream);
    uint32_t size = end - coalesce->start;

    if (size)
    {
        uint32_t offset = coalesce->start - 1;
        uint32_t value = etna_cmd_stream_get(stream, offset);

        value |= VIV_FE_LOAD_STATE_HEADER_COUNT(size);
        etna_cmd_stream_set(stream, offset, value);
    }

    /* append needed padding */
    if (end % 2 == 1)
        etna_cmd_stream_emit(stream, 0xdeadbeef);
}
