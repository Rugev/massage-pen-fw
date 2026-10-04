# Operating states

| State | Required behavior |
| --- | --- |
| Standby | MCU in low power, waiting for power-button or charger-interrupt wake |
| Charging | `SYS_ON` enabled; monitor battery analog input and charger; display battery charge on LEDs |
| Normal operation | `SYS_ON` enabled; monitor buttons, set LEDs, control vibration and heater |
| Hybrid | Charge while operating vibration and heating |

State transitions and implementation architecture will be decided later.
