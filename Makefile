################################################################################
# Ben Baker 2020
#
# Makefile for compiling gfx2next
################################################################################

.PHONY: all install distro clean

MKDIR := mkdir -p

RM := rm -rf

CP := cp

ZIP := zip -r -q

BIN_DIR := bin

TMP_DIR := tmp

EXE_BASE_NAME := gfx2next

EXE_FULL_NAME := $(BIN_DIR)/$(EXE_BASE_NAME)

# common GNU variables related to install location
prefix ?= /usr/local
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin

all: $(EXE_FULL_NAME)

install: $(EXE_FULL_NAME)
	$(CP) $^ $(bindir)/$(^F)

distro: clean
	$(MAKE) all
	$(MKDIR) $(TMP_DIR)/gfx2next
	$(CP) $(BIN_DIR)/* $(TMP_DIR)/gfx2next
	$(CP) src/* $(TMP_DIR)/gfx2next
	$(CP) README.md $(TMP_DIR)/gfx2next
	$(RM) build/gfx2next.zip
	cd $(TMP_DIR); $(ZIP) ../build/gfx2next.zip gfx2next

clean:
	$(RM) $(BIN_DIR) $(TMP_DIR)

$(EXE_FULL_NAME): src/lodepng.c src/zx0.c src/gfx2next.c
	$(MKDIR) $(@D)
	gcc -O2 -Wall -o $@ $^ -lm
