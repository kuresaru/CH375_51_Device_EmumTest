#include "main.h"
#include "uart.h"

#define DATA P2
#define INTn P3_6
#define A0 P3_7
#define CSn P4_1
#define RDn P4_2
#define WRn P4_4

uint8_t __code DeviceDescriptor[18] = {
    18,             // bLength
    0x01,           // bDescriptorType
    0x00, 0x02,     // bcdUSB
    0x00,           // bDeviceClass
    0x00,           // bDeviceSubClass
    0x00,           // bDeviceProtocol
    8,              // bMaxPacketSize0
    0x34, 0x12,     // idVentor
    0x78, 0x56,     // idProduct
    0x01, 0x00,     // bcdDevice
    0,              // iManufacturer
    0,              // iProduct
    0,              // iSerialNumber
    1,              // bNumConfiguration
};

#define wVal(x) ((x) & 0xFF), (((x) >> 8) & 0xFF)
#define CFGDES_SIZE (9)

uint8_t __code ConfigurationDescriptor[CFGDES_SIZE] = {
    9,                  // bLength
    0x02,               // bDescriptorType
    wVal(CFGDES_SIZE),  // wTotalLength
    0,                  // bNumInterfaces
    1,                  // bConfigurationValue
    0,                  // iConfiguration
    0x80,               // bmAttributes
    250,                // bMaxPower
};

uint8_t CurrentSetupRequest = 0;
const uint8_t *CurrentDescriptor;
uint8_t CurrentDescriptor_Sent = 0;
uint8_t CurrentDescriptor_Size = 0;
uint8_t DeviceAddress = 0;

void Delay30ms()		//@22.1184MHz
{
	unsigned char i, j, k;

	_nop_();
	_nop_();
	i = 3;
	j = 134;
	k = 115;
	do
	{
		do
		{
			while (--k);
		} while (--j);
	} while (--i);
}

void Delay1us()		//@22.1184MHz
{
	unsigned char i;

	i = 3;
	while (--i);
}

void wr_cmd(uint8_t cmd)
{
    P2M0 = 0xFF;
    A0 = 1;
    DATA = cmd;
    Delay1us();
    WRn = 0;
    Delay1us();
    WRn = 1;
    Delay1us();
    A0 = 0;
    DATA = 0xFF;
    Delay1us();
}

void wr_data(uint8_t data)
{
    P2M0 = 0xFF;
    A0 = 0;
    DATA = data;
    Delay1us();
    WRn = 0;
    Delay1us();
    WRn = 1;
    Delay1us();
    A0 = 0;
    DATA = 0xFF;
    Delay1us();
}

void rd_data(uint8_t *data)
{
    P2M0 = 0x00;
    DATA = 0xFF;
    A0 = 0;
    Delay1us();
    RDn = 0;
    Delay1us();
    *data = DATA;
    RDn = 1;
    Delay1us();
    A0 = 0;
    Delay1us();
}

void sysinit()
{
    P2M1 = 0b00000000;
    P2M0 = 0b00000000;
    P3M1 = 0b00000000;
    P3M0 = 0b10000000;
    P4M1 = 0b00000000;
    P4M0 = 0b00010110;

    EA = 1;
    ES = 1;
    UartInit();

    CSn = 1;
    DATA = 0xFF;
    INTn = 1;
    RDn = 1;
    WRn = 1;
    A0 = 0;

    print("start...\r\n");
    Delay30ms();
    Delay30ms();
    Delay30ms();
    Delay30ms();
}

uint8_t poll_interrupt()
{
    uint8_t i;
    while (INTn)
    {
    }
    wr_cmd(0x22); // get status
    rd_data(&i);
    return i;
}

void main()
{
    uint8_t i, len;
    uint8_t buf[8];

    sysinit();

    CSn = 0;
    wr_cmd(0x05);
    CSn = 1;
    Delay30ms();
    Delay30ms();

    CSn = 0;

    wr_cmd(0x06);
    wr_data(0x57);
    rd_data(&i);
    haltif(i != 0xA8, "check exist error");
    print("check ok\r\n");

    wr_cmd(0x15);
    wr_data(0x01);
    for (;;)
    {
        rd_data(&i);
        if (i == 0x51)
        {
            break;
        }
    }
    print("set mode ok\r\n");

    while (1)
    {
        i = poll_interrupt();
        print("INT=");
        print_8x(i);
        print("\r\n");
        switch (i)
        {
        case 0x0C: // ep0 setup
            wr_cmd(0x28); // rd usb data
            rd_data(&i);  // read length
            if (i == 8)   // 数据长度一定是8
            {
                // 读8字节的数据
                for (i = 0; i < 8; i++)
                {
                    rd_data(buf + i);
                }
                CurrentSetupRequest = buf[1]; // bRequest
                print("SETUP=");
                print_8x(CurrentSetupRequest);
                print("\r\n");
                switch (CurrentSetupRequest)
                {
                case 0x06: // get descriptor
                    i = buf[3]; // descriptor type
                    print("get desc ");
                    print_8x(i);
                    print("\r\n");
                    switch (i)
                    {
                    case 0x01: // device descriptor
                        CurrentDescriptor = DeviceDescriptor;
                        CurrentDescriptor_Size = 18;
                        print("send device desc\r\n");
                        break;
                    case 0x02: // configuration descriptor
                        CurrentDescriptor = ConfigurationDescriptor;
                        CurrentDescriptor_Size = CFGDES_SIZE;
                        print("send config desc\r\n");
                        break;
                    }
                    wr_cmd(0x29); // wr usb data3
                    wr_data(8);
                    for (i = 0; i < 8; i++)
                    {
                        wr_data(CurrentDescriptor[i]);
                    }
                    CurrentDescriptor_Sent = 8;
                    wr_cmd(0x23); // unlock usb
                    break;
                case 0x05: // set address
                    DeviceAddress = buf[2];
                    wr_cmd(0x29); // wr usb data3
                    wr_data(0);
                    wr_cmd(0x23); // unlock usb
                    break;
                }
            }
            break;
        case 0x08: // ep0 in
            wr_cmd(0x23); // unlock usb
            if ((CurrentSetupRequest == 0x06) && (CurrentDescriptor))
            {
                len = CurrentDescriptor_Size - CurrentDescriptor_Sent;
                len = (len > 8) ? 8 : len;
                wr_cmd(0x29); // wr usb data3
                wr_data(len);
                for (i = 0; i < len; i++)
                {
                    wr_data(CurrentDescriptor[CurrentDescriptor_Sent]);
                    CurrentDescriptor_Sent++;
                }
                print("send desc");
            }
            else if (CurrentSetupRequest == 0x05)
            {
                wr_cmd(0x13);
                wr_data(DeviceAddress);
                print("set address ");
                print_8d(DeviceAddress);
                print("\r\n");
            }
            break;
        case 0x00: // ep0 out
            wr_cmd(0x23); // unlock usb
            break;
        default:
            if ((i & 0x03) == 0x03)
            {
                print("bus reset\r\n");
            }
            wr_cmd(0x23); // unlock usb
            rd_data(&i);  // dummy
            break;
        }
    }
}

void Uart_IRQ() __interrupt(4) __using(1)
{
    extern __bit busy;
    if (RI)
    {
        RI = 0;
        if (SBUF == 'd')
        {

        }
    }
    if (TI)
    {
        TI = 0;
        busy = 0;
    }
}