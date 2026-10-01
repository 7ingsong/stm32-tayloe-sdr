#include "usb_prop.h"

#include "hw_config.h"
#include "usb_conf.h"
#include "usb_lib.h"
#include "usbd.h"

__attribute__((weak)) void OnUsbConfigured() {}
__attribute__((weak)) void OnUsbAttached() {}
__attribute__((weak)) void OnUsbAddressed() {}
__attribute__((weak)) void OnUsbUnconnected() {}

#define ID1 (0x1FFFF7E8)
#define ID2 (0x1FFFF7EC)
#define ID3 (0x1FFFF7F0)

#define SEND_ENCAPSULATED_COMMAND 0x00
#define GET_ENCAPSULATED_RESPONSE 0x01
#define SET_COMM_FEATURE 0x02
#define GET_COMM_FEATURE 0x03
#define CLEAR_COMM_FEATURE 0x04
#define SET_LINE_CODING 0x20
#define GET_LINE_CODING 0x21
#define SET_CONTROL_LINE_STATE 0x22
#define SEND_BREAK 0x23

#define USB_DEVICE_DESCRIPTOR_TYPE 0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE 0x02
#define USB_STRING_DESCRIPTOR_TYPE 0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE 0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE 0x05

#define VIRTUAL_COM_PORT_INT_SIZE 8

#define VIRTUAL_COM_PORT_SIZ_DEVICE_DESC 18
#define VIRTUAL_COM_PORT_SIZ_CONFIG_DESC 67
#define VIRTUAL_COM_PORT_SIZ_STRING_LANGID 4
#define VIRTUAL_COM_PORT_SIZ_STRING_VENDOR 38
#define VIRTUAL_COM_PORT_SIZ_STRING_PRODUCT 50
#define VIRTUAL_COM_PORT_SIZ_STRING_SERIAL 26

#define STANDARD_ENDPOINT_DESC_SIZE 0x09

const uint8_t Virtual_Com_Port_DeviceDescriptor[VIRTUAL_COM_PORT_SIZ_DEVICE_DESC];
const uint8_t Virtual_Com_Port_ConfigDescriptor[VIRTUAL_COM_PORT_SIZ_CONFIG_DESC];

const uint8_t Virtual_Com_Port_StringLangID[VIRTUAL_COM_PORT_SIZ_STRING_LANGID];
const uint8_t Virtual_Com_Port_StringVendor[VIRTUAL_COM_PORT_SIZ_STRING_VENDOR];
const uint8_t Virtual_Com_Port_StringProduct[VIRTUAL_COM_PORT_SIZ_STRING_PRODUCT];
uint8_t Virtual_Com_Port_StringSerial[VIRTUAL_COM_PORT_SIZ_STRING_SERIAL];

const uint8_t Virtual_Com_Port_DeviceDescriptor[] = {
    0x12,                       /* bLength */
    USB_DEVICE_DESCRIPTOR_TYPE, /* bDescriptorType */
    0x00,
    0x02, /* bcdUSB = 2.00 */
    0x02, /* bDeviceClass: CDC */
    0x02, /* bDeviceSubClass */
    0x02, /* bDeviceProtocol */
    0x40, /* bMaxPacketSize0 */
    0x83,
    0x04, /* idVendor = 0x0483 */
    0x40,
    0x57, /* idProduct = 0x7540 */
    0x00,
    0x02, /* bcdDevice = 2.00 */
    1,    /* Index of string descriptor describing manufacturer */
    2,    /* Index of string descriptor describing product */
    3,    /* Index of string descriptor describing the device's serial number */
    0x01  /* bNumConfigurations */
};

