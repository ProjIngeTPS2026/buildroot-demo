################################################################################
#
# apping-roc-toolkit-opus
#
################################################################################

APPING_ROC_TOOLKIT_OPUS_VERSION = local
APPING_ROC_TOOLKIT_OPUS_SITE = $(BR2_EXTERNAL_APPING_PATH)/../roc-toolkit-opus-master
APPING_ROC_TOOLKIT_OPUS_SITE_METHOD = local
APPING_ROC_TOOLKIT_OPUS_LICENSE = MPL-2.0
APPING_ROC_TOOLKIT_OPUS_LICENSE_FILES = LICENSE
APPING_ROC_TOOLKIT_OPUS_DEPENDENCIES = host-gengetopt host-pkgconf host-ragel host-scons alsa-lib libsndfile libuv opus pulseaudio speexdsp

APPING_ROC_TOOLKIT_OPUS_SCONS_OPTS = \
	-Q \
	--host=$(GNU_TARGET_NAME) \
	--build=$(GNU_HOST_NAME) \
	--prefix=/usr \
	--disable-openfec \
	--disable-openssl \
	--disable-libunwind \
	--disable-sox

define APPING_ROC_TOOLKIT_OPUS_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) \
		$(HOST_DIR)/bin/scons -C $(@D) $(APPING_ROC_TOOLKIT_OPUS_SCONS_OPTS) tools
endef

define APPING_ROC_TOOLKIT_OPUS_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/bin/$(GNU_TARGET_NAME)/roc-send \
		$(TARGET_DIR)/usr/bin/roc-send
	$(INSTALL) -D -m 0755 $(@D)/bin/$(GNU_TARGET_NAME)/roc-recv \
		$(TARGET_DIR)/usr/bin/roc-recv
endef

$(eval $(generic-package))
