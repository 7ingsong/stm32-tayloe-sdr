#include "utils.h"
#include "iq.h"

int main() {
    
    clock_init();
    iq_init();
        
    while (1){
        iq_dispatch();
    }

    return 0;
}
