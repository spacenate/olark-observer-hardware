/* Name: main.c
 * Project: Olark Observer Hardware
 * Author: Nathan Sollenberger
 * Creation Date: 2016-01-03
 * Copyright: (c) 2016 Nathan Sollenberger
 * License: GPL
 */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usbdrv.h"

#define CUSTOM_RX_STATUS_OFF 0x00
#define CUSTOM_RX_STATUS_AVAIL 0x01
#define CUSTOM_RX_STATUS_UNAVAIL 0x02
#define CUSTOM_RX_STATUS_MAXCHATS 0x03
#define CUSTOM_RX_STATUS_UNREAD 0x04
#define CUSTOM_RX_CONFIRM 0x22

/* ---------------------- Interface with V-USB driver ---------------------- */

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t *request = (void *)data;
static uchar outputBuffer[2];
uint8_t msgLen = 0;

    switch (request->bRequest) {
        case CUSTOM_RX_STATUS_OFF:
        case CUSTOM_RX_STATUS_UNAVAIL:
            PORTB |= (1 << PB1);
            outputBuffer[0] = CUSTOM_RX_CONFIRM;
            outputBuffer[1] = request->bRequest;
            msgLen = 2;
            break;

        case CUSTOM_RX_STATUS_AVAIL:
        case CUSTOM_RX_STATUS_MAXCHATS:
        case CUSTOM_RX_STATUS_UNREAD:
            PORTB &= ~(1 << PB1);
            outputBuffer[0] = CUSTOM_RX_CONFIRM;
            outputBuffer[1] = request->bRequest;
            msgLen = 2;
            break;

        default:
            break;
    }
    usbMsgPtr = outputBuffer;
    return msgLen;
}

/* --------------------- Calibrate internal oscillator --------------------- */

#define abs(x) ((x) > 0 ? (x) : (-x))
// Called by V-USB after device reset
void usbEventResetReady(void)
{
int frameLength, targetLength = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);
int bestDeviation = 9999;
uchar trialCal, bestCal, step, region;
    // do a binary search in regions 0-127 and 128-255 to get optimum OSCCAL
    for(region = 0; region <= 1; region++) {
        frameLength = 0;
        trialCal = (region == 0) ? 0 : 128;
        for(step = 64; step > 0; step >>= 1) {
            if(frameLength < targetLength) // true for initial iteration
                trialCal += step; // frequency too low
            else
                trialCal -= step; // frequency too high
            OSCCAL = trialCal;
            frameLength = usbMeasureFrameLength();
            if(abs(frameLength-targetLength) < bestDeviation) {
                bestCal = trialCal; // new optimum found
                bestDeviation = abs(frameLength -targetLength);
            }
        }
    }
    OSCCAL = bestCal;
}

/* --------------------------------- Main ---------------------------------- */

int main(void)
{
uchar i;

    usbInit();
    usbDeviceDisconnect();
    for (i=0;i<20;i++) {  /* 300 ms disconnect */
        _delay_ms(15);
    }
    usbDeviceConnect();
    wdt_enable(WDTO_1S);
    sei();
    DDRB |= (1 << PB1); /* led output, active LOW */
    PORTB |= (1 << PB1);
    for(;;){
        wdt_reset();
        usbPoll();
    }
    return 0;
}