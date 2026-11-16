# Host-side test of the wave generator and BCM bit-plane builder.
#   make -C host_test        (build and run)
CC ?= cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -I../include
SRCS = ../src/bcm_frame.c ../src/wave_anim.c test_wave.c

.PHONY: test clean
test: test_wave
	./test_wave

test_wave: $(SRCS) ../include/bcm_frame.h ../include/wave_anim.h ../include/led_pins.h
	$(CC) $(CFLAGS) $(SRCS) -lm -o $@

clean:
	rm -f test_wave
