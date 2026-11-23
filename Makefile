BR2_DIR ?= $(HOME)/Documents/projects/buildroot_clone_dir/buildroot
ifeq ($(wildcard $(BR2_DIR)),)
BR2_DIR := /home/sp/Documents/projects/buildroot_clone_dir/buildroot
endif
EXT_DIR := $(CURDIR)/buildroot-external
CONFIG_DIR := $(CURDIR)/configurations
OUTPUT_DIR := $(CURDIR)/output

ARCH_A_OUT := $(OUTPUT_DIR)/arch-a

.PHONY: arch-a arch-A clean-a clean-A clean savedefconfig-a menuconfig-a

.DEFAULT_GOAL := arch-a

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

clean: clean-a
