/*
 * Copyright (c) 2015 Etnaviv Project
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
 *
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <err.h>

#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_text.h"
#include "tgsi/tgsi_dump.h"

#include "etnaviv_internal.h"
#include "etnaviv_debug.h"
#include "etnaviv_compiler.h"

static const struct etna_pipe_specs specs_gc2000 = {
	.vs_need_z_div = 0,
	.has_sin_cos_sqrt = 1,
	.vertex_sampler_offset = 8,
	.vertex_output_buffer_size = 512,
	.vertex_cache_size = 16,
	.shader_core_count = 4,
	.max_instructions = 512,
	.max_varyings = 12,
	.max_registers = 64,
	.max_vs_uniforms = 168,
	.max_ps_uniforms = 128,
};

static int
read_file(const char *filename, void **ptr, size_t *size)
{
	int fd, ret;
	struct stat st;

	*ptr = MAP_FAILED;

	fd = open(filename, O_RDONLY);
	if (fd == -1) {
		warnx("couldn't open `%s'", filename);
		return 1;
	}

	ret = fstat(fd, &st);
	if (ret)
		errx(1, "couldn't stat `%s'", filename);

	*size = st.st_size;
	*ptr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (*ptr == MAP_FAILED)
		errx(1, "couldn't map `%s'", filename);

	close(fd);

	return 0;
}

static void print_usage(void)
{
	printf("Usage: etnaviv_compiler [OPTIONS]... FILE\n");
	printf("    --verbose         - verbose compiler/debug messages\n");
	printf("    --help            - show this message\n");
}

int main(int argc, char **argv)
{
	int ret = 0, n = 1;
	bool success;
	const char *filename;
	struct tgsi_token toks[65536];
	struct tgsi_parse_context parse;
	struct etna_shader_object *shader_obj = NULL;
	void *ptr;
	size_t size;

	etna_mesa_debug = ETNA_DBG_MSGS;

	/* cmdline args which impact shader variant get spit out in a
	 * comment on the first line..  a quick/dirty way to preserve
	 * that info so when ir3test recompiles the shader with a new
	 * compiler version, we use the same shader-key settings:
	 */
	debug_printf("; options:");

	while (n < argc) {
		if (!strcmp(argv[n], "--verbose")) {
			etna_mesa_debug |=  ETNA_DBG_COMPILER_MSGS;
			n++;
			continue;
		}

		if (!strcmp(argv[n], "--help")) {
			print_usage();
			return 0;
		}

		break;
	}
	debug_printf("\n");

	filename = argv[n];

	ret = read_file(filename, &ptr, &size);
	if (ret) {
		print_usage();
		return ret;
	}

	if (!tgsi_text_translate(ptr, toks, ARRAY_SIZE(toks)))
		errx(1, "could not parse `%s'", filename);

	tgsi_parse_init(&parse, toks);

	success = etna_compile_shader_object(&specs_gc2000, toks, &shader_obj);

	if (success == false) {
		fprintf(stderr, "compiler failed!\n");
		return 1;
	}

	etna_dump_shader_object(shader_obj);
	etna_destroy_shader_object(shader_obj);
}
