################################################################################
#
# imu-zerocopy
#
################################################################################

IMU_ZEROCOPY_VERSION = 1.0.0
IMU_ZEROCOPY_SITE = $(BR2_EXTERNAL_DRIVERLAB_PATH)/../applications/zerocopy-userspace-acquisition
IMU_ZEROCOPY_SITE_METHOD = local
IMU_ZEROCOPY_LICENSE = GPL-2.0

$(eval $(kernel-module))
$(eval $(generic-package))
