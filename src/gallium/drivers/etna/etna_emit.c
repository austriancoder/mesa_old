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
#include "etna_emit.h"

#include "common.xml.h"
#include "state.xml.h"

#include "etna_pipe.h"
#include "etna_rs.h"

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
