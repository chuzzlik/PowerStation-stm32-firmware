# Powerbank BLE API v2

## GATT

- Device name: `Powerbank`
- Service UUID: `6f2a0001-5a3d-4e2c-9a73-1b21d9b00001`
- Status characteristic: `6f2a0002-5a3d-4e2c-9a73-1b21d9b00001`
  - properties: Read, Notify
- Command characteristic: `6f2a0003-5a3d-4e2c-9a73-1b21d9b00001`
  - properties: Write, Write Without Response

Commands are UTF-8 text. Responses and periodic notifications are JSON through the status characteristic.

## Status

Send:

```text
status
```

or:

```text
get status
```

Example response:

```json
{
  "type": "status",
  "apiVersion": 2,
  "systemState": "ON",
  "powerState": "DISCHARGE",
  "socPercent": 74.28,
  "voltageV": 12.841,
  "currentA": -4.102,
  "powerW": -52.672,
  "averagedPowerW": -49.834,
  "currentStoredWh": 334.260,
  "learnedCapacityWh": 450.000,
  "nominalCapacityWh": 450.000,
  "estimatedTimeHours": 6.707,
  "learningActive": false,
  "learningDischargeWh": 0.000,
  "learnedCycles": 2,
  "mosfetEnabled": true,
  "bluetoothEnabled": true,
  "bluetoothConnected": true,
  "settings": {
    "lowCutVoltageV": 9.800,
    "fullVoltageV": 14.200,
    "fullCurrentA": 0.150,
    "chargeEfficiency": 0.950,
    "lowSocPercent": 15.00,
    "criticalSocPercent": 3.00,
    "learningEndVoltageV": 10.400,
    "learningMinDischargeWh": 50.00,
    "learningCorrectionAlpha": 0.250,
    "powerLimitW": 120.00,
    "etaPowerAlpha": 0.080,
    "etaMinPowerW": 3.00,
    "etaMaxHours": 168.00,
    "smallScreenTimeoutSec": 30,
    "mainScreenTimeoutSec": 30
  }
}
```

`powerW` is instantaneous power. `averagedPowerW` is filtered power used for ETA. `estimatedTimeHours = -1` means ETA is unavailable or the station is idle.

## Change a setting

Format:

```text
set key=value
```

Example:

```text
set powerLimitW=140
```

Successful result:

```json
{"type":"result","command":"set powerLimitW=140","ok":true}
```

Error result:

```json
{"type":"result","command":"set unknown=1","ok":false,"error":"unknown_setting"}
```

A successful `set` is immediately saved to NVS.

## Settings recommended for the mobile application

### Basic battery settings

| Key | Meaning | Accepted range |
|---|---|---:|
| `nominalCapacityWh` | Rated battery capacity | 1–5000 Wh |
| `learnedCapacityWh` | Learned/actual capacity | limited by firmware min/max capacity |
| `remainingPercent` | Manually set current charge | 0–100% |
| `currentStoredWh` | Manually set stored energy | 0–learned capacity |
| `lowCutVoltageV` | Emergency low-voltage shutdown | 1–60 V |
| `fullVoltageV` | Full-charge detection voltage | 1–60 V |
| `fullCurrentA` | Full-charge current threshold | 0.01–20 A |
| `chargeEfficiency` | Charging efficiency coefficient | 0.50–1.00 |
| `powerLimitW` | Overload shutdown threshold | 5–2000 W |

For the normal app UI, show `nominalCapacityWh`, `remainingPercent`, `lowCutVoltageV`, `fullVoltageV`, `fullCurrentA`, `chargeEfficiency`, and `powerLimitW`. Put `learnedCapacityWh` and `currentStoredWh` in an advanced/service section because changing them directly affects SoC calculations.

### Protection and indication

| Key | Meaning | Accepted range |
|---|---|---:|
| `lowSocPercent` | Low-battery LED threshold | 0–100% |
| `criticalSocPercent` | Shutdown threshold by SoC | 0–100% |
| `smallScreenTimeoutSec` | Narrow OLED idle timeout | 5–3600 s |
| `mainScreenTimeoutSec` | Main OLED timeout | 5–3600 s |

The app should validate that `criticalSocPercent <= lowSocPercent` before sending values.

### Capacity learning

| Key | Meaning | Accepted range |
|---|---|---:|
| `learningEndVoltageV` | Voltage ending a learning discharge | 1–60 V |
| `learningMinDischargeWh` | Minimum energy for a valid learning cycle | 1–5000 Wh |
| `learningCorrectionAlpha` | Capacity correction strength | 0.01–1.00 |

These belong in an advanced section.

### ETA smoothing

| Key | Meaning | Accepted range |
|---|---|---:|
| `etaPowerAlpha` | EMA response coefficient | 0.01–1.00 |
| `etaMinPowerW` | Minimum power for ETA calculation | 2–100 W |
| `etaMaxHours` | Maximum displayed ETA | 1–1000 h |

Recommended default: `etaPowerAlpha=0.08`. Lower values produce steadier but slower ETA. Higher values react faster but jump more.

## Service commands

```text
save
```

Forces immediate NVS save.

```text
markFull
```

Sets stored energy to learned capacity and starts a capacity-learning cycle.

```text
resetLearning
```

Clears current learning state and the last correction data.

## Error codes

- `empty_command`
- `unknown_command`
- `expected_key_equals_value`
- `empty_value`
- `invalid_number`
- `unknown_setting`

## Mobile-app behavior

1. Subscribe to status notifications before sending commands.
2. Send `status` after connecting.
3. Use `type` to distinguish periodic `status` messages from command `result` messages.
4. After a successful setting change, either update the local model optimistically or request `status` again.
5. Treat BLE loss as normal when the user disables Bluetooth from the station button.

## Стабилизация ETA — firmware 2.2

Новые параметры в объекте `settings`:

- `etaAveragingSeconds` — постоянная времени усреднения мощности, диапазон 5–300 секунд, по умолчанию 45.
- `etaIdleHoldSeconds` — сколько сохранять среднее и последнее ETA при кратком IDLE, диапазон 0–120 секунд, по умолчанию 15.
- `etaMaxChangeMinutesPerSecond` — максимальная скорость изменения отображаемого ETA, диапазон 0.1–60 минут ETA за секунду реального времени, по умолчанию 2.

Примеры команд:

```text
set etaAveragingSeconds=60
set etaIdleHoldSeconds=20
set etaMaxChangeMinutesPerSecond=1
```

Рекомендуемые значения для ноутбука или другой импульсной нагрузки:

```text
etaAveragingSeconds=45
etaIdleHoldSeconds=15
etaMaxChangeMinutesPerSecond=2
```
