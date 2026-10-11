#include "tune.h"

#define XTAL     25000000LL
#define DC_GUARD 200   // keep the shifted SSB band this far from 0 Hz (DC offset, LO leakage)
#define BAND_MAX 29000 // and inside this, short of the 32 kHz Nyquist edge
#define FS_IQ    64000

// Output divider as si5351_CalcIQ() picks it (PLL as high as possible; below 4.9 MHz the PLL runs under 600 MHz)
static uint32_t pick_div(uint32_t lo) {
    if (lo < 4900000) return 127;
    if (lo < 8000000) return 625000000 / lo;
    return 900000000 / lo;
}

static int band_ok(int32_t off, int32_t passband_lo, int32_t passband_hi) {
    int32_t lo = off + passband_lo, hi = off + passband_hi;
    if (lo < -BAND_MAX || hi > BAND_MAX) return 0;
    return lo > DC_GUARD || hi < -DC_GUARD;
}

int tune_choose(uint32_t frequency, int32_t passband_lo, int32_t passband_hi, tune_t *t) {
    int32_t best = 0;
    int found = 0;

    // Coarse grid first; the fine one only when no 40 kHz point keeps the band clear of DC
    static const uint32_t grids[] = {TUNE_LO_GRID, TUNE_LO_GRID_FINE};
    for (int g = 0; g < 2 && !found; g++) {
        uint32_t step = grids[g];
        uint32_t base = frequency / step;
        for (int32_t k = -4; k <= 4; k++) {
            int64_t lo = ((int64_t)base + k) * step;
            if (lo <= 0) continue;
            int64_t off = (int64_t)frequency - lo;
            if (off > TUNE_MAX_OFFSET || off < -TUNE_MAX_OFFSET) continue;
            if (!band_ok((int32_t)off, passband_lo, passband_hi)) continue;
            if (!found || (off < 0 ? -off : off) < (best < 0 ? -best : best)) {
                found = 1;
                best = (int32_t)off;
            }
        }
    }
    if (!found) {
        return 0;
    }

    uint32_t lo = frequency - best;
    uint32_t div = pick_div(lo);
    // LO = 8 kHz * i is exactly 25 MHz * (i * div) / (3125 * div): PLL fraction (i * div) / 3125
    t->div = div;
    t->n = lo / TUNE_LO_GRID_FINE * div;
    t->q = XTAL / TUNE_LO_GRID_FINE;
    t->offset_hz = best;
    t->nco_step = (int32_t)(((int64_t)best << 32) / FS_IQ);
    return 1;
}
