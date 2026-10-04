/* Heater control using PWM and temperature feedback. */
#ifndef USER_HEATER_H
#define USER_HEATER_H

/* NMOS-switched resistive heater: low control signal means off. */
#define HEATER_RESISTANCE_MOHM                  2000U
#define HEATER_CTRL_ACTIVE_HIGH                 1U
#define HEATER_ABSOLUTE_MAX_TEMP_MDEGC          49000L

/* Level 0 means off, not a temperature target. Levels map to 0..3 LEDs. */
#define HEATER_LEVEL_COUNT                      4U
#define HEATER_LEVEL_0_TEMP_MDEGC               0L
#define HEATER_LEVEL_1_TEMP_MDEGC               40000L
#define HEATER_LEVEL_2_TEMP_MDEGC               44000L
#define HEATER_LEVEL_3_TEMP_MDEGC               48000L

/* Public API and implementation remain undecided. */

#endif /* USER_HEATER_H */
