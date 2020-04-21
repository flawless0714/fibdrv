CONFIG_MODULE_SIG = n
TARGET_MODULE := fibdrv

obj-m := $(TARGET_MODULE).o

$(TARGET_MODULE)-objs := fibdrv_mod.o \
                         bignum/apm.o \
                         bignum/bignum.o \
                         bignum/format.o \
                         bignum/mul.o \
                         bignum/sqr.o

# TODO: replace FPU related ops in the bignum lib, it's overhead-monster
# inside the kernel
ccflags-y := -std=gnu99 -Wno-declaration-after-statement -msse2

PWD := $(shell pwd)
KDIR := /lib/modules/$(shell uname -r)/build

GIT_HOOKS := .git/hooks/applied

all: $(GIT_HOOKS) client
	$(MAKE) -C $(KDIR) M=$(PWD) modules

$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) client out
load:
	sudo insmod $(TARGET_MODULE).ko
unload:
	sudo rmmod $(TARGET_MODULE) || true >/dev/null

client: client.c
	$(CC) -o $@ $^

PRINTF = env printf
PASS_COLOR = \e[32;01m
NO_COLOR = \e[0m
pass = $(PRINTF) "$(PASS_COLOR)$1 Passed [-]$(NO_COLOR)\n"

check: all
	$(MAKE) unload
	$(MAKE) load
	sudo ./client > out
	$(MAKE) unload
#	@diff -u /tmp/out scripts/expected.txt && $(call pass)
	@scripts/verify.py
