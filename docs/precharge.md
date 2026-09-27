# Precharge / inrush

The two-relay sequence is read as a precharge / arc-bypass circuit; this is a hypothesis
based on the switching order.

## Why a controller sequences two relays

An electric skateboard's speed controller (ESC) has a large input capacitance. If the
battery is connected to a discharged capacitor through a hard contact, the capacitor
looks almost like a short circuit for an instant: the inrush current is limited only by
the wiring and contact resistance and can be very high. That current pits and welds
relay contacts and stresses the pack.

The usual fix is a **precharge** step: the capacitor is first charged through a resistor
that limits the current, and only once it is nearly charged is the full-current path
closed. Charging through a resistor `R` into a capacitance `C` follows an RC curve with
time constant `τ = R·C`; the current starts at `V/R` and decays. After a few `τ` the
capacitor voltage is close to the pack voltage, so closing the main contact then causes
little inrush. The fixed ~0.5 s (v2) / ~2 s (v1) step delay of the firmware matches
"wait a few time constants for the capacitor to charge".

## How this maps to the two relays

The switch-on order is: close relay 1 → wait → close relay 2 → wait → **open relay 1**.
The end state is "relay 2 closed, relay 1 open". This fits the following wiring:

- **Relay 1** connects the battery to the ESC **through the precharge resistor** while
  the capacitor charges.
- **Relay 2** is the **main / bypass** contact that connects the battery directly once
  the capacitor is charged; relay 1 (and its resistor) is then dropped out of the path.

On switch-off the order is: close relay 1 → wait → open relay 2 → wait → open relay 1.
Bringing relay 1 (the resistor path) back in **before** opening relay 2 means relay 2
does not break the full load current on its own — the resistor path carries the break
current / absorbs the arc energy, and relay 1 opens last with little current flowing.
That is why **relay 2 carries the break current only together with the resistor path**,
which protects its contacts.

## Open parameters

- Precharge resistor value and ESC input capacitance, and with them the real time
  constant. Choose them so that a few `τ` fit into the relay step delay.
- Which relay is wired as the resistor path and which as the bypass. The firmware names
  (relay 1 = main relay `BIT_RELAY_1`, relay 2 = aux relay `BIT_RELAY_2`) do not fix
  this; the mapping above follows from the switching order.
- Pack voltage and the divider ratio feeding ADC0.
