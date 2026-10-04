#include "utils.h"
#include "iq.h"
#include "si5351.h"

int main() {
    si5351_init();
    si5351_clk2_8mhz();
    
    clock_init();

    si5351_set_frequency(10000000, SI5351_DRIVE_STRENGTH_2MA);

    iq_init();
        
    while (1){
        iq_dispatch();
    }

    return 0;
}
