#ifndef _ILI9488
#define _ILI9488
#include <stdint.h>

/*
 * 3.5" ILI9488 480x320 TFT on SPI2 (the former I2S2 header H3):
 *   PB13 = SCK, PB15 = MOSI (SDI), PB14 = D/C, PB12 = CS. RESET is not on a GPIO: power-on reset /
 *   NRST plus the SWRESET command in ili9488_init(). No MISO, so nothing is ever read back.
 *
 * Over SPI the ILI9488 only takes 18-bit (3 bytes per pixel) or 3-bit (8 colours, 2 pixels per byte) colour;
 * GRAM is full colour either way, so the two can be mixed per area (ili9488_set_format()).
 * Pixel data goes out by DMA (DMA1 Channel5) so the main loop isn't blocked; poll ili9488_busy().
 */

// 0 = portrait 320x480 (hardware scroll runs top to bottom: waterfall), 1 = landscape 480x320
#ifndef ILI9488_LANDSCAPE
#define ILI9488_LANDSCAPE 0
#endif

#if ILI9488_LANDSCAPE
#define ILI9488_WIDTH  480
#define ILI9488_HEIGHT 320
#else
#define ILI9488_WIDTH  320
#define ILI9488_HEIGHT 480
#endif

typedef enum {
    ILI9488_RGB666 = 0x66, // 3 bytes per pixel: R, G, B in the top 6 bits of each byte
    ILI9488_RGB111 = 0x61, // 1 byte per 2 pixels: 00RGBrgb (first pixel in bits 5..3)
} ili9488_format_t;

// 3-bit colours (RGB111)
enum { C3_BLACK = 0, C3_BLUE = 1, C3_GREEN = 2, C3_CYAN = 3, C3_RED = 4, C3_MAGENTA = 5, C3_YELLOW = 6, C3_WHITE = 7 };

void ili9488_init(void);  // blocking, ~300 ms (resets and sleep-out delays); call at boot
int ili9488_busy(void);   // 1 while a DMA pixel transfer is in flight

// All of these wait for a previous transfer to finish first
void ili9488_command(uint8_t cmd, const uint8_t *params, uint16_t n);
void ili9488_set_format(ili9488_format_t format);
void ili9488_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1); // then send pixels
void ili9488_write_dma(const uint8_t *data, uint16_t len); // starts DMA, returns immediately
void ili9488_write(const uint8_t *data, uint16_t len);     // blocking

// Blocking helper for drawing at startup
void ili9488_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t r, uint8_t g, uint8_t b);

#endif
