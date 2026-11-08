include $(TOPDIR)/rules.mk

PKG_NAME:=m2modem
PKG_VERSION:=0.2.0
PKG_RELEASE:=1

# Semtech SX1302 HAL (only libloragw is used: SX1302 register access, SX1250 SPI pass-through)
HAL_VERSION:=2.1.0
PKG_SOURCE:=sx1302_hal-$(HAL_VERSION).tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/Lora-net/sx1302_hal/tar.gz/refs/tags/V$(HAL_VERSION)?
PKG_HASH:=4b62aa6a83ad449c68fdd844fc35024586c9faabca3c5a90a2544735b380de5b
PKG_BUILD_DIR:=$(BUILD_DIR)/sx1302_hal-$(HAL_VERSION)

PKG_LICENSE:=BSD-3-Clause
PKG_MAINTAINER:=meshcore-m2

include $(INCLUDE_DIR)/package.mk

define Package/m2modem
  SECTION:=net
  CATEGORY:=Network
  TITLE:=SenseCAP M2 LoRa modem (SX1250 direct, openHop modem protocol)
  DEPENDS:=+kmod-spi-dev +librt +libpthread
endef

define Package/m2modem/description
  Drives the SenseCAP M2's SX1250 radio A as a standalone SX126x LoRa transceiver
  (any SX126x bandwidth incl. 62.5 kHz), with radio B clocking the SX1302 so the
  front-end module can be switched for TX/RX, and serves the openHop modem
  protocol on TCP port 5055 for openhop-repeater (radio_type: modem_tcp).
endef

HAL_CFLAGS:= -std=gnu99 -DSPI_SPEED=2000000 -DI2C_DEVICE=\"/dev/i2c-0\" \
	-DLED_NAME=\"green:lora\" -DLED_INTERVAL=\"100\" \
	-I$(PKG_BUILD_DIR)/libloragw/inc -I$(PKG_BUILD_DIR)/libloragw -I$(PKG_BUILD_DIR)/libtools/inc

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/libloragw inc/config.h
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) $(HAL_CFLAGS) \
		$(PKG_BUILD_DIR)/m2modem.c \
		$(PKG_BUILD_DIR)/libloragw/src/*.c $(PKG_BUILD_DIR)/libtools/src/*.c \
		-lm -lrt -lpthread -o $(PKG_BUILD_DIR)/m2modem
endef

define Package/m2modem/install
	$(INSTALL_DIR) $(1)/usr/sbin $(1)/etc/init.d $(1)/usr/libexec/meshcore-m2
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/m2modem $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/m2modem.init $(1)/etc/init.d/m2modem
	$(INSTALL_BIN) ./files/reset-sx1302 $(1)/usr/libexec/meshcore-m2/reset-sx1302
endef

$(eval $(call BuildPackage,m2modem))
