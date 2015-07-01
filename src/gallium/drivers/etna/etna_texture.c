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
/* Texture CSOs */
#include "etna_texture.h"

#include "etna_emit.h"
#include "etna_internal.h"
#include "etna_pipe.h"
#include "etna_translate.h"
#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"

#include "common.xml.h"
#include "state.xml.h"
#include "state_3d.xml.h"

static void *etna_pipe_create_sampler_state(struct pipe_context *pipe,
                              const struct pipe_sampler_state *ss)
{
    //struct etna_pipe_context *priv = etna_pipe_context(pipe);
    struct compiled_sampler_state *cs = CALLOC_STRUCT(compiled_sampler_state);
    cs->TE_SAMPLER_CONFIG0 =
                VIVS_TE_SAMPLER_CONFIG0_UWRAP(translate_texture_wrapmode(ss->wrap_s))|
                VIVS_TE_SAMPLER_CONFIG0_VWRAP(translate_texture_wrapmode(ss->wrap_t))|
                VIVS_TE_SAMPLER_CONFIG0_MIN(translate_texture_filter(ss->min_img_filter))|
                VIVS_TE_SAMPLER_CONFIG0_MIP(translate_texture_mipfilter(ss->min_mip_filter))|
                VIVS_TE_SAMPLER_CONFIG0_MAG(translate_texture_filter(ss->mag_img_filter));
    cs->TE_SAMPLER_CONFIG1 = 0; /* VIVS_TE_SAMPLER_CONFIG1 (swizzle, extended format) fully determined by sampler view */
    cs->TE_SAMPLER_LOD_CONFIG =
            (ss->lod_bias != 0.0 ? VIVS_TE_SAMPLER_LOD_CONFIG_BIAS_ENABLE : 0) |
            VIVS_TE_SAMPLER_LOD_CONFIG_BIAS(etna_float_to_fixp55(ss->lod_bias));
    if(ss->min_mip_filter != PIPE_TEX_MIPFILTER_NONE)
    {
        cs->min_lod = etna_float_to_fixp55(ss->min_lod);
        cs->max_lod = etna_float_to_fixp55(ss->max_lod);
    } else { /* when not mipmapping, we need to set max/min lod so that always lowest LOD is selected */
        cs->min_lod = cs->max_lod = etna_float_to_fixp55(ss->min_lod);
    }
    return cs;
}

static void etna_pipe_bind_sampler_states (struct pipe_context * pipe,
                                           unsigned shader, unsigned start_slot,
                                           unsigned num_samplers, void **samplers)
{
    /* bind fragment sampler */
    struct etna_context *priv = etna_context(pipe);
    struct etna_screen *screen = etna_screen(pipe->screen);
    priv->dirty_bits |= ETNA_STATE_SAMPLERS;
    priv->num_fragment_samplers = num_samplers;
    for(int idx=0; idx<num_samplers; ++idx)
    {
        priv->sampler_p[idx] = samplers[idx];
        if(samplers[idx])
            priv->sampler[idx] = *(struct compiled_sampler_state*)samplers[idx];
    }

    /* bind vertex sampler */
    for(int idx=0; idx<num_samplers; ++idx)
    {
        priv->sampler_p[screen->specs.vertex_sampler_offset + idx] = samplers[idx];
        if(samplers[idx])
            priv->sampler[screen->specs.vertex_sampler_offset + idx] = *(struct compiled_sampler_state*)samplers[idx];
    }
}

static void etna_pipe_delete_sampler_state(struct pipe_context *pipe, void *ss)
{
    //struct etna_pipe_context *priv = etna_pipe_context(pipe);
    FREE(ss);
}

