// SPDX-FileCopyrightText: Johannes Stockhammer
// SPDX-License-Identifier: Apache-2.0
/*
 * Skateboard_Power_V1.cpp
 *
 *  Author: Johannes Stockhammer
 *
 * Version:      1.0
 * Hardware:     ATtiny13 (internal RC oscillator 9.6 MHz), switch on the handle,
 *               2 relays, status LED
 * Software:     avr-gcc (avr-g++)
 * Description:  Power controller for an electric skateboard. The switch on the handle is
 *               debounced; on switch-on and switch-off the two relays are switched in a
 *               fixed order (see docs/precharge.md). PB1 shows the switch state.
 */

//Includes
#include <avr/io.h>
#include <avr/interrupt.h>

//Defines
	//Clock and times
#ifndef F_CPU
#define F_CPU 9600000UL								//Internal RC oscillator 9.6 MHz
#endif
#define TICK_MS 25									//Timer0 compare interrupt every 25ms
#define TIMER0_TOP ((F_CPU/1024UL*TICK_MS)/1000UL-1)	//(9,6MHz / 1024 * 25ms) - 1 = 233
#define TIME_50ms 2									//(50ms / 25ms) = 2
#define TIME_2sec 80								//(2000ms / 25ms) = 80
#define TIME_DEBOUNCE TIME_50ms
#define TIME_RELAY TIME_2sec						//Wait time per relay step
	//Logic
#define OFF 0
#define ON 1
	//Switching routines
#define SEQ_START 0									//Close relay 1
#define SEQ_WAIT_R2 1								//Switch relay 2
#define SEQ_WAIT_END 2								//Open relay 1
#define SEQ_IDLE 99									//Routine not active = init value
	//Pins
#define BIT_SWITCH PB0								//Switch on the handle (pull-up, pressed = LOW)
#define BIT_INDICATOR PB1							//Status LED "on"
#define BIT_RELAY_1 PB2								//Relay 1 (main relay)
#define BIT_RELAY_2 PB3								//Relay 2 (aux relay)

//Variables
	//Timers (counted down in the ISR, 8 bit -> access is atomic)
volatile uint8_t switch_timer=0, routine_timer=0;
	//Switch and motor
bool switch_state=OFF, engine_state=OFF, do_once_1=0;
	//Switching routines
uint8_t on_routine_state=SEQ_IDLE, off_routine_state=SEQ_IDLE;

int main(void)
{
	//Inputs/Outputs
		//Switch - input
	DDRB &= ~(1 << BIT_SWITCH);						//PB0 as input for switch
	PORTB |= (1 << BIT_SWITCH);						//Switch on pull-up
		//Status LED - output
	DDRB |= (1 << BIT_INDICATOR);					//PB1 as output for status LED
		//Relays - output
	DDRB |= (1 << BIT_RELAY_1);						//PB2 as output for relay 1 (main relay)
	DDRB |= (1 << BIT_RELAY_2);						//PB3 as output for relay 2 (aux relay)

	//Timer
		//8-bit Timer 0, CTC mode
	TCCR0A=0b00000010;								//CTC mode (WGM01)
	OCR0A=TIMER0_TOP;								//Top -> compare interrupt every 25ms
	TCCR0B=0b00000101;								//Prescaler CLK/1024

	//Interrupt
	TIMSK0=0b00000100;								//Timer 0 compare A interrupt on
	sei();											//Switch on interrupts globally

	while(1)		//Main loop
	{
		//Detect switch state and debounce
			//State changed compared to before -> wait
		if(((PINB&(1 << BIT_SWITCH))<1)&&(switch_state==OFF)&&(!do_once_1))
		{
			switch_timer=TIME_DEBOUNCE;				//Start debounce time
			do_once_1=1;
		}
		if(((PINB&(1 << BIT_SWITCH))>0)&&(switch_state==ON)&&(!do_once_1))
		{
			switch_timer=TIME_DEBOUNCE;				//Start debounce time
			do_once_1=1;
		}
			//Save state after bouncing
		if(!switch_timer)
		{
			if(((PINB&(1 << BIT_SWITCH))<1)&&(switch_state==OFF))		//Timer expired, switch pressed now and not before
			{
				switch_state=ON;
				do_once_1=0;
				PORTB |= (1 << BIT_INDICATOR);		//Status LED on
			}
			else
			{
				if(((PINB&(1 << BIT_SWITCH))>0)&&(switch_state==ON))	//Timer expired, switch released now and pressed before
				{
					switch_state=OFF;
					do_once_1=0;
					PORTB &= ~(1 << BIT_INDICATOR);	//Status LED off
				}
			}
		}//end if !switch_timer

		//Select switching routine motor
		if((switch_state==ON)&&(engine_state==OFF)&&(off_routine_state==SEQ_IDLE)&&(on_routine_state==SEQ_IDLE)) on_routine_state=SEQ_START;
		if((switch_state==OFF)&&(engine_state==ON)&&(off_routine_state==SEQ_IDLE)&&(on_routine_state==SEQ_IDLE)) off_routine_state=SEQ_START;

		//Switch-on routine
		switch(on_routine_state)
		{
			case SEQ_START:		PORTB |= (1 << BIT_RELAY_1);		//Close relay 1
								routine_timer=TIME_RELAY;			//Wait time
								on_routine_state=SEQ_WAIT_R2;		//Jump to the next case
								break;

			case SEQ_WAIT_R2:	if(!routine_timer)					//If wait time is over
								{
									PORTB |= (1 << BIT_RELAY_2);	//Close relay 2
									routine_timer=TIME_RELAY;		//Wait time
									on_routine_state=SEQ_WAIT_END;	//Jump to the next case
								}
								break;

			case SEQ_WAIT_END:	if(!routine_timer)					//If wait time is over
								{
									PORTB &= ~(1 << BIT_RELAY_1);	//Open relay 1
									on_routine_state=SEQ_IDLE;		//End routine = init value
									engine_state=ON;
								}
								break;
		}//end switch on_routine_state

		//Switch-off routine
		switch(off_routine_state)
		{
			case SEQ_START:		PORTB |= (1 << BIT_RELAY_1);		//Close relay 1
								routine_timer=TIME_RELAY;			//Wait time
								off_routine_state=SEQ_WAIT_R2;		//Jump to the next case
								break;

			case SEQ_WAIT_R2:	if(!routine_timer)					//If wait time is over
								{
									PORTB &= ~(1 << BIT_RELAY_2);	//Open relay 2
									routine_timer=TIME_RELAY;		//Wait time
									off_routine_state=SEQ_WAIT_END;	//Jump to the next case
								}
								break;

			case SEQ_WAIT_END:	if(!routine_timer)					//If wait time is over
								{
									PORTB &= ~(1 << BIT_RELAY_1);	//Open relay 1
									off_routine_state=SEQ_IDLE;		//End routine = init value
									engine_state=OFF;
								}
								break;
		}//end switch off_routine_state
	}//end while
}//end main

ISR(TIM0_COMPA_vect)		//every 25ms
{
	if(switch_timer) switch_timer--;
	if(routine_timer) routine_timer--;
}
