# burnd: board-side burn daemon. No dependencies beyond libc, built static
# so it runs on any image regardless of its libc version.
#
#   make                  riscv64 (Duo / Duo256 / DuoS)
#   make ARCH=aarch64     aarch64
#   make ARCH=native      host build, for tests
#   make test             run tests against a fake sysfs and a pty pair;
#                         a cross-built burnd runs under qemu-<arch>-static

ARCH ?= riscv64

ifeq ($(ARCH), native)
	CROSS_COMPILE =
	RUNNER =
else
	CROSS_COMPILE = $(ARCH)-linux-gnu-
	RUNNER = qemu-$(ARCH)-static
endif

CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

TARGET := burnd
CFLAGS ?= -O2 -Wall -Wextra
CFLAGS += -Iinclude
LDFLAGS += -static

SRC := $(wildcard src/*.c)
OBJS := $(patsubst %.c, %.o, $(SRC))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	$(STRIP) $@

%.o: %.c $(wildcard include/*.h)
	$(CC) $(CFLAGS) -c $< -o $@

test: all
	BURND_RUNNER="$(RUNNER)" tests/run.sh ./$(TARGET)

.PHONY: all test clean
clean:
	rm -f src/*.o $(TARGET)
