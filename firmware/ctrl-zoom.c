/*
 * LANC receiver for ATtiny{2,4,8}5.
 * Based on:
 * Simple ATtiny USI UART <http://www.technoblogy.com/show?VSX>
 *
 * Pinout:
 *   5 = PB0 (DI)  LANC data
 *   6 = PB1       driver inputs 1 & 4
 *   7 = PB2       driver inputs 2 & 3
 *   3 = PB4       driver "enable" inputs
 *   2 = PB3       LED
 * Pin numbers are for the DIP8 package.
 *
 * Timer configuration:
 *  - TIM0_COMPB fires an interrupt at every bit transition, even when
 *    there is no transmission
 *  - TIM0_COMPA clocks the USI, the interrupt is not enabled
 * __        ____  ____  ____  ____  ____  ____  ____  ____  _______
 *   \      / D0 \/ D1 \/ D2 \/ D3 \/ D4 \/ D5 \/ D6 \/ D7 \/
 *    \____/\____/\____/\____/\____/\____/\____/\____/\____/
 *   B  A  B  A  B
 *
 * Timings:
 *   1 bit   =    104 us (9615 bps, i.e. 0.16% too fast)
 *   1 byte  =  1.248 ms = 12 bits (1 start, 8 data, 3 stop)
 *   1 frame = 19.968 ms = 16 bytes (4 rx-ed, 4 tx-ed, 8 skipped)
 */

#define F_CPU 8000000
#include <stdbool.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#define PIN_DATA PB0
#define PIN_FW   PB1
#define PIN_REV  PB2
#define PIN_EN   PB4  /* OC1B */
#define PIN_LED  PB3

/* Available motor speeds. */
static __flash const uint8_t speeds[8] = {
    2, 4, 8, 16, 32, 64, 128, 255
};

uint8_t byte_cnt, bit_cnt;  /* byte and bit counters, could be fused */
volatile uint8_t recvd[2];    /* bytes received */
volatile bool data_received;  /* boolean */


/***********************************************************************
 * USI LANC driver.
 */

unsigned char reverse_byte(unsigned char x) {
    x = ((x >> 1) & 0x55) | ((x << 1) & 0xaa);
    x = ((x >> 2) & 0x33) | ((x << 2) & 0xcc);
    x = ((x >> 4) & 0x0f) | ((x << 4) & 0xf0);
    return x;
}

void init_usi_lanc(void) {
    DDRB   |= _BV(PIN_DATA);  /* define DI as output */
    TCCR0A  = _BV(WGM01);     /* timer in CTC mode */
    TCCR0B  = _BV(CS01);      /* prescaler to /8 */
    OCR0A   = 104 - 1;        /* shift every 104 us */
    OCR0B   = 104/2 - 1;      /* COMPB between successive COMPA */
    TIFR    = _BV(OCF0B);     /* clear output compare flag */
    TIMSK  |= _BV(OCIE0B);    /* enable output compare interrupt */
}

/* At every bit transition, even during the pause. */
ISR(TIM0_COMPB_vect)
{
    /* Are we in the pause? */
    if (byte_cnt & 8) goto done;

    /* Begin start bit. */
    if (bit_cnt == 0) {
        PORTB &= ~_BV(PIN_DATA);  /* floating */
        DDRB  |=  _BV(PIN_DATA);  /* output low */
    }

    /* End start bit. */
    else if (bit_cnt == 1) {

        /* Here we may want to transmit data instead... */
        DDRB  &= ~_BV(PIN_DATA);  /* floating */
        PORTB |=  _BV(PIN_DATA);  /* enable pullup */

        /* Start the USI receiver on bytes 0 and 1. */
        if (byte_cnt < 2) {
            USICR = _BV(USIOIE) | _BV(USICS0);
            USISR = _BV(USIOIF) | 8;
        }
    }

done:
    /* Increment (byte_cnt, bit_cnt) before returning. */
    if (++bit_cnt >= 12) {
        byte_cnt = (byte_cnt + 1) % 16;
        bit_cnt = 0;
    }
}

/* When a complete byte is received. */
ISR(USI_OVF_vect) {
    USICR = 0;                      /* Disable USI */
    recvd[byte_cnt] = ~reverse_byte(USIBR);
    if (byte_cnt == 1) data_received = true;
}


/***********************************************************************
 * Main program.
 */

/* Power the motor. */
static void control_motor()
{
    uint8_t output = 0;
    if (recvd[0] == 0x28) {  /* move */
        if (recvd[1] & 0x10)
            output = _BV(PIN_REV);  /* wide */
        else
            output = _BV(PIN_FW);  /* tele */
        output |= _BV(PIN_LED);
        OCR1B = speeds[recvd[1] >> 1 & 7];
    } else {
        OCR1B = 0;
    }
    PORTB = output;
}

int main(void)
{
    /* Set main clock prescaler to 1 => f_CPU = 8 MHz. */
    CLKPR = 1<<CLKPCE;
    CLKPR = 0;

    /* Configure Timer 1 for PWM on PIN_EN = PB4 = OC1B */
    GTCCR = _BV(PWM1B)    /* enable PWM on OC1B */
          | _BV(COM1B1);  /* non-inverting, nOC1B not connected */
    TCCR1 = _BV(CS10);    /* clock @ F_CPU */

    init_usi_lanc();
    sei();

    /* Configure outputs. */
    DDRB = _BV(PIN_FW)
         | _BV(PIN_REV)
         | _BV(PIN_EN)
         | _BV(PIN_LED);

    /* Main loop. */
    for (;;) {
        if (data_received) {
            control_motor();
            data_received = false;
        }
        sleep_mode();
    }
}
