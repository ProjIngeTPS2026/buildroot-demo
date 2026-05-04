################################################################################
#
# apping-fixed-base
#
################################################################################

APPING_FIXED_BASE_VERSION = local
APPING_FIXED_BASE_SITE = $(BR2_EXTERNAL_APPING_PATH)/..
APPING_FIXED_BASE_SITE_METHOD = local
APPING_FIXED_BASE_LICENSE = Proprietary
APPING_FIXED_BASE_DEPENDENCIES = apping-roc-toolkit-opus qt5base qt5multimedia gst1-plugins-base gst1-plugins-good
APPING_FIXED_BASE_CONF_OPTS = -DCMAKE_BUILD_TYPE=Release
APPING_FIXED_BASE_BUILD_OPTS = --target fixed-base-service
APPING_FIXED_BASE_SUPPORTS_IN_SOURCE_BUILD = NO
APPING_FIXED_BASE_BINARY = $(@D)/buildroot-build/fixed-base-service
APPING_FIXED_BASE_IN_SOURCE_BINARY = $(@D)/fixed-base-service

define APPING_FIXED_BASE_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 \
		$(or $(wildcard $(APPING_FIXED_BASE_BINARY)),$(APPING_FIXED_BASE_IN_SOURCE_BINARY)) \
		$(TARGET_DIR)/usr/bin/fixed-base-service
	rm -f $(TARGET_DIR)/etc/default/apping-fixed-base
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_APPING_PATH)/package/apping-fixed-base/base.json \
		$(TARGET_DIR)/etc/apping/base.json
	$(SED) 's|@APPING_ROC_MULTICAST_IFACE@|$(call qstrip,$(BR2_PACKAGE_APPING_ROC_MULTICAST_IFACE))|' \
		$(TARGET_DIR)/etc/apping/base.json
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_APPING_PATH)/package/apping-fixed-base/S80apping-fixed-base \
		$(TARGET_DIR)/etc/init.d/S80apping-fixed-base
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/var/lib/apping/base/library
endef

$(eval $(cmake-package))
