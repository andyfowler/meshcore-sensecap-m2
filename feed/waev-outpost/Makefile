include $(TOPDIR)/rules.mk

PKG_NAME:=waev-outpost
PKG_VERSION:=0.9.416
PKG_RELEASE:=1
PKG_SOURCE:=waev_outpost_plugin-$(PKG_VERSION)-py3-none-any.whl
PKG_SOURCE_URL:=https://github.com/Treehouse-00/waev-outpost-plugin/releases/download/v$(PKG_VERSION)/
PKG_HASH:=1d27a4c9372f5104626fafcd84d5d212a6ca88978630e5ca5a54ca88d0faeaef
PKG_LICENSE:=MIT
PKG_MAINTAINER:=meshcore-m2

include $(INCLUDE_DIR)/package.mk

define Package/waev-outpost
  SECTION:=net
  CATEGORY:=Network
  TITLE:=waev:outpost console for openHop Repeater (trimmed for the M2)
  URL:=https://github.com/Treehouse-00/waev-outpost-plugin
  DEPENDS:=+openhop-repeater
  PKGARCH:=all
endef

define Package/waev-outpost/description
  waev:outpost, an alternative web console for openHop Repeater, served by openHop
  at /plugins/waev.outpost/. The openHop dashboard stays the default.
endef

# Big media the console works without, to fit the M2's flash (11.5 MB -> 4 MB):
# the login screen's bunny videos, the terminal's easter eggs, and background
# photos nothing references. rm without -f: a waev release that renames them
# fails the build, so the list gets checked.
WAEV_TRIM:=login-bunny-DYf-HPL8.mov login-bunny-CdOv7mzf.webm bunny-pixels-DhbN3jfj.mp4 \
	partytime.gif magma.jpg water.jpg ribbons.jpg folds.jpg

define Build/Prepare
	rm -rf $(PKG_BUILD_DIR) && mkdir -p $(PKG_BUILD_DIR)
	unzip -q $(DL_DIR)/$(PKG_SOURCE) 'ui/*' openhop-plugin.json -d $(PKG_BUILD_DIR)
	cd $(PKG_BUILD_DIR)/ui/assets && rm $(WAEV_TRIM)
	echo '{"id": "waev.outpost", "version": "$(PKG_VERSION)", "enabled": true, "source": "bundled"}' > $(PKG_BUILD_DIR)/state.json
endef

define Build/Compile
endef

# openhop-repeater's init script links this in as an enabled plugin at startup
define Package/waev-outpost/install
	$(INSTALL_DIR) $(1)/usr/share/openhop/waev.outpost
	$(CP) $(PKG_BUILD_DIR)/ui $(PKG_BUILD_DIR)/openhop-plugin.json $(PKG_BUILD_DIR)/state.json $(1)/usr/share/openhop/waev.outpost/
endef

$(eval $(call BuildPackage,waev-outpost))
