
#include "target-helpers/inline_debug_helper.h"
#include "state_tracker/drm_driver.h"
#include "etna/drm/etna_drm_public.h"

static struct pipe_screen *
create_screen(int fd)
{
   struct pipe_screen *screen;

   screen = etna_drm_screen_create(fd);
   if (!screen)
      return NULL;

   screen = debug_screen_wrap(screen);

   return screen;
}

PUBLIC
DRM_DRIVER_DESCRIPTOR("etnaviv", "etnaviv", create_screen, NULL)
