#ifndef __EC11__
#define __EC11__

void EC11_Init();
int EC11_TakeSteps(void);   // detents turned since the last call (sign = direction)
int EC11_ButtonDown(void);  // raw switch state, not debounced

#endif
