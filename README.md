# Broan ERV serial component

An ESPHome component to control Broan, Nutone, Venmar and VanEE ERVs / HRVs through their RS-485 wall controller bus, from Home Assistant.

This is a fork of [nspitko/broan_erv_uart](https://github.com/nspitko/broan_erv_uart). The original protocol work is documented [here](https://spitko.net/2025/08/08/Reverse-Engineering-an-ERV/). This fork adds everything the original wall controller can do from its user and installer menus, reverse engineered by sniffing a **VanEE V180H75RT** (HRV with recirculation) and its wall controller:

* fan mode and fan speed as two separate controls, including **recirculation** and **intermittent** speeds
* installer menu: **flow setpoints** (minimum / medium / high, supply / exhaust) bounded by the ERV's own limits, and **defrost mode**
* a **listen only** mode to capture what the wall controller reads and writes, and map new registers
* an energy sensor example for the Home Assistant Energy dashboard

<table>
  <tr>
    <td>
      <img width="332" height="477" alt="image" src="https://github.com/user-attachments/assets/5170773c-7def-4df2-bef0-9fa33ca78082" />
    </td>
    <td valign="top">
      <img width="329" height="367" alt="image" src="https://github.com/user-attachments/assets/2e4aba9a-a7a8-4349-9e79-a22f0018b23a" />
    </td>
  </tr>
</table>

## Supported models
All Broan, Nutone, Venmar and VanEE ERVs / HRVs with an RS-485 wall controller bus probably work. The easiest check is whether your unit supports the VTTOUCHW (all brands have a version of this interface).

Tested in this fork: **VanEE V180H75RT** (HRV, recirculation). HRVs tend to support fewer features than ERVs: remove from your YAML the sensors your unit never fills (eg `temperature_out` on the V180H75RT).

## Features

### Fan mode and speed
Two selects, like the wall controller:

| Fan mode | Uses the fan speed | Registers written |
|---|---|---|
| Off | – | `00:20 = 0x01` |
| Air Exchange | yes | `00:20` = `0x09` Minimum, `0x0B` Medium, `0x0A` High |
| Recirculate | yes | `00:20` = `0x05` Minimum, `0x07` Medium, `0x06` High |
| Intermittent | yes | `03:22 = 0x00`, `0E:22` = speed, `00:20 = 0x08` |
| Intermittent + Recirculate | yes | `03:22 = 0x01`, `0E:22` = speed, `00:20 = 0x08` |
| Turbo / Humidity / Smart | – | `00:20` = `0x0C` / `0x0D` / `0x11` |
| Override | – | read only: shown when an auxiliary (dry contact) remote forces the ERV |

* Intermittent speed `0E:22`: `0x00` Minimum, `0x02` Medium, `0x01` High, applied to both the exchange and the recirculation phases.
* Like the wall controller, every `00:20` write is followed by `08:20 = 0x00` in the same frame. Without it, Air Exchange Medium runs slower than Minimum.
* Changing the speed only rewrites what is needed: `00:20` for Air Exchange and Recirculate, `0E:22` alone for both intermittent modes. In other modes the speed is remembered and applied when switching back. The last speed survives reboots.
* Mode and speed are rebuilt from the ERV (`00:20`, `03:22`, `0E:22`), so changes made elsewhere show up in Home Assistant. New values are shown as soon as they are sent; the read back corrects them if the ERV refuses.

### Intermittent period
Minutes ON per hour, 10 to 55 in steps of 5. Stored by the ERV in seconds (`02:22`, eg 600 = 10 min).

### Installer menu
All in the Configuration section of the device in Home Assistant. Nothing here is ever written automatically, only when changed from Home Assistant.

* **Flow setpoints** (CFM) for Minimum (`0A:50` / `0B:50`), Medium (`06:22` / `08:22`) and High (`0E:50` / `0F:50`), supply / exhaust. Like the wall controller:
  * both sides of a speed are written together, supply first;
  * minimum ≤ medium ≤ high is enforced on each side;
  * values outside the ERV's limits are refused: lowest `0C:50` (65 CFM on the V180H75RT), highest `11:50` (152.52 CFM, measured by auto balancing). Until those are read, the entity range applies (default 65 to 193).
* **Defrost mode** (`12:50`): Discretion (`0x02`, factory setting, defrost without fan speed change) or Plus (`0x01`, extended defrost for colder regions).
* **Highest reachable flows** (`16:10` supply, `17:10` exhaust), measured by auto balancing, as diagnostic sensors.

### Other
* Humidity control mode (see below)
* Intake temperature, fan CFM and RPM, power draw
* Filter life left and filter reset
* Fault and warning codes, active mode

## Requirements
1. An ESPHome device that talks RS-485: either a board with a built-in transceiver (eg Waveshare ESP32-S3-RS485-CAN, Waveshare ESP32-S3-Relay-6CH) or an external transceiver on a UART.
2. **Only one controller on the bus.** The ERV answers a single main controller, so the original wall controller must be disconnected (at least D+ and D-) while the ESP is in control. The only exception is `listen_only` (see below). Auxiliary dry contact remotes keep working (mode shows Override).
3. Ideally power the ESP from the ERV's 12V output. If the ERV completes the handshake with the ESP and the ESP later goes away, the ERV eventually goes into an error state.

## Installation
Near the control interface on the ERV, look for the green terminal block with D+, D-, GND and 12V (typically 6 terminals with LED and OVR). Connect D+, D- and GND to your transceiver, and 12V if your board accepts it (check its datasheet, many ESP32 boards don't). Never power the board from 12V and USB at the same time unless its datasheet says it's safe.

* **Wire labels.** Many transceivers label the lines A and B, and conventions differ: often A = D- and B = D+, but the Waveshare ESP32-S3-RS485-CAN labels them A+ / B- and works with D+ → A+, D- → B-. If you see alignment or checksum errors, swap the two data wires (it doesn't damage anything).
* **Termination.** Leave the 120 Ω terminator off first; enable it only if you get communication errors.
* **Direction pin.** Transceivers without automatic direction control (eg SP3485 on the Waveshare ESP32-S3-RS485-CAN, EN = GPIO21) need a direction pin. Declare it **in the `broan:` block** (`flow_control_pin`), not under `uart:`: the component raises it only while sending each frame. Reading works without it, which makes a missing or misplaced pin look like "reads work, writes don't".

## Capturing registers from a wall controller (listen only)
To map a feature this component doesn't support yet, wire the ESP in parallel with the original wall controller (same D+, D- and GND terminals, termination off) and set:
```yaml
broan:
  uart_id: rs485
  flow_control_pin: GPIO21
  listen_only: true
```
In this mode the ESP never transmits:
* every register the wall controller writes is logged as `Sniffed write XXYY (known|UNKNOWN): ...`;
* the ERV's answers to the wall controller's reads are logged as `Sniffed read XXYY (known|UNKNOWN): ...`, the first time a register is seen and whenever its value changes (the wall controller polls in a loop);
* the Home Assistant entities follow what the wall controller sets, but changing them sends nothing (a warning is logged).

Change a setting on the wall controller and look for the matching lines. Power cycling the ERV while listening shows what the wall controller reads at start up. Remove `listen_only` (and disconnect the wall controller) to control the ERV from the ESP again.

## Register notes (VanEE V180H75RT)
Found with `listen_only`. Registers are written `group:field` like the logs (`XXYY` = field `XX`, group `YY`).

| Register | Type | Meaning |
|---|---|---|
| `00:20` | byte | Fan mode (see table above) |
| `01:20` | byte | Read back as `0x08` right after `00:20 = 0x08`: probably the mode actually applied. Not used |
| `07:20` | int | Active mode: 1 Running, 2 Max, 4 Manual, 6 / 7 / 8 Recirculate Minimum / High / Medium |
| `08:20` | byte | Written `0x00` by the wall controller after every `00:20` write |
| `02:22` | int | Intermittent ON time, seconds per hour |
| `03:22` | byte | Intermittent: recirculate during the OFF period (`0x00` / `0x01`) |
| `0E:22` | byte | Intermittent speed: `0x00` min, `0x02` med, `0x01` max |
| `06:22` / `08:22` | float | Medium flow setpoint, supply / exhaust |
| `0A:50` / `0B:50` | float | Minimum flow setpoint, supply / exhaust |
| `0E:50` / `0F:50` | float | High flow setpoint, supply / exhaust |
| `0C:50` | float | Lowest flow setpoint allowed (65) |
| `11:50` | float | Highest flow setpoint allowed (152.52) |
| `16:10` / `17:10` | float | Highest reachable flow, supply / exhaust (auto balancing) |
| `12:50` | byte | Defrost mode: `0x01` Plus, `0x02` Discretion |
| `0D:50`, `10:50`, `17:50`, `18:50`, `04:22`, `16:50` | | Read by the wall controller in the installer menu, role unknown (values in `broan.h`) |

## Humidity Control Mode
In Humidity Control Mode the ERV runs when the humidity is above the target. The ERV has no humidity sensor: the controller sends the current humidity periodically. Since the ESP replaces the controller, send it from ESPHome with `setCurrentHumidity()`, eg from a Home Assistant sensor. See [humidity_control_example.yaml](./examples/humidity_control_example.yaml).

To use it, set the target with "Humidity Setpoint", then turn on the "Humidity Control" switch.

## ESPHome YAML
```yaml
external_components:
  - source:
      type: git
      url: https://github.com/gabidigab/broan_erv_uart
      ref: main
    components: [ broan ]

uart:
  id: rs485
  tx_pin: GPIO17 # Change these to match your RS-485 transceiver
  rx_pin: GPIO18
  baud_rate: 38400
  rx_buffer_size: 2048

broan:
  id: erv
  uart_id: rs485
  # Direction pin of transceivers without automatic direction control
  # (eg GPIO21 on the Waveshare ESP32-S3-RS485-CAN). Here, not under uart:.
  flow_control_pin: GPIO21
  # listen_only: true   # Capture mode, wall controller connected in parallel

select:
  - platform: broan
    # Off, Air Exchange, Intermittent, Intermittent + Recirculate, Turbo, Humidity,
    # Recirculate, Smart, Override (read only)
    fan_mode:
      name: "Fan mode"
    # Minimum, Medium, High. Used by Air Exchange, Recirculate,
    # Intermittent + Recirculate and Intermittent
    fan_speed:
      name: "Fan speed"
    # Installer menu: Discretion (factory setting) or Plus (colder regions)
    defrost_mode:
      name: "Defrost mode"

number:
  - platform: broan
    # Minutes ON per hour in intermittent mode, 10 to 55 in steps of 5
    intermittent_period:
      name: "Intermittent period"

    # Installer flow setpoints, in CFM (entry box, step 1). Values outside the ERV's
    # limits (0C:50 / 11:50) are refused, like the wall controller. Default range 65 to 193
    # (VanEE V180H75RT datasheet), change it with min_value / max_value.
    minimum_supply_flow:
      name: "Minimum supply flow"
    minimum_exhaust_flow:
      name: "Minimum exhaust flow"
    medium_supply_flow:
      name: "Medium supply flow"
    medium_exhaust_flow:
      name: "Medium exhaust flow"
    high_supply_flow:
      name: "High supply flow"
    high_exhaust_flow:
      name: "High exhaust flow"

sensor:
  - platform: broan
    # As reported by the ERV, in watts
    power:
      name: "Power draw"
      id: erv_power
      state_class: measurement
    # Intake air temperature. This will generally read high
    temperature:
      name: "Temperature"
    # In days
    filter_life:
      name: "Remaining filter life"
    # Flows and fan speeds as reported by the ERV
    supply_fan_cfm:
      name: "Supply fan CFM"
    exhaust_fan_cfm:
      name: "Exhaust fan CFM"
    supply_fan_rpm:
      name: "Supply fan RPM"
    exhaust_fan_rpm:
      name: "Exhaust fan RPM"
    # Highest flows reachable, measured by auto balancing (diagnostic)
    max_supply_fan_cfm:
      name: "Max reachable supply flow"
    max_exhaust_fan_cfm:
      name: "Max reachable exhaust flow"
    # Exhaust air temperature. Not available on all units (not on the V180H75RT)
    # temperature_out:
    #   name: "Temperature out"

  # Energy (kWh) for the Home Assistant Energy dashboard
  - platform: integration
    name: "ERV energy"
    sensor: erv_power
    time_unit: h
    integration_method: left
    restore: true
    unit_of_measurement: kWh
    device_class: energy
    state_class: total_increasing
    accuracy_decimals: 3
    filters:
      - multiply: 0.001

text_sensor:
  - platform: broan
    # If there is a major fault, it will be indicated here. Else "OK"
    fault_code:
      name: "Fault code"
    # Same as above, but for warnings
    warning_code:
      name: "Warning code"
    # What the fans are currently doing
    active_mode:
      name: "Active mode"
    # Potentially useful for development
    # model: { name: "Model" }
    # firmware: { name: "Firmware" }
    # firmware_version: { name: "Firmware version" }
    # hardware_revision: { name: "Hardware rev" }

button:
  - platform: broan
    # Resets filter life to 7884000 seconds / 3 months
    filter_reset:
      name: "Filter reset"
```

## Changes from the original component
If you come from nspitko/broan_erv_uart, update your Home Assistant automations:
* **Fan mode options** are now `Off`, `Air Exchange`, `Intermittent`, `Intermittent + Recirculate`, `Turbo`, `Humidity`, `Recirculate`, `Smart`, `Override`. The speed moved to the new `fan_speed` select: `min` / `manual` / `max` become `Air Exchange` + `Minimum` / `Medium` / `High`, `int` becomes `Intermittent`, `ovr` becomes `Override`.
* The **`fan_speed` number** (percentage between min and max CFM) is replaced by the **`fan_speed` select**. Flow setpoints are now set directly, in CFM, with the installer flow numbers.
* **`intermittent_period`** is now in minutes per hour (was seconds).

## FAQ
**Q: I see errors about failed communication**

* Timeouts, or reads work but writes don't: usually the direction pin. Declare `flow_control_pin` in the `broan:` block (see Installation).
* Data arrives but with alignment or checksum errors: swap D+ / D-, check the ground wire, try toggling the termination.

**Q: A setting changed in Home Assistant snaps back**

* In `listen_only` the ESP never sends anything, so entities only display what the wall controller sets.
* Flow setpoints outside the ERV's limits, or breaking minimum ≤ medium ≤ high, are refused. The log says why.

**Q: Why doesn't it support X?**

Not everything is exposed over RS-485, and some features differ between models. Capture it with `listen_only` and open an issue or PR with the `Sniffed write` / `Sniffed read` lines.

**Q: What if I still want wall controls?**

Use the auxiliary remotes: they use the dry contact interface, a hard override (fan mode shows `Override`). Or control the ERV from Home Assistant, eg with a wall tablet or a Sonoff NSPanel.
