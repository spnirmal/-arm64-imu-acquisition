################################################################################
# IIO Userspace Acquisition
################################################################################

IIO_USERSPACE_ACQUISITION_VERSION = 1.0.0
IIO_USERSPACE_ACQUISITION_SITE = $(BR2_EXTERNAL_DRIVERLAB_PATH)/../applications/iio-userspace-acquisition
IIO_USERSPACE_ACQUISITION_SITE_METHOD = local
IIO_USERSPACE_ACQUISITION_SUBDIR = src

$(eval $(cmake-package))