const uint8_t Virtual_Com_Port_ConfigDescriptor[] = {
    /*Configuration Descriptor*/
    0x09,                              /* bLength: Configuration Descriptor size */
    USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType: Configuration */
    VIRTUAL_COM_PORT_SIZ_CONFIG_DESC,  /* wTotalLength:no of returned bytes */
    0x00, 0x02,                        /* bNumInterfaces: 2 interface */
    0x01,                              /* bConfigurationValue: Configuration value */
    0x00, /* iConfiguration: Index of string descriptor describing the configuration */
    0xC0, /* bmAttributes: self powered */
    0x32, /* MaxPower 0 mA */
    /*Interface Descriptor*/
    0x09,                          /* bLength: Interface Descriptor size */
    USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType: Interface */
    /* Interface descriptor type */
    0x00, /* bInterfaceNumber: Number of Interface */
    0x00, /* bAlternateSetting: Alternate setting */
    0x01, /* bNumEndpoints: One endpoints used */
    0x02, /* bInterfaceClass: Communication Interface Class */
    0x02, /* bInterfaceSubClass: Abstract Control Model */
    0x01, /* bInterfaceProtocol: Common AT commands */
    0x00, /* iInterface: */
    /*Header Functional Descriptor*/
    0x05, /* bLength: Endpoint Descriptor size */
    0x24, /* bDescriptorType: CS_INTERFACE */
    0x00, /* bDescriptorSubtype: Header Func Desc */
    0x10, /* bcdCDC: spec release number */
    0x01,
    /*Call Management Functional Descriptor*/
    0x05, /* bFunctionLength */
    0x24, /* bDescriptorType: CS_INTERFACE */
    0x01, /* bDescriptorSubtype: Call Management Func Desc */
    0x00, /* bmCapabilities: D0+D1 */
    0x01, /* bDataInterface: 1 */
    /*ACM Functional Descriptor*/
    0x04, /* bFunctionLength */
    0x24, /* bDescriptorType: CS_INTERFACE */
    0x02, /* bDescriptorSubtype: Abstract Control Management desc */
    0x02, /* bmCapabilities */
    /*Union Functional Descriptor*/
    0x05, /* bFunctionLength */
    0x24, /* bDescriptorType: CS_INTERFACE */
    0x06, /* bDescriptorSubtype: Union func desc */
    0x00, /* bMasterInterface: Communication class interface */
    0x01, /* bSlaveInterface0: Data Class Interface */
    /*Endpoint 2 Descriptor*/
    0x07,                         /* bLength: Endpoint Descriptor size */
    USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType: Endpoint */
    0x82,                         /* bEndpointAddress: (IN2) */
    0x03,                         /* bmAttributes: Interrupt */
    VIRTUAL_COM_PORT_INT_SIZE,    /* wMaxPacketSize: */
    0x00, 0xFF,                   /* bInterval: */
    /*Data class interface descriptor*/
    0x09,                          /* bLength: Endpoint Descriptor size */
    USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType: */
    0x01,                          /* bInterfaceNumber: Number of Interface */
    0x00,                          /* bAlternateSetting: Alternate setting */
    0x02,                          /* bNumEndpoints: Two endpoints used */
    0x0A,                          /* bInterfaceClass: CDC */
    0x00,                          /* bInterfaceSubClass: */
    0x00,                          /* bInterfaceProtocol: */
    0x00,                          /* iInterface: */
    /*Endpoint 3 Descriptor*/
    0x07,                         /* bLength: Endpoint Descriptor size */
    USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType: Endpoint */
    0x03,                         /* bEndpointAddress: (OUT3) */
    0x02,                         /* bmAttributes: Bulk */
    VIRTUAL_COM_PORT_DATA_SIZE,   /* wMaxPacketSize: */
    0x00, 0x00,                   /* bInterval: ignore for Bulk transfer */
    /*Endpoint 1 Descriptor*/
    0x07,                         /* bLength: Endpoint Descriptor size */
    USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType: Endpoint */
    0x81,                         /* bEndpointAddress: (IN1) */
    0x02,                         /* bmAttributes: Bulk */
    VIRTUAL_COM_PORT_DATA_SIZE,   /* wMaxPacketSize: */
    0x00, 0x00                    /* bInterval */
};

const uint8_t Virtual_Com_Port_StringLangID[VIRTUAL_COM_PORT_SIZ_STRING_LANGID] = {
    VIRTUAL_COM_PORT_SIZ_STRING_LANGID, USB_STRING_DESCRIPTOR_TYPE, 0x09,
    0x04 /* LangID = 0x0409: U.S. English */
};

