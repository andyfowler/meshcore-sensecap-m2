include $(TOPDIR)/rules.mk

PKG_NAME:=meshcore-m2-base
PKG_VERSION:=0.2.0
PKG_RELEASE:=1
PKG_LICENSE:=MIT
PKG_MAINTAINER:=meshcore-m2

include $(INCLUDE_DIR)/package.mk

define Package/meshcore-m2-base
  SECTION:=net
  CATEGORY:=Network
  TITLE:=SenseCAP M2 MeshCore repeater: site config + first-boot setup
  DEPENDS:=+openhop-repeater +python3-yaml +python3-sqlite3
  PKGARCH:=all
endef

define Package/meshcore-m2-base/description
  /etc/config/meshcore site pre-configuration (name, location, owner, radio preset,
  repeater settings, regions, Wi-Fi client, MQTT, companion) applied once on first boot:
  joins Wi-Fi, renders /etc/openhop_repeater/config.yaml, queues region setup.
endef

define Package/meshcore-m2-base/conffiles
/etc/config/meshcore
endef

define Build/Compile
endef

define Package/meshcore-m2-base/install
	$(INSTALL_DIR) $(1)/etc/config $(1)/etc/uci-defaults $(1)/usr/libexec/meshcore-m2
	$(INSTALL_CONF) ./files/meshcore.config $(1)/etc/config/meshcore
	$(INSTALL_BIN) ./files/uci-defaults-95-meshcore $(1)/etc/uci-defaults/95-meshcore
	$(INSTALL_BIN) ./files/firstboot.sh $(1)/usr/libexec/meshcore-m2/firstboot
	$(INSTALL_BIN) ./files/render_config.py $(1)/usr/libexec/meshcore-m2/render_config.py
	$(INSTALL_BIN) ./files/apply_pending.py $(1)/usr/libexec/meshcore-m2/apply-pending
	$(INSTALL_BIN) ./files/ssh-access $(1)/usr/libexec/meshcore-m2/ssh-access
	$(INSTALL_BIN) ./files/sync-root-password $(1)/usr/libexec/meshcore-m2/sync-root-password
	$(INSTALL_BIN) ./files/db-snapshot $(1)/usr/libexec/meshcore-m2/db-snapshot
	$(INSTALL_BIN) ./files/uci-defaults-90-meshcore-trim $(1)/etc/uci-defaults/90-meshcore-trim
	$(INSTALL_DIR) $(1)/lib/upgrade/keep.d
	$(INSTALL_DATA) ./files/sysupgrade-keep $(1)/lib/upgrade/keep.d/meshcore-m2
endef

$(eval $(call BuildPackage,meshcore-m2-base))
