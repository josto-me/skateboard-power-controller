# Pinout

Directions and functions are those configured by the firmware.

## v2 (ATtiny26)

| Pin | Signal | Direction | Function |
|---|---|---|---|
| PB0 | Key switch | Input, pull-up | Handle switch; pressed = LOW |
| PB1 | Piezo | Output | Low-voltage warning buzzer |
| PB2 | Relay 1 | Output | Main relay (`BIT_RELAY_1`) |
| PB3 | Relay 2 | Output | Aux relay (`BIT_RELAY_2`) |
| PB4 | LED green | Output | Battery gauge, highest level |
| PB5 | LED yellow 1 | Output | Battery gauge |
| PB6 | LED yellow 2 | Output | Battery gauge |
| PA7 | LED red | Output | Battery gauge, lowest level / blinks on alarm |
| PA6 | "Show battery" button | Input, pull-up | Shows the gauge while idle; pressed = LOW |
| PA0 / ADC0 | Battery divider | Analog input | Battery voltage via a resistor divider |

The gauge range maps ADC full scale (≈5 V at the divider output) to the four LED levels;
the 1–4 V thresholds are on the *divided* measurement voltage, not the real pack voltage.
Choose the divider ratio for your pack.

## v1 (ATtiny13)

| Pin | Signal | Direction | Function |
|---|---|---|---|
| PB0 | Key switch | Input, pull-up | Handle switch; pressed = LOW |
| PB1 | Status LED | Output | Follows the switch state ("on" indicator) |
| PB2 | Relay 1 | Output | Main relay (`BIT_RELAY_1`) |
| PB3 | Relay 2 | Output | Aux relay (`BIT_RELAY_2`) |

v1 has no ADC, LED bar or piezo. On the ATtiny13 the same PB1 output is used as a plain
status LED.
