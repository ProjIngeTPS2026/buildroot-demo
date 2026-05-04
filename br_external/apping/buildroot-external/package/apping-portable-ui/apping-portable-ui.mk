################################################################################
#
# apping-portable-ui
#
################################################################################

APPING_PORTABLE_UI_VERSION = local
APPING_PORTABLE_UI_SITE = $(BR2_EXTERNAL_APPING_PATH)/..
APPING_PORTABLE_UI_SITE_METHOD = local
APPING_PORTABLE_UI_LICENSE = Proprietary
APPING_PORTABLE_UI_DEPENDENCIES = apping-roc-toolkit-opus qt5base qt5multimedia gst1-plugins-base gst1-plugins-good
APPING_PORTABLE_UI_CONF_OPTS = -DCMAKE_BUILD_TYPE=Release
APPING_PORTABLE_UI_BUILD_OPTS = --target portable-console
APPING_PORTABLE_UI_SUPPORTS_IN_SOURCE_BUILD = NO
APPING_PORTABLE_UI_BINARY = $(@D)/buildroot-build/portable-console
APPING_PORTABLE_UI_IN_SOURCE_BINARY = $(@D)/portable-console

define APPING_PORTABLE_UI_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 \
		$(or $(wildcard $(APPING_PORTABLE_UI_BINARY)),$(APPING_PORTABLE_UI_IN_SOURCE_BINARY)) \
		$(TARGET_DIR)/usr/bin/portable-console
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_APPING_PATH)/package/apping-portable-ui/apping-portable-ui \
		$(TARGET_DIR)/usr/bin/apping-portable-ui
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_APPING_PATH)/package/apping-portable-ui/portable-console.json \
		$(TARGET_DIR)/etc/apping/portable-console.json
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_APPING_PATH)/package/apping-portable-ui/apping-portable-ui.default \
		$(TARGET_DIR)/etc/default/apping-portable-ui
	$(SED) 's|@APPING_ROC_MULTICAST_IFACE@|$(call qstrip,$(BR2_PACKAGE_APPING_ROC_MULTICAST_IFACE))|' \
		$(TARGET_DIR)/etc/default/apping-portable-ui
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/usr/share/apping/assets
	cp -a $(@D)/assets/map $(TARGET_DIR)/usr/share/apping/assets/
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/var/lib/apping/portable
endef

$(eval $(cmake-package))
