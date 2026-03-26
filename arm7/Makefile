# SPDX-License-Identifier: CC0-1.0
# DSi MP3 Player - ARM7 Makefile

BLOCKSDS	?= /opt/blocksds/core

WONDERFUL_TOOLCHAIN	?= /opt/wonderful
ARM_NONE_EABI_PATH	?= $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/

SOURCEDIRS	?= arm7/source
INCLUDEDIRS	?= arm7/include common
BINDIRS		?=

DEFINES		?=

LIBS		?= -lnds7
LIBDIRS		+= $(BLOCKSDS)/libs/libnds

BUILDDIR	:= build/arm7
NAME		:= arm7
ELF			:= build/$(NAME).elf
DUMP		:= build/$(NAME).dump
MAP			:= build/$(NAME).map

PREFIX		:= $(ARM_NONE_EABI_PATH)arm-none-eabi-
CC			:= $(PREFIX)gcc
CXX			:= $(PREFIX)g++
OBJDUMP		:= $(PREFIX)objdump
MKDIR		:= mkdir
RM			:= rm -rf

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

ifneq ($(BINDIRS),)
    SOURCES_BIN	:= $(shell find -L $(BINDIRS) -name "*.bin")
    INCLUDEDIRS	+= $(addprefix $(BUILDDIR)/,$(BINDIRS))
endif

SOURCES_S	:= $(shell find -L $(SOURCEDIRS) -name "*.s")
SOURCES_C	:= $(shell find -L $(SOURCEDIRS) -name "*.c")
SOURCES_CPP	:= $(shell find -L $(SOURCEDIRS) -name "*.cpp")

DEFINES		+= -D__NDS__ -DARM7

ARCH		:= -mcpu=arm7tdmi -mtune=arm7tdmi

WARNFLAGS	:= -Wall

ifeq ($(SOURCES_CPP),)
    LD		:= $(CC)
    LIBS	+= -lc
else
    LD		:= $(CXX)
    LIBS	+= -lstdc++ -lc
endif

INCLUDEFLAGS	:= $(foreach path,$(INCLUDEDIRS),-I$(path)) \
		   $(foreach path,$(LIBDIRS),-I$(path)/include)

LIBDIRSFLAGS	:= $(foreach path,$(LIBDIRS),-L$(path)/lib)

ASFLAGS		+= -x assembler-with-cpp $(DEFINES) $(ARCH) \
		   -mthumb -mthumb-interwork $(INCLUDEFLAGS) \
		   -ffunction-sections -fdata-sections

CFLAGS		+= -std=gnu11 $(WARNFLAGS) $(DEFINES) $(ARCH) \
		   -mthumb -mthumb-interwork $(INCLUDEFLAGS) -O2 \
		   -ffunction-sections -fdata-sections \
		   -fomit-frame-pointer

LDFLAGS		:= -mthumb-interwork $(LIBDIRSFLAGS) \
		   -Wl,-Map,$(MAP) -Wl,--gc-sections -nostdlib \
		   -T$(BLOCKSDS)/sys/crts/ds_arm7.ld \
		   -Wl,--no-warn-rwx-segments \
		   -Wl,--start-group $(LIBS) -lgcc -Wl,--end-group

OBJS_ASSETS	:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_BIN)))
HEADERS_ASSETS	:= $(patsubst %.bin,%_bin.h,$(addprefix $(BUILDDIR)/,$(SOURCES_BIN)))
OBJS_SOURCES	:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_S))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_C))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_CPP)))
OBJS		:= $(OBJS_ASSETS) $(OBJS_SOURCES)
DEPS		:= $(OBJS:.o=.d)

.PHONY: all clean

all: $(ELF)

$(ELF): $(OBJS)
	@echo "  LD.7    $@"
	$(V)$(LD) -o $@ $(OBJS) $(BLOCKSDS)/sys/crts/ds_arm7_crt0.o $(LDFLAGS)

clean:
	@echo "  CLEAN.7"
	$(V)$(RM) $(ELF) $(DUMP) $(MAP) $(BUILDDIR)

$(BUILDDIR)/%.s.o : %.s
	@echo "  AS.7    $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(ASFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.c.o : %.c
	@echo "  CC.7    $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.arm.c.o : %.arm.c
	@echo "  CC.7    $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -marm -mlong-calls -c -o $@ $<

$(BUILDDIR)/%.cpp.o : %.cpp
	@echo "  CXX.7   $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

$(SOURCES_S) $(SOURCES_C) $(SOURCES_CPP): $(HEADERS_ASSETS)

-include $(DEPS)
