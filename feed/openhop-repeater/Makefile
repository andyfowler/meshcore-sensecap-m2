include $(TOPDIR)/rules.mk

PKG_NAME:=openhop-repeater
PKG_VERSION:=2026.09.09
PKG_RELEASE:=1

# openHop Repeater 1.1.4 (keep in step with the commit below)
OPENHOP_REPEATER_VERSION:=1.1.4
PKG_SOURCE:=openhop_repeater-13eb8b2ea8b1.tar.gz
PKG_SOURCE_URL:=https://codeload.github.com/openhop-dev/openhop_repeater/tar.gz/13eb8b2ea8b1cdb4a07ed6e282dc99e3aa8a5a8b?
PKG_HASH:=e32248bd1f3095179a83532bb962a1f0a07a518cb254c1ec29ddfecda919f865
PKG_BUILD_DIR:=$(BUILD_DIR)/openhop-repeater
PKG_BUILD_DEPENDS:=python3/host
PKG_LICENSE:=MIT
PKG_MAINTAINER:=meshcore-m2

OPENHOP_CORE_SHA:=e07722ea96093752ac8b3c6772d653f8b68224bf
include $(INCLUDE_DIR)/package.mk
define Download/openhop-core
  FILE:=openhop_core-$(OPENHOP_CORE_SHA).tar.gz
  URL:=https://codeload.github.com/openhop-dev/openhop_core/tar.gz/$(OPENHOP_CORE_SHA)?
  HASH:=3228e6a29cb54e574dbf93fa75d286d28e3667383824e9dc02da5031ddbbd8f3
endef
$(eval $(call Download,openhop-core))
define Download/cheroot
  FILE:=cheroot-11.1.2-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/41/99/af65511a10c4212438ac52bc5e45e486e7a04d292201ad84dfd9208fe9a8/
  HASH:=0f6c0ba05c00fbc869fb46b1de4ec2384e1d85418ae963d3bc10ae83b688dbfa
endef
$(eval $(call Download,cheroot))
define Download/cherrypy
  FILE:=CherryPy-18.10.0-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/97/61/4aba4860c91e66adbc44a62a437ad845abab21cc3dcb0b8e320000d091dd/
  HASH:=129e444b9a63cea4e765481b156376f1cfe319e64caaaec2485636532373b298
endef
$(eval $(call Download,cherrypy))
define Download/cherrypy-cors
  FILE:=cherrypy_cors-1.7.0-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/d7/05/01d4deda775d3a84d172719ebd1cb5314487377a128e892208b0a9533fc5/
  HASH:=9cdeddcad89efba5a7bed06ed80150ef7cc418899209e6507481e84be7df6eea
endef
$(eval $(call Download,cherrypy-cors))
define Download/httpagentparser
  FILE:=httpagentparser-1.9.9-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/95/ca/50ca912f58807e14b6e993d1b75d2a951e014ed82fe42779947ca3e6a159/
  HASH:=755898cc96e796f6bf5327b604a81502622fd00637ef319af133a6ffd1a3e753
endef
$(eval $(call Download,httpagentparser))
define Download/jaraco-collections
  FILE:=jaraco_collections-5.2.1-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/b5/fe/570f6a7b4443a3548ab02ba88c77b64521452a2eed2c80449d13be8fd4d1/
  HASH:=ea3415ddfc50206b6ba7d09c3deb358b76a5f8c1ddc82be788e8e8bd151ebfa8
endef
$(eval $(call Download,jaraco-collections))
define Download/jaraco-context
  FILE:=jaraco_context-6.1.2-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/f2/58/bc8954bda5fcda97bd7c19be11b85f91973d67a706ed4a3aec33e7de22db/
  HASH:=bf8150b79a2d5d91ae48629d8b427a8f7ba0e1097dd6202a9059f29a36379535
endef
$(eval $(call Download,jaraco-context))
define Download/jaraco-functools
  FILE:=jaraco_functools-4.6.0-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/02/36/ecc85bc96c273dc8a11273ed4782272975e6338d4a3e9228621175edf0e3/
  HASH:=99e3dc0060c5cbe8fcd1cdb36258e2a65ca40f1566b2033b12abb1bb44dd3c30
