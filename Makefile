################################################################################
# Ben Baker 2020
#
# Makefile for compiling gfx2next
################################################################################

MKDIR := mkdir -p

RM := rm -rf

CP := cp

ZIP := zip -r -q

all:
	$(MKDIR) bin
	gcc -O2 -Wall -o bin/gfx2next src/lodepng.c src/zx7.c src/megalz.c src/gfx2next.c

distro:
	$(MAKE) clean all
	$(RM) tmp
	$(MKDIR) tmp/gfx2next
	$(CP) bin/* tmp/gfx2next
	$(CP) src/* tmp/gfx2next
	$(CP) README.md tmp/gfx2next
	$(RM) build/gfx2next.zip
	cd tmp; $(ZIP) ../build/gfx2next.zip gfx2next
	$(RM) tmp

clean:
	$(RM) bin tmp