const uint8_t Virtual_Com_Port_StringVendor[VIRTUAL_COM_PORT_SIZ_STRING_VENDOR] = {
    VIRTUAL_COM_PORT_SIZ_STRING_VENDOR, /* Size of Vendor string */
    USB_STRING_DESCRIPTOR_TYPE,         /* bDescriptorType*/
    /* Manufacturer: "STMicroelectronics" */
    'S', 0, 'T', 0, 'M', 0, 'i', 0, 'c', 0, 'r', 0, 'o', 0, 'e', 0, 'l', 0, 'e', 0, 'c', 0, 't', 0,
    'r', 0, 'o', 0, 'n', 0, 'i', 0, 'c', 0, 's', 0};

const uint8_t Virtual_Com_Port_StringProduct[VIRTUAL_COM_PORT_SIZ_STRING_PRODUCT] = {
    VIRTUAL_COM_PORT_SIZ_STRING_PRODUCT, /* bLength */
    USB_STRING_DESCRIPTOR_TYPE,          /* bDescriptorType */
    /* Product name: "STM32 Virtual COM Port" */
    'S', 0, 'T', 0, 'M', 0, '3', 0, '2', 0, ' ', 0, 'V', 0, 'i', 0, 'r', 0, 't', 0, 'u', 0, 'a', 0,
    'l', 0, ' ', 0, 'C', 0, 'O', 0, 'M', 0, ' ', 0, 'P', 0, 'o', 0, 'r', 0, 't', 0, ' ', 0, ' ', 0};

uint8_t Virtual_Com_Port_StringSerial[VIRTUAL_COM_PORT_SIZ_STRING_SERIAL] = {
    VIRTUAL_COM_PORT_SIZ_STRING_SERIAL, /* bLength */
    USB_STRING_DESCRIPTOR_TYPE,         /* bDescriptorType */
    'S',
    0,
    'T',
    0,
    'M',
    0,
    '3',
    0,
    '2',
    0};

typedef struct {
    uint32_t bitrate;
    uint8_t format;
    uint8_t paritytype;
    uint8_t datatype;
} LINE_CODING;

#define Virtual_Com_Port_GetConfiguration NOP_Process
// #define Virtual_Com_Port_SetConfiguration          NOP_Process
#define Virtual_Com_Port_GetInterface NOP_Process
#define Virtual_Com_Port_SetInterface NOP_Process
#define Virtual_Com_Port_GetStatus NOP_Process
#define Virtual_Com_Port_ClearFeature NOP_Process
#define Virtual_Com_Port_SetEndPointFeature NOP_Process
#define Virtual_Com_Port_SetDeviceFeature NOP_Process
// #define Virtual_Com_Port_SetDeviceAddress          NOP_Process

uint8_t* Virtual_Com_Port_GetLineCoding(uint16_t Length);
uint8_t* Virtual_Com_Port_SetLineCoding(uint16_t Length);

void Virtual_Com_Port_init(void);
void Virtual_Com_Port_Reset(void);
void Virtual_Com_Port_SetConfiguration(void);
void Virtual_Com_Port_SetDeviceAddress(void);
void Virtual_Com_Port_Status_In(void);
void Virtual_Com_Port_Status_Out(void);
RESULT Virtual_Com_Port_Data_Setup(uint8_t);
RESULT Virtual_Com_Port_NoData_Setup(uint8_t);
RESULT Virtual_Com_Port_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting);
uint8_t* Virtual_Com_Port_GetDeviceDescriptor(uint16_t);
uint8_t* Virtual_Com_Port_GetConfigDescriptor(uint16_t);
uint8_t* Virtual_Com_Port_GetStringDescriptor(uint16_t);

uint8_t Request = 0;

LINE_CODING linecoding = {
    115200, /* baud rate*/
    0x00,   /* stop bits-1*/
    0x00,   /* parity - none*/
    0x08    /* no. of bits 8*/
};

DEVICE Device_Table = {EP_NUM, 1};

DEVICE_PROP Device_Property = {
    Virtual_Com_Port_init,
    Virtual_Com_Port_Reset,
    Virtual_Com_Port_Status_In,
    Virtual_Com_Port_Status_Out,
    Virtual_Com_Port_Data_Setup,
    Virtual_Com_Port_NoData_Setup,
    Virtual_Com_Port_Get_Interface_Setting,
    Virtual_Com_Port_GetDeviceDescriptor,
    Virtual_Com_Port_GetConfigDescriptor,
    Virtual_Com_Port_GetStringDescriptor,
    0,
    0x40 /*MAX PACKET SIZE*/
};

