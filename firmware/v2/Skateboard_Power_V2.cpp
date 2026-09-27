// SPDX-FileCopyrightText: Johannes Stockhammer
// SPDX-License-Identifier: Apache-2.0
/*
 * Skateboard_Power_V2.cpp
 *
 *  Author: Johannes Stockhammer
 *
 * Version:      2.0
 * Hardware:     ATtiny26 (internal RC oscillator 8 MHz), switch on the handle, 2 relays,
 *               4 LEDs battery display, button "show battery", piezo, voltage divider on ADC0
 * Software:     avr-gcc (avr-g++)
 * Description:  Power controller for an electric skateboard. The switch on the handle is
 *               debounced; on switch-on and switch-off the two relays are switched in a
 *               fixed order (see docs/precharge.md). The battery voltage is measured every
 *               500ms and shown on 4 LEDs. On undervoltage the red LED blinks and the piezo
 *               sounds; the relays are not switched (no power cut while riding).
 */

//Includes
#include <avr/io.h>
#include <avr/interrupt.h>

//Defines
	//Times
#define TIME_16ms 2									//(16ms / 8,192ms) = 2
#define TIME_500ms 61								//(500ms / 8,192ms) = 61
#define TIME_DEBOUNCE TIME_16ms
#define TIME_RELAY TIME_500ms						//Wait time per relay step
#define TIME_ADC TIME_500ms							//Measuring interval battery voltage
	//Logic
#define OFF 0
#define ON 1
#define MINUS 0										//Piezo count direction down
#define PLUS 1										//Piezo count direction up
	//Switching routines
#define SEQ_START 0									//Close relay 1
#define SEQ_WAIT_R2 1								//Switch relay 2
#define SEQ_WAIT_END 2								//Open relay 1
#define SEQ_IDLE 99									//Routine not active = init value
	//Battery voltage (divided measuring voltage, ADCH 255 = 5V -> 50000)
#define ADC_FACTOR 196								//(255 * 196) = 49980 ~ 5V
#define VOLTAGE_1V 10000
#define VOLTAGE_2V 20000
#define VOLTAGE_3V 30000
#define VOLTAGE_4V 40000
#define VOLTAGE_HYST 2000							//0,2V hysteresis against LED flicker at the thresholds
	//Battery level
#define LEVEL_ALARM 0								//<1V: red LED blinks, piezo
#define LEVEL_1 1									//1..2V: red
#define LEVEL_2 2									//2..3V: red, yellow 1
#define LEVEL_3 3									//3..4V: red, yellow 1 and 2
#define LEVEL_4 4									//>4V: all 4 LEDs
#define PIEZO_MAX 255								//Count range piezo tone
	//Pins
#define BIT_SWITCH PB0								//Switch on the handle (pull-up, pressed = LOW)
#define BIT_PIEZO PB1								//Piezo undervoltage warning
#define BIT_RELAY_1 PB2								//Relay 1 (main relay)
#define BIT_RELAY_2 PB3								//Relay 2 (aux relay)
#define BIT_LED_GREEN PB4							//LED 1 green
#define BIT_LED_YELLOW_1 PB5						//LED 2 yellow
#define BIT_LED_YELLOW_2 PB6						//LED 3 yellow
#define BIT_LED_RED PA7								//LED 4 red (Port A)
#define BIT_BUTTON_SHOW PA6							//Button "show battery" (Port A, pull-up, pressed = LOW)
#define BIT_ADC_BATTERY PA0							//ADC0 voltage divider battery

//Variables
	//Timers (counted down in the ISR, 8 bit -> access is atomic)
volatile uint8_t switch_timer=0, routine_timer=0, adc_timer=0;
	//Switch and motor
bool switch_state=OFF, engine_state=OFF, do_once_1=0, show_battery=OFF;
	//Switching routines
uint8_t on_routine_state=SEQ_IDLE, off_routine_state=SEQ_IDLE;
	//Battery
uint16_t battery_voltage=0;
uint8_t battery_level=LEVEL_4;
	//Piezo
uint8_t num_piezo_beep=0;
bool piezo_count_direction=PLUS;

