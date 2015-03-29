targets = danbo
CFLAGS = -std=c99 -D_GNU_SOURCE -g

include vars.mk

.PHONY: all
all: $(targets)

.PHONY: install
install: all
	install -d -m 0700 $(SANDBOX_ROOT)
	install -d -m 0755 $(SANDBOX_ROOT)/root \
	                   $(SANDBOX_ROOT)/layers \
			   $(SANDBOX_ROOT)/base \
			   $(SANDBOX_ROOT)/base/bin \
			   $(SANDBOX_ROOT)/base/sbin \
			   $(SANDBOX_ROOT)/base/etc \
			   $(SANDBOX_ROOT)/base/lib \
			   $(SANDBOX_ROOT)/base/dev \
			   $(SANDBOX_ROOT)/base/proc \
			   $(SANDBOX_ROOT)/base/var \
			   $(SANDBOX_ROOT)/base/var/lib \
			   $(SANDBOX_ROOT)/base/var/run \
			   $(SANDBOX_ROOT)/base/sys
	install -d -m 1777 $(SANDBOX_ROOT)/base/tmp
	install -m 4711 danbo $(SANDBOX_ROOT)/base/sbin

.PHONY:
clean:
	rm -f $(targets) $(addsuffix .o, $(targets))

.PHONY:
busybox-root:
	$(MAKE) -C roots/busybox

.PHONY:
install-busybox-root:
	$(MAKE) -C roots/busybox install

danbo: danbo.o
	$(CC) $< -static -o $@
