#ifndef _UI
#define _UI

/*
 * Front panel: 0.91" SSD1306 OLED (128x32, I2C1 shared with the Si5351) and the EC11 encoder.
 * Turn = tune the LO by the current step, push = next step (10 Hz .. 1 MHz).
 * Shows the frequency (step digit underlined) and RX/TX; host changes (CMD_SET_FREQ, CMD_PTT) show up too.
 */

// Set to 1 if turning clockwise tunes down
#ifndef UI_ENCODER_REVERSE
#define UI_ENCODER_REVERSE 0
#endif

void ui_init(void);  // after clock_init() and iq_set_frequency(); blocks ~20 ms to bring up the display
void ui_poll(void);  // every main-loop pass; never blocks for more than one ~0.5 ms I2C chunk

#endif
