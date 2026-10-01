#include "si5351-ray.h"
#include "i2c.h"
#define SI5351_ADDRESS 0x60
#define I2C_HANDLE I2C1

#define SSD1306_128_32
#define SI5351A_ADDRESS        0xC0
#define Si5351A_XTAL_FREQ      24999117
#define SI_CLK0_CONTROL        16
#define SI_CLK1_CONTROL        17
#define SI_CLK2_CONTROL        18
#define SI_SYNTH_PLL_A         26
#define SI_SYNTH_PLL_B         34
#define SI_SYNTH_MS_0          42
#define SI_SYNTH_MS_1          50
#define SI_SYNTH_MS_2          58
#define SI_PLL_RESET           177
#define SI_R_DIV_1             0b00000000
#define SI_R_DIV_2             0b00010000
#define SI_R_DIV_4             0b00100000
#define SI_R_DIV_8             0b00110000
#define SI_R_DIV_16            0b01000000
#define SI_R_DIV_32            0b01010000
#define SI_R_DIV_64            0b01100000
#define SI_R_DIV_128           0b01110000
#define SI_CLK_SRC_PLL_A       0b00000000
#define SI_CLK_SRC_PLL_B       0b00100000
#define CLK_ENABLE_CONTROL     3
#define PLLX_SRC               15
#define XTAL_LOAD_CAP          183
#define CLK0_PHOFF             165
#define CLK1_PHOFF             166

void setupPLL(unsigned char pll, unsigned char mult, unsigned long num, unsigned long denom);
void setupMultisynth(unsigned char synth, unsigned long divider, unsigned char rDiv);

void sendRegister(char reg, char value) {
    I2C_WriteByte(SI5351_ADDRESS, reg, value);
}

void si5351aInit(unsigned long frequency){
    sendRegister(CLK_ENABLE_CONTROL, 0x04);
    sendRegister(SI_CLK0_CONTROL, 0x0F);
    sendRegister(SI_CLK1_CONTROL, 0x0F);
    si5351aSetFrequency(frequency);
    sendRegister(SI_PLL_RESET, 0xA0);
}

// Si5351A commands///////////////////////////////

void si5351aSetFrequency(unsigned long frequency){
    unsigned divider;
    unsigned long pllFreq;
    unsigned long xtalFreq = Si5351A_XTAL_FREQ;
    unsigned long l;
    float f;
    unsigned char mult;
    unsigned long num;
    unsigned long denom;
    if(frequency < 9050001)divider = 124;
    if(frequency > 9050000)divider = 44;
    pllFreq = divider * frequency;        // Calculate the pllFrequency: the divider * desired output frequency
    mult = pllFreq / xtalFreq;                // Determine the multiplier to get to the required pllFrequency
    l = pllFreq % xtalFreq;                        // It has three parts:
    f = l;                                                        // mult is an integer that must be in the range 15..90
    f *= 1048575;                                        // num and denom are the fractional parts, the numerator and denominator
    f /= xtalFreq;                                        // each is 20 bits (range 0..1048575)
    num = f;                                                // the actual multiplier is  mult + num / denom
    denom = 1048575;                                // For simplicity we set the denominator to the maximum 1048575
    setupPLL(SI_SYNTH_PLL_A, mult, num, denom);      // Set up PLL A with the calculated multiplication ratio
    setupMultisynth(SI_SYNTH_MS_0, divider, SI_R_DIV_1);
    setupMultisynth(SI_SYNTH_MS_1, divider, SI_R_DIV_1);
    sendRegister(CLK0_PHOFF,divider);
    sendRegister(CLK1_PHOFF, 0);
}
//////////////////////////////////////////////////////////////////////////////////////
void setupPLL(unsigned char pll, unsigned char mult, unsigned long num, unsigned long denom){
    unsigned long P1;                                        // PLL config register P1
    unsigned long P2;                                        // PLL config register P2
    unsigned long P3;                                        // PLL config register P3

    P1 = (unsigned long)(128 * ((float)num / (float)denom));
    P1 = (unsigned long)(128 * (unsigned long)(mult) + P1 - 512);
    P2 = (unsigned long)(128 * ((float)num / (float)denom));
    P2 = (unsigned long)(128 * num - denom * P2);
    P3 = denom;

    sendRegister(pll + 0, (P3 & 0x0000FF00) >> 8);
    sendRegister(pll + 1, (P3 & 0x000000FF));
    sendRegister(pll + 2, (P1 & 0x00030000) >> 16);
    sendRegister(pll + 3, (P1 & 0x0000FF00) >> 8);
    sendRegister(pll + 4, (P1 & 0x000000FF));
    sendRegister(pll + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
    sendRegister(pll + 6, (P2 & 0x0000FF00) >> 8);
    sendRegister(pll + 7, (P2 & 0x000000FF));
}
//////////////////////////////////////////////////////////////////////////////////////
void setupMultisynth(unsigned char synth, unsigned long divider, unsigned char rDiv){
    unsigned long P1;                                        // Synth config register P1
    unsigned long P2;                                        // Synth config register P2
    unsigned long P3;                                        // Synth config register P3
    P1 = 128 * divider - 512;
    P2 = 0; // P2 = 0, P3 = 1 forces an integer value for the divider
    P3 = 1;
    sendRegister(synth + 0,   (P3 & 0x0000FF00) >> 8);
    sendRegister(synth + 1,   (P3 & 0x000000FF));
    sendRegister(synth + 2,   ((P1 & 0x00030000) >> 16) | rDiv);
    sendRegister(synth + 3,   (P1 & 0x0000FF00) >> 8);
    sendRegister(synth + 4,   (P1 & 0x000000FF));
    sendRegister(synth + 5,   ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
    sendRegister(synth + 6,   (P2 & 0x0000FF00) >> 8);
    sendRegister(synth + 7,   (P2 & 0x000000FF));
}