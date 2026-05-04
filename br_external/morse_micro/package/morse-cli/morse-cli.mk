################################################################################
#
# Morse Micro CLI
#
################################################################################

MORSE_CLI_VERSION = 1.17.8
MORSE_CLI_SITE = https://github.com/MorseMicro/morse_cli.git
MORSE_CLI_SITE_METHOD = git

MORSE_CLI_DEPENDENCIES = host-pkgconf libnl libusb

define MORSE_CLI_FIX_INCLUDE
	$(SED) 's|-I/usr/include/libusb-1.0||g' $(@D)/Makefile
endef

MORSE_CLI_POST_PATCH_HOOKS += MORSE_CLI_FIX_INCLUDE

MORSE_CLI_CFLAGS += -Wno-error=unused-result
MORSE_CLI_CFLAGS += -Wno-error=maybe-uninitialized

define MORSE_CLI_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" \
		AR="$(TARGET_AR)" \
		LD="$(TARGET_LD)" \
		CONFIG_MORSE_TRANS_NL80211=1 \
		CFLAGS="$(TARGET_CFLAGS) $(MORSE_CLI_CFLAGS) \
			$$( $(PKG_CONFIG_HOST_BINARY) --cflags libnl-3.0 libnl-genl-3.0 libusb-1.0 )" \
		LDFLAGS="$(TARGET_LDFLAGS) \
			$$( $(PKG_CONFIG_HOST_BINARY) --libs libnl-3.0 libnl-genl-3.0 libusb-1.0 )"
endef

define MORSE_CLI_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/morse_cli \
		$(TARGET_DIR)/usr/bin/morse_cli
endef

$(eval $(generic-package))
