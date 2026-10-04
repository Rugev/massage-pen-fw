/* Vibration control behavior. */
#ifndef USER_VIBRATION_H
#define USER_VIBRATION_H

/* Linear resonant motor; driver frequency tracking is intended. */
#define VIBRATION_NOMINAL_FREQUENCY_HZ          70U
#define VIBRATION_MAX_RMS_MV                    2000U

/* Levels 0..3 correspond to 0..3 illuminated LEDs. */
#define VIBRATION_LEVEL_COUNT                   4U
#define VIBRATION_LEVEL_0_RMS_MV                0U
#define VIBRATION_LEVEL_1_RMS_MV                500U
#define VIBRATION_LEVEL_2_RMS_MV                1000U
#define VIBRATION_LEVEL_3_RMS_MV                2000U

/* Public API and implementation remain undecided. */

#endif /* USER_VIBRATION_H */