USER_STANDARD_REQUESTS User_Standard_Requests = {
    Virtual_Com_Port_GetConfiguration,   Virtual_Com_Port_SetConfiguration,
    Virtual_Com_Port_GetInterface,       Virtual_Com_Port_SetInterface,
    Virtual_Com_Port_GetStatus,          Virtual_Com_Port_ClearFeature,
    Virtual_Com_Port_SetEndPointFeature, Virtual_Com_Port_SetDeviceFeature,
    Virtual_Com_Port_SetDeviceAddress};

ONE_DESCRIPTOR Device_Descriptor = {(uint8_t*)Virtual_Com_Port_DeviceDescriptor,
                                    VIRTUAL_COM_PORT_SIZ_DEVICE_DESC};

ONE_DESCRIPTOR Config_Descriptor = {(uint8_t*)Virtual_Com_Port_ConfigDescriptor,
                                    VIRTUAL_COM_PORT_SIZ_CONFIG_DESC};

ONE_DESCRIPTOR String_Descriptor[4] = {
    {(uint8_t*)Virtual_Com_Port_StringLangID, VIRTUAL_COM_PORT_SIZ_STRING_LANGID},
    {(uint8_t*)Virtual_Com_Port_StringVendor, VIRTUAL_COM_PORT_SIZ_STRING_VENDOR},
    {(uint8_t*)Virtual_Com_Port_StringProduct, VIRTUAL_COM_PORT_SIZ_STRING_PRODUCT},
    {(uint8_t*)Virtual_Com_Port_StringSerial, VIRTUAL_COM_PORT_SIZ_STRING_SERIAL}};

static void IntToUnicode(uint32_t value, uint8_t* pbuf, uint8_t len) {
    uint8_t idx = 0;

    for (idx = 0; idx < len; idx++) {
        if (((value >> 28)) < 0xA) {
            pbuf[2 * idx] = (value >> 28) + '0';
        } else {
            pbuf[2 * idx] = (value >> 28) + 'A' - 10;
        }

        value = value << 4;

        pbuf[2 * idx + 1] = 0;
    }
}

void Get_SerialNum(void) {
    uint32_t Device_Serial0, Device_Serial1, Device_Serial2;

    Device_Serial0 = *(uint32_t*)ID1;
    Device_Serial1 = *(uint32_t*)ID2;
    Device_Serial2 = *(uint32_t*)ID3;

    Device_Serial0 += Device_Serial2;

    if (Device_Serial0 != 0) {
        IntToUnicode(Device_Serial0, &Virtual_Com_Port_StringSerial[2], 8);
        IntToUnicode(Device_Serial1, &Virtual_Com_Port_StringSerial[18], 4);
    }
}

void Virtual_Com_Port_init(void) {
    /* Update the serial number string descriptor with the data from the unique
    ID*/
    Get_SerialNum();

    pInformation->Current_Configuration = 0;

    /* Connect the device */
    PowerOn();

    /* Perform basic device initialization operations */
    USB_SIL_Init();

    bDeviceState = UNCONNECTED;
    OnUsbUnconnected();
}

void Virtual_Com_Port_Reset(void) {
    /* Set Virtual_Com_Port DEVICE as not configured */
    pInformation->Current_Configuration = 0;

    /* Current Feature initialization */
    pInformation->Current_Feature = Virtual_Com_Port_ConfigDescriptor[7];

    /* Set Virtual_Com_Port DEVICE with the default Interface*/
    pInformation->Current_Interface = 0;

    SetBTABLE(BTABLE_ADDRESS);

    /* Initialize Endpoint 0 */
    SetEPType(ENDP0, EP_CONTROL);
    SetEPTxStatus(ENDP0, EP_TX_STALL);
    SetEPRxAddr(ENDP0, ENDP0_RXADDR);
    SetEPTxAddr(ENDP0, ENDP0_TXADDR);
    Clear_Status_Out(ENDP0);
    SetEPRxCount(ENDP0, Device_Property.MaxPacketSize);
    SetEPRxValid(ENDP0);

    /* Initialize Endpoint 1 */
    SetEPType(ENDP1, EP_BULK);
    SetEPTxAddr(ENDP1, ENDP1_TXADDR);
    SetEPTxStatus(ENDP1, EP_TX_NAK);
    SetEPRxStatus(ENDP1, EP_RX_DIS);

    /* Initialize Endpoint 2 */
    SetEPType(ENDP2, EP_INTERRUPT);
    SetEPTxAddr(ENDP2, ENDP2_TXADDR);
    SetEPRxStatus(ENDP2, EP_RX_DIS);
    SetEPTxStatus(ENDP2, EP_TX_NAK);

    /* Initialize Endpoint 3 */
    SetEPType(ENDP3, EP_BULK);
    SetEPRxAddr(ENDP3, ENDP3_RXADDR);
    SetEPRxCount(ENDP3, VIRTUAL_COM_PORT_DATA_SIZE);
    SetEPRxStatus(ENDP3, EP_RX_VALID);
    SetEPTxStatus(ENDP3, EP_TX_DIS);

    /* Set this device to response on default address */
    SetDeviceAddress(0);

    bDeviceState = ATTACHED;
    OnUsbAttached();
}

