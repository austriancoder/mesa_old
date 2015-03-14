if HAVE_GALLIUM_ETNA

TARGET_DRIVERS += etna
TARGET_CPPFLAGS += -DGALLIUM_ETNA
TARGET_LIB_DEPS += \
	$(top_builddir)/src/gallium/winsys/etna/drm/libetnadrm.la \
	$(top_builddir)/src/gallium/drivers/etna/libetna.la \
	$(ETNA_LIBS) \
	$(LIBDRM_LIBS)

endif
