# SPDX-License-Identifier: CC0-1.0
# DSi MP3 Player - Root Makefile

BLOCKSDS	?= /opt/blocksds/core
BLOCKSDSEXT	?= /opt/blocksds/external

NAME		:= DSi_MP3_Player
GAME_TITLE	:= DSi MP3 Player
GAME_AUTHOR	:= Based on SSEQPlayer by CaitSith2/Rocket Robz
GAME_ICON	:= icon.gif

ROM			:= $(NAME).nds
MAKE		:= make
RM			:= rm -rf

.PHONY: all clean arm9 arm7

all: $(ROM)

clean:
	@echo "  CLEAN"
	$(V)$(MAKE) -f arm9/Makefile clean --no-print-directory
	$(V)$(MAKE) -f arm7/Makefile clean --no-print-directory
	$(V)$(RM) $(ROM) build

arm9:
	$(V)+$(MAKE) -f arm9/Makefile --no-print-directory

arm7:
	$(V)+$(MAKE) -f arm7/Makefile --no-print-directory

$(ROM): arm9 arm7
	@echo "  NDSTOOL $@"
	$(V)$(BLOCKSDS)/tools/ndstool/ndstool -c $@ \
		-7 build/arm7.elf -9 build/arm9.elf \
		-b $(GAME_ICON) "$(GAME_TITLE);$(GAME_AUTHOR)"
