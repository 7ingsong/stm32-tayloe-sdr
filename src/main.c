#include "utils.h"
#include "iq.h"
#include "i2s.h"
#include "si5351.h"
#include "mic.h"
#include "ssb_rx.h"

int main() {
    si5351_init();
    si5351_clk2_8mhz();

    clock_init();

    iq_set_frequency(LO_FREQ_DEFAULT);

    iq_init();

    mic_init(); // ADC3 on PA3, paced by the TIM3 started in iq_init()
    mic_start();

    i2s_init();
    i2s_start();

    while (1){
        iq_dispatch();
        i2s_dispatch();
        mic_dispatch();
        ssb_rx_poll();
    }

    return 0;
}
