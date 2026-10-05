#include <math.h>
#include "utils.h"
#include "iq.h"
#include "i2s.h"
#include "si5351.h"

// I2S TX test tone for headphones
#define SINE_FREQ_HZ   2000
#define SINE_AMPLITUDE (0x7FFFFF / 32) // -30 dBFS, NS4168 BTL into headphones is loud
#define SINE_LUT_BITS  8

static int32_t sine_lut[1 << SINE_LUT_BITS];
static uint32_t sine_phase = 0;
static const uint32_t sine_step = (uint32_t)(((uint64_t)SINE_FREQ_HZ << 32) / I2S_FS);

void sine_init() {
    for (int i = 0; i < (1 << SINE_LUT_BITS); i++) {
        sine_lut[i] = (int32_t)(SINE_AMPLITUDE * sinf(2.0f * (float)M_PI * i / (1 << SINE_LUT_BITS)));
    }
}

void on_i2s(uint16_t *buf, int n) {
    for (int i = 0; i < n; i += 4) {
        int32_t s = sine_lut[sine_phase >> (32 - SINE_LUT_BITS)];
        sine_phase += sine_step;
        i2s_put24(&buf[i], s);     // L
        i2s_put24(&buf[i + 2], s); // R
    }
}

int main() {
    si5351_init();
    si5351_clk2_8mhz();

    clock_init();

    si5351_set_frequency(10000000, SI5351_DRIVE_STRENGTH_2MA);

    iq_init();

    // sine_init();
    // i2s_init();
    // i2s_start();

    while (1){
        iq_dispatch();
        // i2s_dispatch();
    }

    return 0;
}