endef
$(eval $(call Download,jaraco-functools))
define Download/jaraco-text
  FILE:=jaraco_text-4.3.0-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/d2/4f/cd038114e8ed88682eabe3123e598f8d236ab0f6b8c642d522438bfba175/
  HASH:=07a06a9eaf8b7eee54f2fec816565ef1eec5aa78fc81e4f1c3131ad09ff45a9a
endef
$(eval $(call Download,jaraco-text))
define Download/portend
  FILE:=portend-3.2.1-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/49/11/b0b19077f80cb8972b44c96af983e8cfcdb1506090bacef137ed8b448af8/
  HASH:=bd2bb1cd8585e32f1f986c1b8eae2950c8622c648db163b711a2fc2ef8d8d492
endef
$(eval $(call Download,portend))
define Download/tempora
  FILE:=tempora-5.13.0-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/64/e1/e43758bd6ca6c73f885472f48450e3cc808ac07f9a871123444ef95b2aae/
  HASH:=bf5eae4209e0f320778bf375240dcac800f7eb8e838eafee77a349723fca343b
endef
$(eval $(call Download,tempora))
define Download/zc-lockfile
  FILE:=zc_lockfile-4.1-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/e6/d6/2276130fc7cce28e0e0f815f2725ccb7599aeac8f6520fbc17d8154fe4a8/
  HASH:=d1897568e3bb9dc38121768641393ae7365246700041a40866c91d626c8e8fc7
endef
$(eval $(call Download,zc-lockfile))
define Download/pyjwt
  FILE:=pyjwt-2.15.1-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/50/ca/44de4e75f8aadc457f0634be3b542815078ded46dca30efb960edeecad6e/
  HASH:=42d59d631f7768a1028a64c7ff581a9bf7519804daf91fc5b6c56e30eec5e193
endef
$(eval $(call Download,pyjwt))
define Download/backports-tarfile
  FILE:=backports.tarfile-1.2.0-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/b9/fa/123043af240e49752f1c4bd24da5053b6bd00cad78c2be53c0d1e8b975bc/
  HASH:=77e284d754527b01fb1e6fa8a1afe577858ebe4e9dad8919e34c862cb399bc34
endef
$(eval $(call Download,backports-tarfile))
define Download/autocommand
  FILE:=autocommand-2.2.2-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/b5/62/05203c39d21aa3171454a6c5391ea3b582a97bfb1bc1adad25628331a1cd/
  HASH:=710afe251075e038e19e815e25f8155cabe02196cfb545b2185e0d9c8b2b0459
endef
$(eval $(call Download,autocommand))
define Download/ws4py
  FILE:=ws4py-0.6.0-py3-none-any.whl
  URL:=https://files.pythonhosted.org/packages/89/62/dad08725f855f9695d1c2b34466cf0d1482d1168bb54d73d22b6b0124ba3/
  HASH:=1499c3fc103a65eb12d7b1ead7566f88487f6f678ab10ee4e53cf2411c068752
endef
$(eval $(call Download,ws4py))
PY_WHEELS:=cheroot-11.1.2-py3-none-any.whl \
	CherryPy-18.10.0-py3-none-any.whl \
	cherrypy_cors-1.7.0-py3-none-any.whl \
	httpagentparser-1.9.9-py3-none-any.whl \
	jaraco_collections-5.2.1-py3-none-any.whl \
	jaraco_context-6.1.2-py3-none-any.whl \
	jaraco_functools-4.6.0-py3-none-any.whl \
	jaraco_text-4.3.0-py3-none-any.whl \
	portend-3.2.1-py3-none-any.whl \
	tempora-5.13.0-py3-none-any.whl \
	zc_lockfile-4.1-py3-none-any.whl \
	pyjwt-2.15.1-py3-none-any.whl \
	backports.tarfile-1.2.0-py3-none-any.whl \
	autocommand-2.2.2-py3-none-any.whl \
	ws4py-0.6.0-py3-none-any.whl

define Package/openhop-repeater
  SECTION:=net
  CATEGORY:=Network
  TITLE:=openHop MeshCore repeater daemon (with openhop_core)
  URL:=https://github.com/openhop-dev/openhop_repeater
  DEPENDS:=+python3-light +python3-asyncio +python3-logging +python3-sqlite3 +python3-openssl \
	+python3-urllib +python3-email +python3-codecs +python3-ctypes +python3-uuid +python3-decimal \
	+python3-multiprocessing +libopenssl +python3-pynacl +python3-yaml +python3-paho-mqtt \
	+python3-psutil +python3-pyserial +python3-websockets +python3-dateutil +python3-more-itertools \
	+python3-typing-extensions +m2modem
