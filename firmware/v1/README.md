# Firmware v1 (ATtiny13)

Basic relay sequencer, no battery gauge and no alarm. A switch on the handle is
debounced; on switch-on and switch-off the two relays are driven in a fixed order
(see [`../../docs/precharge.md`](../../docs/precharge.md)).
PB1 drives a status LED that follows the switch state.

- Timer0 in **CTC mode**, prescaler **1024**, `TIM0_COMPA` interrupt, 25 ms tick
  (`OCR0A = 233` at 9.6 MHz, derived from `F_CPU`).
- Relay step wait time ~2 s (`TIME_RELAY`).
- No ADC, LED bar or piezo (these are v2 features).

## Build

- MCU: ATtiny13
- Clock: internal RC, 9.6 MHz. The 25 ms tick is derived from `F_CPU`
  (`OCR0A = 233`, prescaler 1024). Set the fuses to match.
- Toolchain: avr-gcc and avr-libc.

```
avr-g++ -mmcu=attiny13 -DF_CPU=9600000UL -Os -o v1.elf Skateboard_Power_V1.cpp
```
