# Block diagram

The controller part follows from the firmware. The power path (precharge resistor, how
the relays bypass it, the ESC/motor) follows the precharge hypothesis in
[`precharge.md`](precharge.md). No component values are given.

```mermaid
flowchart LR
    KEY[Key switch<br/>PB0, pull-up] --> MCU

    subgraph CTRL[Controller]
        MCU[ATtiny13 v1 / ATtiny26 v2]
        MCU -- PB2 --> R1[Relay 1<br/>main relay]
        MCU -- PB3 --> R2[Relay 2<br/>aux relay]
    end

    BATT[Battery pack] --> R1
    BATT --> DIV[Voltage divider]:::open
    DIV -- PA0/ADC0 --> MCU

    R1 --> NODE((Power node))
    R2 --> NODE
    RPRE[Precharge resistor R_pre]:::open --- R2
    NODE --> ESC[ESC / motor]:::open

    MCU -- PB1 --> PIEZO[Piezo v2]
    MCU -- PB4/PB5/PB6/PA7 --> LEDS[4-LED battery gauge v2]
    BTN["Show-battery button<br/>PA6, pull-up (v2)"] --> MCU

    classDef open stroke-dasharray: 5 5,stroke:#999;
```

Dashed boxes (precharge resistor, divider ratio, ESC/motor) are not defined by the
firmware. How relay 2 and the precharge resistor sit relative to relay 1 depends on the
wiring — see [`precharge.md`](precharge.md).

- **v1** uses only the key switch, the two relays and a status LED on PB1.
- **v2** adds the ADC0 divider, the 4-LED gauge, the piezo and the show-battery button.
