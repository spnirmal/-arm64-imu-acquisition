BR2_DIR ?= $(HOME)/Documents/projects/buildroot_clone_dir/buildroot
ifeq ($(wildcard $(BR2_DIR)),)
BR2_DIR := /home/sp/Documents/projects/buildroot_clone_dir/buildroot
endif
EXT_DIR := $(CURDIR)/buildroot-external
CONFIG_DIR := $(CURDIR)/configurations
OUTPUT_DIR := $(CURDIR)/output

ARCH_A_OUT := $(OUTPUT_DIR)/arch-a
ARCH_B_OUT := $(OUTPUT_DIR)/arch-b
ARCH_C_OUT := $(OUTPUT_DIR)/arch-c

.PHONY: arch-a arch-A clean-a clean-A savedefconfig-a menuconfig-a \
        arch-b arch-B clean-b clean-B savedefconfig-b menuconfig-b \
        arch-c arch-C clean-c clean-C savedefconfig-c menuconfig-c \
        clean

.DEFAULT_GOAL := arch-a

# ==============================================================================
# Architecture A: Userspace SPI driver (spidev + libgpiod)
# ==============================================================================
arch-a:
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_A_OUT) \
		BR2_EXTERNAL=$(EXT_DIR) \
		BR2_DEFCONFIG=$(CONFIG_DIR)/arch-a_defconfig \
		defconfig
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_A_OUT) \
		BR2_EXTERNAL=$(EXT_DIR)

arch-A: arch-a

menuconfig-a:
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_A_OUT) \
		BR2_EXTERNAL=$(EXT_DIR) \
		menuconfig

savedefconfig-a:
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_A_OUT) \
		BR2_EXTERNAL=$(EXT_DIR) \
		BR2_DEFCONFIG=$(CONFIG_DIR)/arch-a_defconfig \
		savedefconfig

clean-a:
	@if [ -d "$(ARCH_A_OUT)" ]; then \
		$(MAKE) -C $(BR2_DIR) O=$(ARCH_A_OUT) clean; \
	fi

clean-A: clean-a

# ==============================================================================
# Architecture B: Kernel-space IIO driver (inv-mpu6050-spi)
# ==============================================================================
arch-b:
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_B_OUT) \
		BR2_EXTERNAL=$(EXT_DIR) \
		BR2_DEFCONFIG=$(CONFIG_DIR)/arch-b_defconfig \
		defconfig
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_B_OUT) \
		BR2_EXTERNAL=$(EXT_DIR)

arch-B: arch-b

menuconfig-b:
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_B_OUT) \
		BR2_EXTERNAL=$(EXT_DIR) \
		menuconfig

savedefconfig-b:
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_B_OUT) \
		BR2_EXTERNAL=$(EXT_DIR) \
		BR2_DEFCONFIG=$(CONFIG_DIR)/arch-b_defconfig \
		savedefconfig

clean-b:
	@if [ -d "$(ARCH_B_OUT)" ]; then \
		$(MAKE) -C $(BR2_DIR) O=$(ARCH_B_OUT) clean; \
	fi

clean-B: clean-b

# ==============================================================================
# Architecture C: Custom Zero-Copy SPI Driver (DMA / SPSC Ring Buffer)
# ==============================================================================
arch-c:
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_C_OUT) \
		BR2_EXTERNAL=$(EXT_DIR) \
		BR2_DEFCONFIG=$(CONFIG_DIR)/arch-c_defconfig \
		defconfig
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_C_OUT) \
		BR2_EXTERNAL=$(EXT_DIR)

arch-C: arch-c

menuconfig-c:
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_C_OUT) \
		BR2_EXTERNAL=$(EXT_DIR) \
		menuconfig

savedefconfig-c:
	$(MAKE) -C $(BR2_DIR) \
		O=$(ARCH_C_OUT) \
		BR2_EXTERNAL=$(EXT_DIR) \
		BR2_DEFCONFIG=$(CONFIG_DIR)/arch-c_defconfig \
		savedefconfig

clean-c:
	@if [ -d "$(ARCH_C_OUT)" ]; then \
		$(MAKE) -C $(BR2_DIR) O=$(ARCH_C_OUT) clean; \
	fi

clean-C: clean-c

# ==============================================================================
# Global Clean
# ==============================================================================
clean: clean-a clean-b clean-c