static struct pipe_sampler_view *etna_pipe_create_sampler_view(struct pipe_context *pipe,
                                                 struct pipe_resource *texture,
                                                 const struct pipe_sampler_view *templat)
{
    struct etna_screen *screen = etna_screen(pipe->screen);
    struct etna_sampler_view *sv = CALLOC_STRUCT(etna_sampler_view);
    sv->base = *templat;
    sv->base.context = pipe;
    sv->base.texture = 0;
    pipe_resource_reference(&sv->base.texture, texture);
    sv->base.texture = texture;
    assert(sv->base.texture);

    struct compiled_sampler_view *cs = CALLOC_STRUCT(compiled_sampler_view);
    struct etna_resource *res = etna_resource(sv->base.texture);
    assert(res != NULL);

    cs->TE_SAMPLER_CONFIG0 =
                VIVS_TE_SAMPLER_CONFIG0_TYPE(translate_texture_target(res->base.target, false)) |
                VIVS_TE_SAMPLER_CONFIG0_FORMAT(translate_texture_format(sv->base.format, false));
                /* merged with sampler state */
    cs->TE_SAMPLER_CONFIG0_MASK = 0xffffffff;

    cs->TE_SAMPLER_CONFIG1 =
                VIVS_TE_SAMPLER_CONFIG1_SWIZZLE_R(templat->swizzle_r) |
                VIVS_TE_SAMPLER_CONFIG1_SWIZZLE_G(templat->swizzle_g) |
                VIVS_TE_SAMPLER_CONFIG1_SWIZZLE_B(templat->swizzle_b) |
                VIVS_TE_SAMPLER_CONFIG1_SWIZZLE_A(templat->swizzle_a) |
                VIVS_TE_SAMPLER_CONFIG1_HALIGN(res->halign);
    cs->TE_SAMPLER_SIZE =
            VIVS_TE_SAMPLER_SIZE_WIDTH(res->base.width0)|
            VIVS_TE_SAMPLER_SIZE_HEIGHT(res->base.height0);
    cs->TE_SAMPLER_LOG_SIZE =
            VIVS_TE_SAMPLER_LOG_SIZE_WIDTH(etna_log2_fixp55(res->base.width0)) |
            VIVS_TE_SAMPLER_LOG_SIZE_HEIGHT(etna_log2_fixp55(res->base.height0));

    /* Set up levels-of-detail */
    for(int lod=0; lod<=res->base.last_level; ++lod)
    {
        struct etna_reloc r = {
            .bo = res->bo,
            .offset = res->levels[lod].offset,
        };
        cs->TE_SAMPLER_LOD_ADDR[lod] = r;
    }
    cs->min_lod = sv->base.u.tex.first_level << 5;
    cs->max_lod = MIN2(sv->base.u.tex.last_level, res->base.last_level) << 5;

    /* Workaround for npot textures -- it appears that only CLAMP_TO_EDGE is supported when the
     * appropriate capability is not set. */
    if(!screen->specs.npot_tex_any_wrap &&
        (!util_is_power_of_two(res->base.width0) || !util_is_power_of_two(res->base.height0)))
    {
        cs->TE_SAMPLER_CONFIG0_MASK = ~(VIVS_TE_SAMPLER_CONFIG0_UWRAP__MASK |
                                        VIVS_TE_SAMPLER_CONFIG0_VWRAP__MASK);
        cs->TE_SAMPLER_CONFIG0 |= VIVS_TE_SAMPLER_CONFIG0_UWRAP(TEXTURE_WRAPMODE_CLAMP_TO_EDGE) |
                                  VIVS_TE_SAMPLER_CONFIG0_VWRAP(TEXTURE_WRAPMODE_CLAMP_TO_EDGE);
    }

    sv->internal = cs;
    pipe_reference_init(&sv->base.reference, 1);
    return &sv->base;
}

static void etna_pipe_sampler_view_destroy(struct pipe_context *pipe,
                            struct pipe_sampler_view *view)
{
    //struct etna_pipe_context *priv = etna_pipe_context(pipe);
    pipe_resource_reference(&view->texture, NULL);
    FREE(etna_sampler_view(view)->internal);
    FREE(view);
}


static void etna_pipe_set_sampler_views(struct pipe_context *pipe,
                                        unsigned shader,
                                        unsigned start_slot, unsigned num_views,
                                        struct pipe_sampler_view **info)
{
    /* fragment sampler */
    struct etna_context *priv = etna_context(pipe);
    struct etna_screen *screen = etna_screen(pipe->screen);
    unsigned idx;
    priv->dirty_bits |= ETNA_STATE_SAMPLER_VIEWS;
    priv->num_fragment_sampler_views = num_views;
    for(idx=0; idx<num_views; ++idx)
    {
        pipe_sampler_view_reference(&priv->sampler_view_s[idx], info[idx]);
        if(info[idx])
            priv->sampler_view[idx] = *etna_sampler_view(info[idx])->internal;
    }
    for(; idx<screen->specs.fragment_sampler_count; ++idx)
    {
        pipe_sampler_view_reference(&priv->sampler_view_s[idx], NULL);
    }

    /* vertex sampler */
    unsigned offset = screen->specs.vertex_sampler_offset;

    for(idx=0; idx<num_views; ++idx)
    {
        pipe_sampler_view_reference(&priv->sampler_view_s[offset + idx], info[idx]);
        if(info[idx])
            priv->sampler_view[offset + idx] = *etna_sampler_view(info[idx])->internal;
    }
    for(; idx<screen->specs.vertex_sampler_count; ++idx)
    {
        pipe_sampler_view_reference(&priv->sampler_view_s[offset + idx], NULL);
    }
}

static void etna_pipe_texture_barrier(struct pipe_context *pipe)
{
    struct etna_context *priv = etna_context(pipe);
    /* clear color and texture cache to make sure that texture unit reads
     * what has been written
     */
    etna_set_state(priv->stream, VIVS_GL_FLUSH_CACHE, VIVS_GL_FLUSH_CACHE_COLOR | VIVS_GL_FLUSH_CACHE_TEXTURE);
}

void etna_pipe_texture_init(struct pipe_context *pc)
{
    pc->create_sampler_state = etna_pipe_create_sampler_state;
    pc->bind_sampler_states = etna_pipe_bind_sampler_states;
    /* XXX bind_geometry_sampler_states */
    /* XXX bind_compute_sampler_states */
    pc->delete_sampler_state = etna_pipe_delete_sampler_state;
    pc->set_sampler_views = etna_pipe_set_sampler_views;
    /* XXX set_geometry_sampler_views */
    /* XXX set_compute_sampler_views */
    /* XXX set_shader_resources */
    pc->create_sampler_view = etna_pipe_create_sampler_view;
    pc->sampler_view_destroy = etna_pipe_sampler_view_destroy;
    pc->texture_barrier = etna_pipe_texture_barrier;
}