void Virtual_Com_Port_SetConfiguration(void) {
    DEVICE_INFO* pInfo = &Device_Info;

    if (pInfo->Current_Configuration != 0) {
        /* Device configured */
        bDeviceState = CONFIGURED;
        OnUsbConfigured();
    }
}

void Virtual_Com_Port_SetDeviceAddress(void) {
    bDeviceState = ADDRESSED;
    OnUsbAddressed();
}

void Virtual_Com_Port_Status_In(void) {
    if (Request == SET_LINE_CODING) {
        Request = 0;
    }
}

void Virtual_Com_Port_Status_Out(void) {}

RESULT Virtual_Com_Port_Data_Setup(uint8_t RequestNo) {
    uint8_t* (*CopyRoutine)(uint16_t);

    CopyRoutine = NULL;

    if (RequestNo == GET_LINE_CODING) {
        if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT)) {
            CopyRoutine = Virtual_Com_Port_GetLineCoding;
        }
    } else if (RequestNo == SET_LINE_CODING) {
        if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT)) {
            CopyRoutine = Virtual_Com_Port_SetLineCoding;
        }
        Request = SET_LINE_CODING;
    }

    if (CopyRoutine == NULL) {
        return USB_UNSUPPORT;
    }

    pInformation->Ctrl_Info.CopyData = CopyRoutine;
    pInformation->Ctrl_Info.Usb_wOffset = 0;
    (*CopyRoutine)(0);
    return USB_SUCCESS;
}

RESULT Virtual_Com_Port_NoData_Setup(uint8_t RequestNo) {
    if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT)) {
        if (RequestNo == SET_COMM_FEATURE) {
            return USB_SUCCESS;
        } else if (RequestNo == SET_CONTROL_LINE_STATE) {
            return USB_SUCCESS;
        }
    }

    return USB_UNSUPPORT;
}

uint8_t* Virtual_Com_Port_GetDeviceDescriptor(uint16_t Length) {
    return Standard_GetDescriptorData(Length, &Device_Descriptor);
}

uint8_t* Virtual_Com_Port_GetConfigDescriptor(uint16_t Length) {
    return Standard_GetDescriptorData(Length, &Config_Descriptor);
}

uint8_t* Virtual_Com_Port_GetStringDescriptor(uint16_t Length) {
    uint8_t wValue0 = pInformation->USBwValue0;
    if (wValue0 >= 4) {
        return NULL;
    } else {
        return Standard_GetDescriptorData(Length, &String_Descriptor[wValue0]);
    }
}

RESULT Virtual_Com_Port_Get_Interface_Setting(uint8_t Interface, uint8_t AlternateSetting) {
    if (AlternateSetting > 0) {
        return USB_UNSUPPORT;
    } else if (Interface > 1) {
        return USB_UNSUPPORT;
    }
    return USB_SUCCESS;
}

uint8_t* Virtual_Com_Port_GetLineCoding(uint16_t Length) {
    if (Length == 0) {
        pInformation->Ctrl_Info.Usb_wLength = sizeof(linecoding);
        return NULL;
    }
    return (uint8_t*)&linecoding;
}

uint8_t* Virtual_Com_Port_SetLineCoding(uint16_t Length) {
    if (Length == 0) {
        pInformation->Ctrl_Info.Usb_wLength = sizeof(linecoding);
        return NULL;
    }
    return (uint8_t*)&linecoding;
}
