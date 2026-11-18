################################################################################
# SPI Userspace Acquisition
################################################################################

SPI_USERSPACE_ACQUISITION_VERSION = 1.0
SPI_USERSPACE_ACQUISITION_SITE = $(BR2_EXTERNAL_DRIVERLAB_PATH)/../applciations/spi-userspace-acquisition
SPI_USERSPACE_ACQUISITION_SITE_METHOD = local

define SPI_USERSPACE_ACQUISITION_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CXX="$(TARGET_CXX)"
endef

define SPI_USERSPACE_ACQUISITION_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/spi-userspace-acquisition \
		$(TARGET_DIR)/usr/bin/spi-userspace-acquisition
endef

$(eval $(generic-package))
