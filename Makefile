.PHONY: all cleansub music

all:
	mkpsxiso -y ./isoconfig.xml

cleansub:
	$(MAKE) clean
	rm -f psx_break.cue psx_break.bin

# Either add the nolibgs folder to your environment variables under NOLIBGS,
# or replace ${NOLIBGS} with the path to your nolibgs folder.
NOLIBROOT = ${NOLIBGS}

TARGET = psx_break

SRCS = main.c \
res/marquee.tim \
res/marquee1.tim \
res/marquee2.tim \
res/paddle.tim \
res/ball.tim \

include $(NOLIBROOT)/common.mk 
