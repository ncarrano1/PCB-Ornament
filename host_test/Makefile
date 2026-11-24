# Host-side tests (no hardware needed):  make -C host_test
CC ?= cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Werror -I../include

WAVE_SRCS = ../src/bcm_frame.c ../src/wave_anim.c
ORN_SRCS  = ../src/ring_frame.c ../src/tables.c ../src/ornament_core.c \
            ../src/button_fsm.c $(wildcard ../src/modes/*.c)
HDRS      = $(wildcard ../include/*.h)

.PHONY: test clean
test: test_wave test_ornament
	./test_wave
	./test_ornament

test_wave: $(WAVE_SRCS) test_wave.c $(HDRS)
	$(CC) $(CFLAGS) $(WAVE_SRCS) test_wave.c -lm -o $@

# No -lm: the ornament code must not need floating-point math.
test_ornament: $(ORN_SRCS) test_ornament.c $(HDRS)
	$(CC) $(CFLAGS) $(ORN_SRCS) test_ornament.c -o $@

render: $(ORN_SRCS) render.c $(HDRS)
	$(CC) $(CFLAGS) $(ORN_SRCS) render.c -o $@

clean:
	rm -f test_wave test_ornament render
