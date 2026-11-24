/*
 * render.c - dump a mode's duty codes for the GIF preview.
 *   render <mode 1..N> <milliseconds>   -> stdout: 40 bytes per frame
 *   render --list                       -> mode names, one per line
 * Codes are what the engine would actually drive (gamma + cap applied).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ring_frame.h"
#include "mode.h"
#include "ornament_core.h"

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--list") == 0) {
        printf("%d %d\n", RING_CAP_CODE, CORE_FRAME_MS);
        for (int m = 0; m < MODE_COUNT; m++) printf("%s\n", MODES[m]->name);
        return 0;
    }
    if (argc != 3) {
        fprintf(stderr, "usage: render <mode> <ms> | --list\n");
        return 2;
    }
    int m = atoi(argv[1]) - 1;
    int ms = atoi(argv[2]);
    if (m < 0 || m >= MODE_COUNT) return 2;
    core_init(2024, (uint8_t)m);
    uint8_t out[RING_LEDS], code[RING_LEDS];
    for (int t = 0; t < ms; t += CORE_FRAME_MS) {
        core_frame(out);
        ring_codes(out, code);
        fwrite(code, 1, RING_LEDS, stdout);
    }
    return 0;
}
