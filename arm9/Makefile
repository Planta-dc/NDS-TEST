# SPDX-License-Identifier: CC0-1.0
# DSi MP3 Player - ARM9 Makefile

BLOCKSDS	?= /opt/blocksds/core
BLOCKSDSEXT	?= /opt/blocksds/external

WONDERFUL_TOOLCHAIN	?= /opt/wonderful
ARM_NONE_EABI_PATH	?= $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/

SOURCEDIRS	?= arm9/source
INCLUDEDIRS	?= arm9/include common
GFXDIRS		?=
BINDIRS		?=
AUDIODIRS	?=

DEFINES		?=

# -lmad is the key addition vs SSEQPlayer
LIBS		?= -lmad -lnds9
LIBDIRS		+= $(BLOCKSDS)/libs/libnds
LIBDIRS		+= $(BLOCKSDSEXT)/libs/libmad

BUILDDIR	:= build/arm9
NAME		:= arm9
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

SOURCES_S	:= $(shell find -L $(SOURCEDIRS) -name "*.s")
SOURCES_C	:= $(shell find -L $(SOURCEDIRS) -name "*.c")
SOURCES_CPP	:= $(shell find -L $(SOURCEDIRS) -name "*.cpp")

DEFINES		+= -D__NDS__ -DARM9

ARCH		:= -march=armv5te -mtune=arm946e-s

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

CXXFLAGS	+= -std=gnu++14 $(WARNFLAGS) $(DEFINES) $(ARCH) \
		   -mthumb -mthumb-interwork $(INCLUDEFLAGS) -O2 \
		   -ffunction-sections -fdata-sections \
		   -fno-exceptions -fno-rtti \
		   -fomit-frame-pointer

LDFLAGS		:= -mthumb -mthumb-interwork $(LIBDIRSFLAGS) \
		   -Wl,-Map,$(MAP) -Wl,--gc-sections -nostdlib \
		   -Tarm9/ds_arm9_hi.mem \
		   -T$(BLOCKSDS)/sys/crts/ds_arm9.ld \
		   -Wl,--no-warn-rwx-segments \
		   -Wl,--start-group $(LIBS) -lgcc -Wl,--end-group

OBJS_SOURCES	:= $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_S))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_C))) \
		   $(addsuffix .o,$(addprefix $(BUILDDIR)/,$(SOURCES_CPP)))

OBJS		:= $(OBJS_SOURCES)
DEPS		:= $(OBJS:.o=.d)

.PHONY: all clean

all: $(ELF)

$(ELF): $(OBJS)
	@echo "  LD.9    $@"
	$(V)$(LD) -o $@ $(OBJS) $(BLOCKSDS)/sys/crts/ds_arm9_crt0.o $(LDFLAGS)

clean:
	@echo "  CLEAN.9"
	$(V)$(RM) $(ELF) $(DUMP) $(MAP) $(BUILDDIR)

$(BUILDDIR)/%.s.o : %.s
	@echo "  AS.9    $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(ASFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.c.o : %.c
	@echo "  CC.9    $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILDDIR)/%.arm.c.o : %.arm.c
	@echo "  CC.9    $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CC) $(CFLAGS) -MMD -MP -marm -mlong-calls -c -o $@ $<

$(BUILDDIR)/%.cpp.o : %.cpp
	@echo "  CXX.9   $<"
	@$(MKDIR) -p $(@D)
	$(V)$(CXX) $(CXXFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPS)
