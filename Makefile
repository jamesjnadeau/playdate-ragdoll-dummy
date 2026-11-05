HEAP_SIZE      = 8388208
STACK_SIZE     = 61800

PRODUCT = RagdollDummies.pdx

# Host tests need no SDK; only device/simulator targets do.
HOST_ONLY := $(if $(MAKECMDGOALS),$(if $(filter-out test host-tests,$(MAKECMDGOALS)),,1),)

ifeq ($(HOST_ONLY),)
# Locate the SDK
SDK = ${PLAYDATE_SDK_PATH}
ifeq ($(SDK),)
	SDK = $(shell egrep '^\s*SDKRoot' ~/.Playdate/config | head -n 1 | cut -c9-)
endif

ifeq ($(SDK),)
$(error SDK path not found; set ENV value PLAYDATE_SDK_PATH)
endif

endif

VPATH += src/core:src/platform/playdate

# List C source files here
SRC = $(wildcard src/core/*.c) $(wildcard src/platform/playdate/*.c)

# List all user directories here
UINCDIR = src/core src/platform/playdate

# List user asm files
UASRC = 

# List all user C define here, like -D_DEBUG=1
UDEFS = -std=gnu11 -Wfloat-conversion -fsingle-precision-constant -Werror=double-promotion -Werror=float-conversion

# Define ASM defines here
UADEFS = 

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ULIBS =

# Host unit tests (pure src/core only; no Playdate SDK involved)
HOST_CFLAGS = -std=c99 -O1 -g -Wall -Wextra -Wdouble-promotion -Wfloat-conversion -fsingle-precision-constant -Werror -Isrc/core -Itests

ifeq ($(HOST_ONLY),)
include $(SDK)/C_API/buildsupport/common.mk
endif

.PHONY: test check-doubles host-tests
# Parallel by default: recurse with -j so only compilation fans out.
test:
	@$(MAKE) -j$(shell nproc) --no-print-directory host-tests

HOST_SRC = $(wildcard src/core/*.c) $(wildcard tests/*.c)
HOST_OBJ = $(patsubst %.c,build/host/obj/%.o,$(HOST_SRC))

host-tests: build/host/tests
	./build/host/tests

build/host/tests: $(HOST_OBJ)
	gcc $(HOST_OBJ) -lm -o $@

build/host/obj/%.o: %.c
	@mkdir -p $(dir $@)
	gcc $(HOST_CFLAGS) -MMD -MP -c $< -o $@

-include $(HOST_OBJ:.o=.d)

check-doubles: device_bin
	tools/check_no_doubles.sh build/pdex.elf
