#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/u_format.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"

#include "etnaviv_drm_public.h"

#include "etnaviv/etnaviv_screen.h"

struct pipe_screen *
etna_drm_screen_create(int fd)
{
	struct etna_device *dev = etna_device_new(fd);
	if (!dev)
		return NULL;
    return etna_screen_create(dev);
}