endef

define Package/openhop-repeater/description
  openHop Repeater (formerly pyMC_Repeater): a MeshCore repeater with web dashboard,
  MQTT observer uploads (LetsMesh/MeshMapper/...) and remote CLI. Bundles openhop_core,
  a small pycryptodome AES shim and the pure-python CherryPy/JWT/ws4py dependencies
  that OpenWrt does not package. Talks to m2modem on 127.0.0.1:5055.
endef

define Package/openhop-repeater/conffiles
/etc/openhop_repeater/
endef

OH_LIB:=$(PKG_BUILD_DIR)/lib

define Build/Prepare
	rm -rf $(PKG_BUILD_DIR) && mkdir -p $(PKG_BUILD_DIR)/src $(OH_LIB)/vendor
	tar -xzf $(DL_DIR)/$(PKG_SOURCE) -C $(PKG_BUILD_DIR)/src
	tar -xzf $(DL_DIR)/openhop_core-$(OPENHOP_CORE_SHA).tar.gz -C $(PKG_BUILD_DIR)/src
	for p in ./patches/*.patch; do patch -p1 -d $(PKG_BUILD_DIR)/src/openhop_repeater-* < $$$$p || exit 1; done
	cp -R $(PKG_BUILD_DIR)/src/openhop_repeater-*/repeater $(OH_LIB)/
	# normally generated from git tags at packaging time; without it openHop reports 'unknown'
	echo 'version = __version__ = "$(OPENHOP_REPEATER_VERSION)"' > $(OH_LIB)/repeater/_version.py
	cp -R $(PKG_BUILD_DIR)/src/openhop_core-*/src/openhop_core $(OH_LIB)/
	mkdir -p $(OH_LIB)/Crypto/Cipher
	touch $(OH_LIB)/Crypto/__init__.py $(OH_LIB)/Crypto/Cipher/__init__.py
	cp ./files/pycryptodome_aes_shim.py $(OH_LIB)/Crypto/Cipher/AES.py
	for w in $(PY_WHEELS); do unzip -q -o $(DL_DIR)/$$$$w -d $(OH_LIB)/vendor -x '*.dist-info/RECORD'; done
	$(STAGING_DIR_HOSTPKG)/bin/python3 ./files/speedups.py $(OH_LIB)/vendor
	find $(OH_LIB) \( -name tests -o -name test -o -name __pycache__ \) -type d -prune -exec rm -rf {} +
	find $(OH_LIB) -name '*.map' -delete
endef

define Build/Compile
	# add the SenseCAP M2 as the first hardware choice in openHop's setup wizard
	$(STAGING_DIR_HOSTPKG)/bin/python3 -c 'import json,sys; f=sys.argv[1]; d=json.load(open(f)); d["hardware"]={**json.load(open(sys.argv[2])), **d["hardware"]}; json.dump(d,open(f,"w"),indent=2)' \
		$$$$(ls -d $(PKG_BUILD_DIR)/src/openhop_repeater-*)/radio-settings.json ./files/sensecap-m2-hardware.json
	$(STAGING_DIR_HOSTPKG)/bin/python3 -m compileall -q -j 0 -d /usr/lib/openhop $(OH_LIB) >/dev/null
endef

define Package/openhop-repeater/install
	$(INSTALL_DIR) $(1)/usr/lib/openhop $(1)/usr/share/openhop_repeater $(1)/etc/init.d $(1)/etc/openhop_repeater
	$(CP) $(OH_LIB)/* $(1)/usr/lib/openhop/
	$(CP) $(PKG_BUILD_DIR)/src/openhop_repeater-*/config.yaml.example $(1)/usr/share/openhop_repeater/
	# openHop looks for these next to the package (repeater/web/../../)
	$(CP) $(PKG_BUILD_DIR)/src/openhop_repeater-*/radio-presets.json $(1)/usr/lib/openhop/
	$(CP) $(PKG_BUILD_DIR)/src/openhop_repeater-*/radio-settings.json $(1)/usr/lib/openhop/
	$(INSTALL_BIN) ./files/openhop-repeater.init $(1)/etc/init.d/openhop-repeater
endef

$(eval $(call BuildPackage,openhop-repeater))