int main(void)
{
	//Inputs/Outputs
		//Switch for relays - input
	DDRB &= ~(1 << BIT_SWITCH);						//PB0 as input for switch
	PORTB |= (1 << BIT_SWITCH);						//Switch on pull-up
		//Button for battery display - input
	DDRA &= ~(1 << BIT_BUTTON_SHOW);				//PA6 as input for button
	PORTA |= (1 << BIT_BUTTON_SHOW);				//Switch on pull-up
		//Piezo speaker
	DDRB |= (1 << BIT_PIEZO);						//PB1 as output for undervoltage warning
		//Relays - output
	DDRB |= (1 << BIT_RELAY_1);						//PB2 as output for relay 1 (main relay)
	DDRB |= (1 << BIT_RELAY_2);						//PB3 as output for relay 2 (aux relay)
		//Battery display LEDs - output
	DDRB |= (1 << BIT_LED_GREEN);					//PB4 as output for LED 1 green
	DDRB |= (1 << BIT_LED_YELLOW_1);				//PB5 as output for LED 2 yellow
	DDRB |= (1 << BIT_LED_YELLOW_2);				//PB6 as output for LED 3 yellow
	DDRA |= (1 << BIT_LED_RED);						//PA7 as output for LED 4 red

	//Timer
		//8-bit Timer 0
	TCCR0=0b00000100;								//Prescaler CLK/256 -> overflow every 8,192ms

	//ADC
	DDRA &= ~(1 << BIT_ADC_BATTERY);				//ADC0 (PA0) as input
	ADMUX=0b00100000;								//MUX to ADC0, left adjusted, AREF = AVCC
	ADCSR=0b10000110;								//ADC enable, prescaler 64
	ADCSR |= (1 << ADSC);							//First conversion
	while(ADCSR&(1 << ADSC));						//Wait until done -> ADCH valid, no false alarm at power up

	//Interrupt
	TIMSK=0b00000010;								//Timer 0 overflow interrupt on
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
			}
			else
			{
				if(((PINB&(1 << BIT_SWITCH))>0)&&(switch_state==ON))	//Timer expired, switch released now and pressed before
				{
					switch_state=OFF;
					do_once_1=0;
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

		//Voltage monitoring
		if(!adc_timer)
		{
			battery_voltage=ADC_FACTOR*ADCH;		//ADCH 255 = 5V -> 49980 in battery_voltage

			//Battery level with hysteresis (change only when the threshold is crossed by VOLTAGE_HYST)
			switch(battery_level)
			{
				case LEVEL_4:		if(battery_voltage<VOLTAGE_4V-VOLTAGE_HYST) battery_level=LEVEL_3;
									break;

				case LEVEL_3:		if(battery_voltage>VOLTAGE_4V+VOLTAGE_HYST) battery_level=LEVEL_4;
									if(battery_voltage<VOLTAGE_3V-VOLTAGE_HYST) battery_level=LEVEL_2;
									break;

				case LEVEL_2:		if(battery_voltage>VOLTAGE_3V+VOLTAGE_HYST) battery_level=LEVEL_3;
									if(battery_voltage<VOLTAGE_2V-VOLTAGE_HYST) battery_level=LEVEL_1;
									break;

				case LEVEL_1:		if(battery_voltage>VOLTAGE_2V+VOLTAGE_HYST) battery_level=LEVEL_2;
									if(battery_voltage<VOLTAGE_1V-VOLTAGE_HYST) battery_level=LEVEL_ALARM;
									break;

				case LEVEL_ALARM:	if(battery_voltage>VOLTAGE_1V+VOLTAGE_HYST) battery_level=LEVEL_1;
									break;
			}//end switch battery_level

			//Show battery while riding, or while standing as long as the button is pressed
			show_battery=((switch_state==ON)||(((PINA&(1 << BIT_BUTTON_SHOW))<1)&&(switch_state==OFF)));

			//Switch LEDs and piezo
			if(battery_level==LEVEL_ALARM)			//Undervoltage: warning only, relays are not switched (no power cut while riding)
			{
				PORTB &= ~((1 << BIT_LED_GREEN)|(1 << BIT_LED_YELLOW_1)|(1 << BIT_LED_YELLOW_2));
				PORTA ^= (1 << BIT_LED_RED);		//Invert PA7 -> red LED blinks
				switch(piezo_count_direction)
				{
					case PLUS:	num_piezo_beep++;
								PORTB ^= (1 << BIT_PIEZO);								//Invert PB1 -> piezo tone
								if(num_piezo_beep>=PIEZO_MAX) piezo_count_direction=MINUS;	//Change count direction
								break;

					case MINUS:	num_piezo_beep--;
								PORTB &= ~(1 << BIT_PIEZO);								//Piezo off
								if(!num_piezo_beep) piezo_count_direction=PLUS;			//Change count direction
								break;
				}//end switch piezo_count_direction
			}
			else
			{
				num_piezo_beep=0;					//Counter to 0
				PORTB &= ~(1 << BIT_PIEZO);			//Piezo off
				if(show_battery)
				{
					PORTA |= (1 << BIT_LED_RED);	//Red LED from level 1 upwards
					switch(battery_level)
					{
						case LEVEL_4:	PORTB |= (1 << BIT_LED_GREEN)|(1 << BIT_LED_YELLOW_1)|(1 << BIT_LED_YELLOW_2);
										break;

						case LEVEL_3:	PORTB &= ~(1 << BIT_LED_GREEN);
										PORTB |= (1 << BIT_LED_YELLOW_1)|(1 << BIT_LED_YELLOW_2);
										break;

						case LEVEL_2:	PORTB &= ~((1 << BIT_LED_GREEN)|(1 << BIT_LED_YELLOW_2));
										PORTB |= (1 << BIT_LED_YELLOW_1);
										break;

						case LEVEL_1:	PORTB &= ~((1 << BIT_LED_GREEN)|(1 << BIT_LED_YELLOW_1)|(1 << BIT_LED_YELLOW_2));
										break;
					}//end switch battery_level
				}
				else								//Standing without button -> all LEDs off
				{
					PORTB &= ~((1 << BIT_LED_GREEN)|(1 << BIT_LED_YELLOW_1)|(1 << BIT_LED_YELLOW_2));
					PORTA &= ~(1 << BIT_LED_RED);
				}
			}

			ADCSR |= (1 << ADSC);					//Start a new conversion
			adc_timer=TIME_ADC;						//Start timer
		}//end if !adc_timer
	}//end while
}//end main

ISR(TIMER0_OVF0_vect)		//every 8,192ms
{
	if(switch_timer) switch_timer--;
	if(routine_timer) routine_timer--;
	if(adc_timer) adc_timer--;
}
