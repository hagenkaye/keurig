//
// stdout implementation using software serial port on ATtiny
// this file should be #include'd in your main start up.
// before including this file, include the following
//
// #include <avr/io.h>
// #include <string.h>
// #include <avr/interrupt.h>
// #include <stdio.h>
//
// #define STDOUT_BAUD 9600
//
// this software serial port version uses the 16 bit timer 1
// and one output pin.  the output pin must be defined as well
//
// #define STDOUT_DDR DDRA
// #define STDOUT_PORT PORTA
// #define STDOUT_PIN PA5
//

static volatile char txState = 0;
static volatile char txChar = 0;
static int software_serial_putchar(char c, FILE *stream);
static FILE mystdout = FDEV_SETUP_STREAM(software_serial_putchar, NULL, _FDEV_SETUP_WRITE);

void init_stdout(void)
{
    cli();
    STDOUT_DDR |= (1 << STDOUT_PIN);
    STDOUT_PORT |= (1 << STDOUT_PIN);

    // set up timer for baud bate
    TCCR1B |= (1 << CS10);
    OCR1A = F_CPU / STDOUT_BAUD;
    TCCR1B |= (1 << WGM12);
    TIMSK1 |= (1 << OCIE1A);

    stdout = &mystdout;
}

int software_serial_putchar(char value, FILE *stream)
{
    if (value == '\n')
    {
        software_serial_putchar('\r', stream);
    }
    while (txState != 0);

    txChar = value;
    txState = 1;

    return 0;
}

ISR(TIM1_COMPA_vect)
{
    switch (txState)
    {
        case 0:
            // idle - set wait for txByte
            break;
        case 1:
            // start bit - set pin low
            STDOUT_PORT &= ~(1 << STDOUT_PIN);
            txState++;
            break;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
            // send out data bits
            if (txChar & 1)
            {
                STDOUT_PORT |= (1 << STDOUT_PIN);
            }
            else
            {
                STDOUT_PORT &= ~(1 << STDOUT_PIN);
            }
            txChar = txChar >> 1;
            txState++;
            break;
        case 10:
            // send stop bit - set pin high
            STDOUT_PORT |= (1 << STDOUT_PIN);
            txState++;
            break;
        case 11:
            // reset state machine
            // allow another byte to be sent
            txState = 0;
            break;
    }
}
