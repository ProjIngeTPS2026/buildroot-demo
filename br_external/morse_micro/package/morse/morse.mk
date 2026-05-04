################################################################################
#
# morse micro driver
#
################################################################################

MORSE_VERSION = main #1.17.8
MORSE_SITE = https://github.com/MorseMicro/morse_driver.git
MORSE_SITE_METHOD = git

MORSE_GIT_SUBMODULES = YES
MORSE_GIT_SUBMODULES_RECURSIVE = YES

MORSE_LICENSE = GPL-2.0
MORSE_LICENSE_FILES = LICENSE

MORSE_DEPENDENCIES = linux

# Options du driver (TODO : mettre des options selectionnables dans menuconfig buildroot)
MORSE_MODULE_MAKE_OPTS = \
    KERNEL_SRC=$(LINUX_DIR) \
    CONFIG_WLAN_VENDOR_MORSE=m \
    CONFIG_MORSE_USER_ACCESS=y \
    CONFIG_MORSE_VENDOR_COMMAND=y \
    CONFIG_MORSE_SDIO=y \
    CONFIG_MORSE_SPI=y \
    EXTRA_CFLAGS="-I$(@D)/mmrc-submodule/src/core"

################################################################################
# Install ALL firmware files
################################################################################

define MORSE_INSTALL_FIRMWARE
    mkdir -p $(TARGET_DIR)/lib/firmware/morse
    find $(BR2_EXTERNAL_MORSE_EXTERNAL_PATH)/firmware -type f | while read f; do \
        $(INSTALL) -D -m 0644 $$f \
            $(TARGET_DIR)/lib/firmware/morse/$$(basename $$f); \
    done
endef

MORSE_POST_INSTALL_TARGET_HOOKS += MORSE_INSTALL_FIRMWARE

# Autoload modules au boot
define MORSE_INSTALL_INIT
    mkdir -p $(TARGET_DIR)/etc/modules-load.d
    echo "morse" > $(TARGET_DIR)/etc/modules-load.d/morse.conf
    echo "dot11ah" >> $(TARGET_DIR)/etc/modules-load.d/morse.conf
endef

MORSE_POST_INSTALL_TARGET_HOOKS += MORSE_INSTALL_INIT

$(eval $(kernel-module))
$(eval $(generic-package))
