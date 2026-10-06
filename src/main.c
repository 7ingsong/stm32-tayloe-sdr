#include "utils.h"
#include "iq.h"
#include "i2s.h"
#include "si5351.h"

int main() {
    si5351_init();
    si5351_clk2_8mhz();

    clock_init();

    iq_set_frequency(LO_FREQ_DEFAULT);

    iq_init();

    i2s_init();
    i2s_start();

    while (1){
        iq_dispatch();
        i2s_dispatch();
    }

    return 0;
}
