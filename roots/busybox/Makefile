BUSYBOX_VERSION=1.23.2

include ../../vars.mk

.PHONY:
all: busybox-$(BUSYBOX_VERSION)/busybox

.PHONY:
install: all
	install -m 0755 busybox-$(BUSYBOX_VERSION)/busybox $(SANDBOX_ROOT)/base/bin
	chroot $(SANDBOX_ROOT)/base /bin/busybox --install /bin

.PHONY:
clean:
	rm -rf busybox-$(BUSYBOX_VERSION)
	@echo
	@echo "I'm leaving busybox-$(BUSYBOX_VERSION).tar.bz2 so you don't have"
	@echo "to download it again."

busybox-$(BUSYBOX_VERSION).tar.bz2:
	wget http://busybox.net/downloads/busybox-$(BUSYBOX_VERSION).tar.bz2

busybox-$(BUSYBOX_VERSION): busybox-$(BUSYBOX_VERSION).tar.bz2
	tar xfj busybox-$(BUSYBOX_VERSION).tar.bz2

busybox-$(BUSYBOX_VERSION)/.config: busybox-$(BUSYBOX_VERSION) busybox-config-$(BUSYBOX_VERSION)
	cp busybox-config-$(BUSYBOX_VERSION) busybox-$(BUSYBOX_VERSION)/.config

busybox-$(BUSYBOX_VERSION)/busybox: busybox-$(BUSYBOX_VERSION) busybox-$(BUSYBOX_VERSION)/.config
	$(MAKE) -C busybox-$(BUSYBOX_VERSION)

