#ifndef __USBD_H
#define __USBD_H

#include <stm32f10x.h>

#include "usb_lib.h"

typedef enum _RESUME_STATE {
    RESUME_EXTERNAL,
    RESUME_INTERNAL,
    RESUME_LATER,
    RESUME_WAIT,
    RESUME_START,
    RESUME_ON,
    RESUME_OFF,
    RESUME_ESOF
} RESUME_STATE;

typedef enum _DEVICE_STATE {
    UNCONNECTED,
    ATTACHED,
    POWERED,
    SUSPENDED,
    ADDRESSED,
    CONFIGURED
} DEVICE_STATE;

void Suspend(void);
void Resume_Init(void);
void Resume(RESUME_STATE eResumeSetVal);
RESULT PowerOn(void);
RESULT PowerOff(void);
void Enter_LowPowerMode(void);
void Leave_LowPowerMode(void);
void USB_Cable_Config(FunctionalState NewState);
void USB_Interrupts_Config(void);
void Set_USBClock(void);
void Set_System(void);

extern __IO uint32_t bDeviceState; /* USB device status */

#endif /* __USBD_H */
