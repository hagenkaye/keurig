//
// Keurig auto fill controller
//

#include <util/delay.h>
#include <avr/io.h>
#include <string.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdio.h>

//
// setup for stdout to bus pirate
//
#define STDOUT_BAUD 9600
#define STDOUT_DDR DDRA
#define STDOUT_PORT PORTA
#define STDOUT_PIN PA5
#include "stdout_software_serial.h"

// water level
#define WATER_LEVEL_MAX 145
#define WATER_LEVEL_FILL 155
#define WATER_LEVEL_LOW 165
#define WATER_LEVEL_MIN 175

// leds
#include "light_ws2812.h"
struct cRGB led[2];

#define LEDS_BLUE 0
#define LEDS_GREEN 1
#define LEDS_YELLOW 2
#define LEDS_RED 3
#define LEDS_OFF 4

int current_color = -1;

// modes
#define TANK_INIT -1
#define TANK_FULL 0
#define TANK_WATER_FILL 1
#define TANK_ERROR 2

int tank_mode = TANK_INIT;

void led_color(int color)
{
    if (color != current_color)
    {
        current_color = color;
        switch (current_color)
        {
            case LEDS_BLUE:
                led[0].r = 0; led[0].g = 0; led[0].b = 32;
                led[1].r = 0; led[1].g = 0; led[1].b = 32;
                break;
            case LEDS_GREEN:
                led[0].r = 0; led[0].g = 128; led[0].b = 0;
                led[1].r = 0; led[1].g = 128; led[1].b = 0;
                break;
            case LEDS_YELLOW:
                led[0].r = 96; led[0].g = 64; led[0].b = 0;
                led[1].r = 96; led[1].g = 64; led[1].b = 0;
                break;
            case LEDS_OFF:
                led[0].r = 0; led[0].g = 0; led[0].b = 0;
                led[1].r = 0; led[1].g = 0; led[1].b = 0;
                break;
            default:
                led[0].r = 255; led[0].g = 0; led[0].b = 0;
                led[1].r = 255; led[1].g = 0; led[1].b = 0;
                break;
        }
        ws2812_setleds(led, 2);
    }
}

// water valve
#define WATER_ON() (PORTB |= _BV(PB1))
#define WATER_OFF() (PORTB &= ~_BV(PB1))

// water valve on time and interrupt

int water_on_time = 0;

ISR(TIM0_COMPA_vect)
{
    if (tank_mode == TANK_WATER_FILL)
    {
        water_on_time++;
        // fill for maximum 2 minutes, then shut off with error
        // 2 * 60 seconds * 30 interrupts per second
        if (water_on_time > 2 * 60 * 30)
        {
            tank_mode = TANK_ERROR;
            WATER_OFF();
            led_color(LEDS_RED);
        }
    }
    else
    {
        water_on_time = 0;
    }
}

int main(void)
{
    // setup watch dog timer for 4 seconds
    wdt_reset();
    wdt_enable(WDTO_4S);

    tank_mode = TANK_INIT;

    // setup ADC for eTape level sensor
    ADCSRA = (1 << ADPS2) | (1 << ADPS1);  // div by 64, 125Khz conversion
    ADMUX = 0; // VCC as ref, PA0 as input
    ADCSRB = (1 << ADLAR); // 8 bit precision
    DIDR0 |= (1 << ADC0D); // disable digital input
    ADCSRA |= (1 << ADEN) | (1 << ADSC) | (1 << ADATE); // enable ADC and start

    // setup water valve output
    DDRB |= (1 << PB1);
    WATER_OFF();

    _delay_ms(100);

    init_stdout();

    // set up timer0 to interrupt every 30 times a second
    TCCR0A |= (1 << WGM01);
    TCCR0B |= (1 << CS02) | (1 << CS00);
    TCNT0 = 0;
    OCR0A = 255;
    TIMSK0 |= (1 << OCIE0A);


    sei();

    led_color(LEDS_BLUE);
    led_color(LEDS_RED);
    led_color(LEDS_OFF);
    _delay_ms(250);

    int level = 0;
    while (1)
    {
        wdt_reset();
        level = ADCH;

        printf("Water level %d\n", level);

        // no error condition
        // we fill the tank when the water drops below the fill line
        // and stop it when it is full.
        if (tank_mode != TANK_ERROR)
        {
            if (level < WATER_LEVEL_MAX && tank_mode != TANK_FULL)
            {
                tank_mode = TANK_FULL;
                WATER_OFF();
                led_color(LEDS_BLUE);
            }

            if (level > WATER_LEVEL_FILL && tank_mode != TANK_WATER_FILL)
            {
                tank_mode = TANK_WATER_FILL;
                WATER_ON();
                led_color(LEDS_GREEN);
            }

            // test for error condition - water is too low
            if (level > WATER_LEVEL_MIN)
            {
                tank_mode = TANK_ERROR;
                WATER_OFF();
                led_color(LEDS_RED);
            }

            // or maybe this is just a warning that it is getting low
            if (level > WATER_LEVEL_LOW && tank_mode == TANK_WATER_FILL)
            {
                led_color(LEDS_YELLOW);
            }
        }

        // wait 100 ms before looping again
        _delay_ms(100);
    }
}

