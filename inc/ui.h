#ifndef _UI
#define _UI
#include <stdint.h>

/*
 * Front panel: 0.91" SSD1306 OLED (128x32, I2C1 shared with the Si5351) and the EC11 encoder.
 * Turn = tune the LO by the current step, short push = next step (10 Hz .. 1 MHz),
 * long push = toggle RX/TX (from duplex: back to RX; duplex itself is set with CMD_PTT).
 * Shows the frequency (step digit underlined) and RX/TX; host changes (CMD_SET_FREQ, CMD_PTT) show up too.
 */

// Set to 1 if turning clockwise tunes down
#ifndef UI_ENCODER_REVERSE
#define UI_ENCODER_REVERSE 0
#endif

// 1 = switch the OLED off while transmitting. Its row scan (~312 Hz) pulls pulsed current from 3.3 V and
// the ripple reached the mic preamp as a hum comb (+17 dB). Off by default while testing an RC filter on the
// preamp supply; set to 1 if the hum is back.
#ifndef UI_BLANK_ON_TX
#define UI_BLANK_ON_TX 0
#endif

// Hold the encoder button this long to toggle RX/TX; it fires while still held
// Second press within this time after a release = double press (USB/LSB); a single press acts only after it
#ifndef UI_DOUBLE_PRESS_MS
#define UI_DOUBLE_PRESS_MS 300
#endif

#ifndef UI_LONG_PRESS_MS
#define UI_LONG_PRESS_MS 700
#endif

uint32_t ui_get_frequency(void);  // the LO the user dialled (shown at once, retune follows within ~20 ms)
const char *ui_get_step_name(void);

void ui_init(void);  // after clock_init() and iq_set_frequency(); blocks ~20 ms to bring up the display
void ui_poll(void);  // every main-loop pass; never blocks for more than one ~0.5 ms I2C chunk

#endif
