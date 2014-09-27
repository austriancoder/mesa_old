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
#ifndef H_ETNA_EMIT
#define H_ETNA_EMIT

#include "cmdstream.xml.h"
#include "etna_screen.h"

struct etna_context;
struct compiled_rs_state;

static inline void etna_emit_load_state(struct etna_cmd_stream *stream,
		const uint16_t offset, const uint16_t count, const int fixp)
{
	uint32_t v;

	v = (VIV_FE_LOAD_STATE_HEADER_OP_LOAD_STATE | ((fixp)?VIV_FE_LOAD_STATE_HEADER_FIXP:0) |
		VIV_FE_LOAD_STATE_HEADER_OFFSET(offset) |
		(VIV_FE_LOAD_STATE_HEADER_COUNT(count) & VIV_FE_LOAD_STATE_HEADER_COUNT__MASK));

	etna_cmd_stream_emit(stream, v);
}

static inline void etna_set_state(struct etna_cmd_stream *stream, uint32_t address, uint32_t value)
{
	etna_cmd_stream_reserve(stream, 2);
	etna_emit_load_state(stream, address >> 2, 1, 0);
	etna_cmd_stream_emit(stream, value);
}

static inline void etna_set_state_multi(struct etna_cmd_stream *stream, uint32_t base, uint32_t num, const uint32_t *values)
{
    if (num == 0)
    	return;

    etna_cmd_stream_reserve(stream, 1 + num + 1); /* 1 extra for potential alignment */
    etna_emit_load_state(stream, base >> 2, num, 0);

    for (uint32_t i = 0; i < num; i++)
    {
    	etna_cmd_stream_emit(stream, values[i]);
    }
}

void etna_stall(struct etna_cmd_stream *stream, uint32_t from, uint32_t to);

static inline void etna_draw_primitives(struct etna_cmd_stream *stream, uint32_t primitive_type, uint32_t start, uint32_t count)
{
    etna_cmd_stream_reserve(stream, 4);

    etna_cmd_stream_emit(stream, VIV_FE_DRAW_PRIMITIVES_HEADER_OP_DRAW_PRIMITIVES);
    etna_cmd_stream_emit(stream, primitive_type);
    etna_cmd_stream_emit(stream, start);
    etna_cmd_stream_emit(stream, count);
}
static inline void etna_draw_indexed_primitives(struct etna_cmd_stream *stream, uint32_t primitive_type, uint32_t start, uint32_t count, uint32_t offset)
{
    etna_cmd_stream_reserve(stream, 5+1);

    etna_cmd_stream_emit(stream, VIV_FE_DRAW_INDEXED_PRIMITIVES_HEADER_OP_DRAW_INDEXED_PRIMITIVES);
    etna_cmd_stream_emit(stream, primitive_type);
    etna_cmd_stream_emit(stream, start);
    etna_cmd_stream_emit(stream, count);
    etna_cmd_stream_emit(stream, offset);
    etna_cmd_stream_emit(stream, 0);
}


/* ETNA_COALESCE
 *
 * Mechanism to emit state changes and join consecutive
 * state updates into single SET_STATE commands when possible.
 *
 * Usage:
 * - Set state words with ETNA_COALESCE_STATE_UPDATE.
 *
 * - Before starting the state update, reserve space using EMIT_STATE_OPEN(max_updates),
 *   where max_updates is the maximum number of possible updates that will be emitted between
 *   this ETNA_COALESCE_STATE_OPEN .. ETNA_COALESCE_STATE_CLOSE pair.
 *
 * - When done with updating, call ETNA_COALESCE_STATE_CLOSE.
 *
 * In the scope where these macros are used, define the variables
 *     uint32_t last_reg,  -> last register to write to
 *       last_fixp,   -> fixp conversion flag for last written register
 *       span_start   -> start of span in command buffer
 * for state tracking.
 *
 * It works by keeping track of the last register that was written to plus one,
 * thus the next register that will be written. If the register number to be written
 * matches this next register, add it to the current span. If not, close the span
 * and open a new one.
 */

/* TODO */
#define ETNA_COALESCE_STATE_OPEN(max_updates) \
		span_start = 0; last_fixp = 0; last_reg = 0;

#define ETNA_COALESCE_STATE_CLOSE() \
		span_start = span_start; last_fixp = last_fixp; last_reg = last_reg;

#define ETNA_COALESCE_STATE_UPDATE(state_name, src_value, fixp) \
		etna_emit_load_state(stream, (VIVS_##state_name) >> 2, 1, (fixp));

#endif
