################################################################################
#
# zerocopy-userspace-acquisition
#
################################################################################

ZEROCOPY_USERSPACE_ACQUISITION_VERSION = 1.0.0
ZEROCOPY_USERSPACE_ACQUISITION_SITE = $(BR2_EXTERNAL_DRIVERLAB_PATH)/../applications/zerocopy-userspace-acquisition
ZEROCOPY_USERSPACE_ACQUISITION_SITE_METHOD = local
ZEROCOPY_USERSPACE_ACQUISITION_DEPENDENCIES = imu-zerocopy

$(eval $(cmake-package))
