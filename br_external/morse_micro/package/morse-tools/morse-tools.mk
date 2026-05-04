################################################################################
#
# Morse Micro S1G hostap/wpa_supplicant
#
################################################################################

MORSE_TOOLS_VERSION = 1.17.8
MORSE_TOOLS_SITE = https://github.com/MorseMicro/hostap.git
MORSE_TOOLS_SITE_METHOD = git
MORSE_TOOLS_LICENSE = BSD-3-Clause
MORSE_TOOLS_LICENSE_FILES = README

MORSE_TOOLS_DEPENDENCIES = host-pkgconf libnl libopenssl morse-cli

MORSE_TOOLS_MORSE_VERSION = rel_1_17_8_2026_Mar_24
MORSE_TOOLS_COMMON_CFLAGS = \
	$(TARGET_CFLAGS) \
	-Wno-error=sign-compare \
	-Wno-error=deprecated-declarations \
	-I$(STAGING_DIR)/usr/include/libnl3 \
	`$(PKG_CONFIG_HOST_BINARY) --cflags libnl-3.0 libnl-genl-3.0 libnl-route-3.0 openssl`
MORSE_TOOLS_COMMON_LIBS = \
	`$(PKG_CONFIG_HOST_BINARY) --libs libnl-3.0 libnl-genl-3.0 libnl-route-3.0 openssl` \
	-lm

ifeq ($(BR2_STATIC_LIBS),y)
MORSE_TOOLS_COMMON_LIBS += -lnl-3 -lm -lpthread
endif

define MORSE_TOOLS_ENABLE_CONFIG
	for s in $(2); do \
		if grep -q "^#$$s" $(1); then \
			$(SED) "s/^#\($$s\)/\1/" $(1); \
		elif ! grep -q "^$$s" $(1); then \
			echo "$$s=y" >> $(1); \
		fi; \
	done
endef

define MORSE_TOOLS_DISABLE_CONFIG
	for s in $(2); do \
		if grep -q "^$$s" $(1); then \
			$(SED) "s/^\($$s\)/#\1/" $(1); \
		fi; \
	done
endef

define MORSE_TOOLS_SET_CONFIG
	if grep -q "^#$(2)=" $(1); then \
		$(SED) "s|^#\($(2)=\).*|\1$(3)|" $(1); \
	elif grep -q "^$(2)=" $(1); then \
		$(SED) "s|^\($(2)=\).*|\1$(3)|" $(1); \
	else \
		echo "$(2)=$(3)" >> $(1); \
	fi
endef

define MORSE_TOOLS_CONFIGURE_CMDS
	cp $(@D)/wpa_supplicant/defconfig $(@D)/wpa_supplicant/.config
	cp $(@D)/hostapd/defconfig $(@D)/hostapd/.config
	$(call MORSE_TOOLS_ENABLE_CONFIG,$(@D)/wpa_supplicant/.config, \
		CONFIG_DRIVER_NL80211 CONFIG_LIBNL32 CONFIG_CTRL_IFACE \
		CONFIG_AP CONFIG_MESH CONFIG_SAE CONFIG_DPP)
	$(call MORSE_TOOLS_DISABLE_CONFIG,$(@D)/wpa_supplicant/.config, \
		CONFIG_CTRL_IFACE_DBUS_NEW CONFIG_CTRL_IFACE_DBUS_INTRO)
	$(call MORSE_TOOLS_SET_CONFIG,$(@D)/wpa_supplicant/.config,CONFIG_TLS,openssl)
	$(call MORSE_TOOLS_ENABLE_CONFIG,$(@D)/hostapd/.config, \
		CONFIG_DRIVER_NL80211 CONFIG_LIBNL32 CONFIG_IEEE80211AH \
		CONFIG_CTRL_IFACE CONFIG_SAE CONFIG_DPP)
	$(call MORSE_TOOLS_SET_CONFIG,$(@D)/hostapd/.config,CONFIG_TLS,openssl)
endef

define MORSE_TOOLS_BUILD_WPA_SUPPLICANT
	$(TARGET_MAKE_ENV) \
		CFLAGS="$(MORSE_TOOLS_COMMON_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D)/wpa_supplicant \
		CC="$(TARGET_CC)" \
		LIBS="$(MORSE_TOOLS_COMMON_LIBS)" \
		LIBS_c="$(MORSE_TOOLS_COMMON_LIBS)" \
		LIBS_p="$(MORSE_TOOLS_COMMON_LIBS)" \
		BINDIR=/usr/sbin \
		MORSE_VERSION="$(MORSE_TOOLS_MORSE_VERSION)"
endef

define MORSE_TOOLS_BUILD_HOSTAPD
	$(TARGET_MAKE_ENV) \
		CFLAGS="$(MORSE_TOOLS_COMMON_CFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		$(MAKE) -C $(@D)/hostapd \
		CC="$(TARGET_CC)" \
		LIBS="$(MORSE_TOOLS_COMMON_LIBS)" \
		LIBS_c="$(MORSE_TOOLS_COMMON_LIBS)" \
		BINDIR=/usr/sbin \
		MORSE_VERSION="$(MORSE_TOOLS_MORSE_VERSION)"
endef

define MORSE_TOOLS_BUILD_CMDS
	$(MORSE_TOOLS_BUILD_WPA_SUPPLICANT)
	$(MORSE_TOOLS_BUILD_HOSTAPD)
endef

define MORSE_TOOLS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/wpa_supplicant/wpa_supplicant_s1g \
		$(TARGET_DIR)/usr/sbin/wpa_supplicant_s1g
	$(INSTALL) -D -m 0755 $(@D)/wpa_supplicant/wpa_cli_s1g \
		$(TARGET_DIR)/usr/sbin/wpa_cli_s1g
	$(INSTALL) -D -m 0755 $(@D)/wpa_supplicant/wpa_passphrase_s1g \
		$(TARGET_DIR)/usr/sbin/wpa_passphrase_s1g
	$(INSTALL) -D -m 0755 $(@D)/hostapd/hostapd_s1g \
		$(TARGET_DIR)/usr/sbin/hostapd_s1g
	$(INSTALL) -D -m 0755 $(@D)/hostapd/hostapd_cli_s1g \
		$(TARGET_DIR)/usr/bin/hostapd_cli_s1g
	$(INSTALL) -D -m 0644 $(@D)/hostapd/hostapd_s1g.conf \
		$(TARGET_DIR)/etc/hostapd_s1g.conf
endef

$(eval $(generic-package))
