/* SPDX-License-Identifier: GPL-3.0-only */
/* FM-1 knobs (hal/fm1_input.h): the fast turn is timed by the scanner, between clicks, not by how many clicks
 * the main loop finds waiting. Before, the app tripled any take of two or more, so a busy main loop (a redraw
 * while the audio has most of the CPU) let clicks pile up and a slow turn jumped by three times as much.
 * Feeds encoder 0's two lines frame by frame (fm1__frame), as the 10 kHz scan does. */
#define FM1_INPUT_HOST 1
#include <stdio.h>
#include "fm1_input.h"
static int fails;
static void check(int ok, const char *what) { printf("enc: %-74s %s\n", what, ok ? "ok" : "FAIL"); fails += !ok; }

static void frame(uint32_t st)                  /* encoder 0: A = column 0 bit 0, B = column 1 bit 0 */
{
    fm1_in.raw[0] = (uint8_t)(st >> 1 & 1u);
    fm1_in.raw[1] = (uint8_t)(st & 1u);
    fm1__frame();
}

static void reset(void)
{
    uint32_t e;
    for (e = 0; e < FM1_NENC; e++) {
        fm1_in.enc_prev[e] = fm1_in.enc_last[e] = 0xFF;
        fm1_in.enc_sub[e] = 0;
        fm1_in.enc_steps[e] = fm1_in.enc_fast[e] = 0;
        fm1_in.enc_t[e] = 0;
        fm1_in.enc_dir[e] = 0;
    }
    fm1_in.frames = 1000;                       /* (well past power-on) */
    frame(0); frame(0);                         /* at rest on state 0: the detent */
}

/* n clicks, each state held h frames; dir > 0 clockwise (0 1 3 2 0), < 0 the other way. take: after every
 * frame (an idle main loop) or 0 = only at the end (a busy one); returns what fm1_enc_take_fast gave in all */
static int32_t turn(int n, uint32_t h, int dir, int take)
{
    static const uint8_t CW[4] = {1, 3, 2, 0}, CCW[4] = {2, 3, 1, 0};
    int32_t got = 0;
    int c, s;
    uint32_t i;
    for (c = 0; c < n; c++)
        for (s = 0; s < 4; s++)
            for (i = 0; i < h; i++) {
                frame(dir > 0 ? CW[s] : CCW[s]);
                if (take) got += fm1_enc_take_fast(0);
            }
    return got + fm1_enc_take_fast(0);
}

int main(void)
{
    int32_t a, b;
    reset();
    check(turn(5, 20, 1, 1) == 5, "slow turn (80 frames a click): 5 clicks = 5 steps");
    check(turn(5, 20, -1, 0) == -5, "slow turn, taken at the end only (a busy main loop): still 5, no jump");
    reset();
    a = turn(5, 3, 1, 1);
    reset();
    b = turn(5, 3, 1, 0);
    check(a == 13, "fast turn (12 frames a click): the first click 1, the next 4 count 3 each = 13");
    check(a == b, "fast turn: the same taken every frame or once at the end");
    reset();
    turn(3, 3, 1, 1);
    check(turn(1, 3, -1, 1) == -1, "turning back fast: the first click the other way counts 1");
    reset();
    for (a = 0; a < 3; a++) { frame(1); frame(1); frame(3); frame(3); frame(2); frame(2); frame(0); frame(0); }
    check(fm1_in.enc_fast[0] == 4 && fm1_enc_take(0) == 3 && fm1_enc_take_fast(0) == 0,
          "fm1_enc_take: 3 fast clicks give 3 plain steps, and it clears the fast ones too");
    reset();
    check(turn(1, 3, 1, 1) == 1 && turn(1, 40, 1, 1) == 1, "a click after a pause (160 frames) is plain again");
    printf(fails ? "ENC TEST FAILED\n" : "enc test passed\n");
    return fails != 0;
}
