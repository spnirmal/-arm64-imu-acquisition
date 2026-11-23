################################################################################
# SPI Userspace Acquisition
################################################################################

SPI_USERSPACE_ACQUISITION_VERSION = 1.0.0
SPI_USERSPACE_ACQUISITION_SITE = $(BR2_EXTERNAL_DRIVERLAB_PATH)/../applications/spi-userspace-acquisition
SPI_USERSPACE_ACQUISITION_SITE_METHOD = local
SPI_USERSPACE_ACQUISITION_SUBDIR = src
SPI_USERSPACE_ACQUISITION_DEPENDENCIES = libgpiod

$(eval $(cmake-package))
