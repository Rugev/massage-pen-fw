/* DRV2624 programming reference, TI SLOS893D (August 2025), sections 7-8.
 * All field codes/defaults are UNSHIFTED; register defaults are complete bytes.
 * FIELD_PREP/GET only pack/extract bits: validate ranges before use.
 * WRITABLE_MASK covers documented functional fields, excluding reserved bits,
 * even where TI labels reserved bits R/W. Preserve reserved bits.
 * R/W unless a field comment says R. Hardware updates calibration and GO fields.
 * Initialization, actuator settings and application policy remain application-owned.
 */
#ifndef USER_DRV2624_H
#define USER_DRV2624_H

#include <stdint.h>

#define DRV2624_I2C_ADDRESS_7BIT                            0x5AU
#define DRV2624_I2C_BROADCAST_ADDRESS_7BIT                  0x58U
/* Unshifted addresses; STM32 HAL address arguments require a left shift.
 * Broadcast address responds only with I2C_BCAST_EN; use it for shared writes.
 */
#define DRV2624_REGISTER_COUNT                              49U
/* Undocumented gaps: 0x1E, 0x2B, 0x2D and 0x31..0xFC. Do not access. */
#define DRV2624_FIELD_PREP(mask, shift, code)               \
    ((((uint32_t)(code)) << (shift)) & (uint32_t)(mask))
#define DRV2624_FIELD_GET(mask, shift, byte)                \
    ((((uint32_t)(byte)) & (uint32_t)(mask)) >> (shift))
/* Each macro argument is evaluated once. To replace a normal R/W field:
 * (old & ~MASK) | DRV2624_FIELD_PREP(MASK, SHIFT, code).
 * Never read-modify-write STATUS (read clears events) or treat GO as plain config.
 */

/* ID: Device identity and silicon revision (R). */
#define DRV2624_REG_ID                                      0x00U
#define DRV2624_ID_RESET_DEFAULT                            0x03U
#define DRV2624_ID_RESERVED_MASK                            0x00U
#define DRV2624_ID_WRITABLE_MASK                            0x00U

/* CHIPID [7:4]
 * R; identifies chip family. */
#define DRV2624_CHIPID_SHIFT                                4U
#define DRV2624_CHIPID_MASK                                 0xF0U
#define DRV2624_CHIPID_DEFAULT                              0U
#define DRV2624_CHIPID_DRV2624                              0U
#define DRV2624_CHIPID_DRV2625                              1U

/* REV [3:0]
 * R; commercially released revisions are 2 and 3. */
#define DRV2624_REV_SHIFT                                   0U
#define DRV2624_REV_MASK                                    0x0FU
#define DRV2624_REV_DEFAULT                                 3U
#define DRV2624_REV_REV2                                    2U
#define DRV2624_REV_REV3                                    3U

/* STATUS: Sticky event/results byte; reading clears all functional flags. */
#define DRV2624_REG_STATUS                                  0x01U
#define DRV2624_STATUS_RESET_DEFAULT                        0x00U
#define DRV2624_STATUS_RESERVED_MASK                        0x60U
#define DRV2624_STATUS_WRITABLE_MASK                        0x00U

#define DRV2624_STATUS_READ_CLEAR_MASK                      0x9FU

/* DIAG_RESULT [7]
 * R; 0 success, 1 diagnostics or calibration failed; resistance alone does not set this. Clears on STATUS read. */
#define DRV2624_DIAG_RESULT_SHIFT                           7U
#define DRV2624_DIAG_RESULT_MASK                            0x80U
#define DRV2624_DIAG_RESULT_DEFAULT                         0U

/* PRG_ERROR [4]
 * R; 1 invalid RAM waveform format. Clears on STATUS read. */
#define DRV2624_PRG_ERROR_SHIFT                             4U
#define DRV2624_PRG_ERROR_MASK                              0x10U
#define DRV2624_PRG_ERROR_DEFAULT                           0U

/* PROCESS_DONE [3]
 * R; 1 sequencer, diagnostics or calibration completed; not RTP or aborted processes. Clears on STATUS read. */
#define DRV2624_PROCESS_DONE_SHIFT                          3U
#define DRV2624_PROCESS_DONE_MASK                           0x08U
#define DRV2624_PROCESS_DONE_DEFAULT                        0U

/* UVLO [2]
 * R; 1 VDD fell below UVLO_THRES. Clears on STATUS read. */
#define DRV2624_UVLO_SHIFT                                  2U
#define DRV2624_UVLO_MASK                                   0x04U
#define DRV2624_UVLO_DEFAULT                                0U

/* OVER_TEMP [1]
 * R; 1 overtemperature detected. Clears on STATUS read. */
#define DRV2624_OVER_TEMP_SHIFT                             1U
#define DRV2624_OVER_TEMP_MASK                              0x02U
#define DRV2624_OVER_TEMP_DEFAULT                           0U

/* OC_DETECT [0]
 * R; 1 output overcurrent detected. Clears on STATUS read. */
#define DRV2624_OC_DETECT_SHIFT                             0U
#define DRV2624_OC_DETECT_MASK                              0x01U
#define DRV2624_OC_DETECT_DEFAULT                           0U

/* INTZ_MASK: 1 masks the matching interrupt; status remains readable. */
#define DRV2624_REG_INTZ_MASK                               0x02U
#define DRV2624_INTZ_MASK_RESET_DEFAULT                     0x18U
#define DRV2624_INTZ_MASK_RESERVED_MASK                     0xE0U
#define DRV2624_INTZ_MASK_WRITABLE_MASK                     0x1FU

/* INTZ_MASK [4:0]
 * Bits 0..4 mask OC_DETECT, OVER_TEMP, UVLO, PROCESS_DONE, PRG_ERROR.
 * Register table omits bit 4 description; Figure 7-6 explicitly assigns PRG_ERROR. */
#define DRV2624_INTZ_MASK_SHIFT                             0U
#define DRV2624_INTZ_MASK_MASK                              0x1FU
#define DRV2624_INTZ_MASK_DEFAULT                           24U
#define DRV2624_INTZ_MASK_OC_DETECT                         1U
#define DRV2624_INTZ_MASK_OVER_TEMP                         2U
#define DRV2624_INTZ_MASK_UVLO                              4U
#define DRV2624_INTZ_MASK_PROCESS_DONE                      8U
#define DRV2624_INTZ_MASK_PRG_ERROR                         16U
#define DRV2624_INTZ_MASK_ALL                               31U
#define DRV2624_INTZ_MASK_NONE                              0U

/* DIAG_Z_RESULT: Actuator resistance measurement result (R); use with CURRENT_K. */
#define DRV2624_REG_DIAG_Z_RESULT                           0x03U
#define DRV2624_DIAG_Z_RESULT_RESET_DEFAULT                 0x00U
#define DRV2624_DIAG_Z_RESULT_RESERVED_MASK                 0x00U
#define DRV2624_DIAG_Z_RESULT_WRITABLE_MASK                 0x00U

/* DIAG_Z_RESULT [7:0]
 * R; R_mohm = 478430 * code / (719 + 4 * CURRENT_K).
 * Use a wide integer intermediate; meaningful after resistance diagnostics. */
#define DRV2624_DIAG_Z_RESULT_SHIFT                         0U
#define DRV2624_DIAG_Z_RESULT_MASK                          0xFFU
#define DRV2624_DIAG_Z_RESULT_DEFAULT                       0U

/* VBAT: VDD measurement (R), updated only during waveform playback. */
#define DRV2624_REG_VBAT                                    0x04U
#define DRV2624_VBAT_RESET_DEFAULT                          0x00U
#define DRV2624_VBAT_RESERVED_MASK                          0x00U
#define DRV2624_VBAT_WRITABLE_MASK                          0x00U

/* VBAT [7:0]
 * R; VDD_mV = code * 5600 / 255 (Eq. 5). Not a battery state-of-charge value. */
#define DRV2624_VBAT_SHIFT                                  0U
#define DRV2624_VBAT_MASK                                   0xFFU
#define DRV2624_VBAT_DEFAULT                                0U

/* LRA_PERIOD_H: Measured resonance period high bits (R). */
#define DRV2624_REG_LRA_PERIOD_H                            0x05U
#define DRV2624_LRA_PERIOD_H_RESET_DEFAULT                  0x00U
#define DRV2624_LRA_PERIOD_H_RESERVED_MASK                  0xFCU
#define DRV2624_LRA_PERIOD_H_WRITABLE_MASK                  0x00U

/* LRA_PERIOD_H [1:0]
 * R; bits 9:8 of measured period. Read high first, then low during the
 * same playback; high read locks pair until low read or waveform end. */
#define DRV2624_LRA_PERIOD_H_SHIFT                          0U
#define DRV2624_LRA_PERIOD_H_MASK                           0x03U
#define DRV2624_LRA_PERIOD_H_DEFAULT                        0U

/* LRA_PERIOD_L: Measured resonance period low bits (R). */
#define DRV2624_REG_LRA_PERIOD_L                            0x06U
#define DRV2624_LRA_PERIOD_L_RESET_DEFAULT                  0x00U
#define DRV2624_LRA_PERIOD_L_RESERVED_MASK                  0x00U
#define DRV2624_LRA_PERIOD_L_WRITABLE_MASK                  0x00U

/* LRA_PERIOD_L [7:0]
 * R; combine (high & 3) << 8 | low. Period_ns = code * 24390;
 * frequency_mHz = 1000000000000 / period_ns (64-bit). Reject zero.
 * Read during active tracking; accuracy is not verified during braking. */
#define DRV2624_LRA_PERIOD_L_SHIFT                          0U
#define DRV2624_LRA_PERIOD_L_MASK                           0xFFU
#define DRV2624_LRA_PERIOD_L_DEFAULT                        0U

/* MODE: Broadcast, period reporting, LDO compensation, trigger and process selection. */
#define DRV2624_REG_MODE                                    0x07U
#define DRV2624_MODE_RESET_DEFAULT                          0x44U
#define DRV2624_MODE_RESERVED_MASK                          0x00U
#define DRV2624_MODE_WRITABLE_MASK                          0xFFU

/* I2C_BCAST_EN [7]
 * 0 disable, 1 enable response to I2C_BROADCAST_ADDRESS_7BIT. */
#define DRV2624_I2C_BCAST_EN_SHIFT                          7U
#define DRV2624_I2C_BCAST_EN_MASK                           0x80U
#define DRV2624_I2C_BCAST_EN_DEFAULT                        0U

/* LRA_PERIOD_AVG_DIS [6]
 * Averaging uses a four-period shift register initially filled with zeros. */
#define DRV2624_LRA_PERIOD_AVG_DIS_SHIFT                    6U
#define DRV2624_LRA_PERIOD_AVG_DIS_MASK                     0x40U
#define DRV2624_LRA_PERIOD_AVG_DIS_DEFAULT                  1U
#define DRV2624_LRA_PERIOD_AVG_DIS_AVERAGE_4                0U
#define DRV2624_LRA_PERIOD_AVG_DIS_LAST_PERIOD              1U

/* LINEREG_COMP_SEL [5:4]
 * LDO variation compensation. */
#define DRV2624_LINEREG_COMP_SEL_SHIFT                      4U
#define DRV2624_LINEREG_COMP_SEL_MASK                       0x30U
#define DRV2624_LINEREG_COMP_SEL_DEFAULT                    0U
#define DRV2624_LINEREG_COMP_SEL_0_PERCENT                  0U
#define DRV2624_LINEREG_COMP_SEL_2_PERCENT                  1U
#define DRV2624_LINEREG_COMP_SEL_4_PERCENT                  2U
#define DRV2624_LINEREG_COMP_SEL_5_PERCENT                  3U

/* TRIG_PIN_FUNC [3:2]
 * Changing during a process aborts it. Code 3 reserved. Pulse mode
 * also permits GO; level mode ignores GO; interrupt mode uses GO only. */
#define DRV2624_TRIG_PIN_FUNC_SHIFT                         2U
#define DRV2624_TRIG_PIN_FUNC_MASK                          0x0CU
#define DRV2624_TRIG_PIN_FUNC_DEFAULT                       1U
#define DRV2624_TRIG_PIN_FUNC_PULSE                         0U
#define DRV2624_TRIG_PIN_FUNC_LEVEL                         1U
#define DRV2624_TRIG_PIN_FUNC_INTERRUPT                     2U

/* MODE [1:0]
 * Select process, then trigger it. Changing during a process aborts it. */
#define DRV2624_MODE_SHIFT                                  0U
#define DRV2624_MODE_MASK                                   0x03U
#define DRV2624_MODE_DEFAULT                                0U
#define DRV2624_MODE_RTP                                    0U
#define DRV2624_MODE_WAVEFORM_SEQUENCER                     1U
#define DRV2624_MODE_DIAGNOSTICS                            2U
#define DRV2624_MODE_AUTO_CALIBRATION                       3U

/* CONTROL: Actuator selection, loop behavior and braking. */
#define DRV2624_REG_CONTROL                                 0x08U
#define DRV2624_CONTROL_RESET_DEFAULT                       0x88U
#define DRV2624_CONTROL_RESERVED_MASK                       0x03U
#define DRV2624_CONTROL_WRITABLE_MASK                       0xFCU

/* LRA_ERM [7]
 * Configure before calibration. */
#define DRV2624_LRA_ERM_SHIFT                               7U
#define DRV2624_LRA_ERM_MASK                                0x80U
#define DRV2624_LRA_ERM_DEFAULT                             1U
#define DRV2624_LRA_ERM_ERM                                 0U
#define DRV2624_LRA_ERM_LRA                                 1U

/* CONTROL_LOOP [6]
 * Applies to both actuator types. */
#define DRV2624_CONTROL_LOOP_SHIFT                          6U
#define DRV2624_CONTROL_LOOP_MASK                           0x40U
#define DRV2624_CONTROL_LOOP_DEFAULT                        0U
#define DRV2624_CONTROL_LOOP_CLOSED                         0U
#define DRV2624_CONTROL_LOOP_OPEN                           1U

/* HYBRID_LOOP [5]
 * Select full closed loop or hybrid operation. */
#define DRV2624_HYBRID_LOOP_SHIFT                           5U
#define DRV2624_HYBRID_LOOP_MASK                            0x20U
#define DRV2624_HYBRID_LOOP_DEFAULT                         0U
#define DRV2624_HYBRID_LOOP_FULL_CLOSED                     0U
#define DRV2624_HYBRID_LOOP_HYBRID                          1U

/* AUTO_BRK_OL [4]
 * 0 disable, 1 enter closed loop for braking when open-loop input <= 0.
 * Requires valid closed-loop calibration; positive input remains open loop. */
#define DRV2624_AUTO_BRK_OL_SHIFT                           4U
#define DRV2624_AUTO_BRK_OL_MASK                            0x10U
#define DRV2624_AUTO_BRK_OL_DEFAULT                         0U

/* AUTO_BRK_INTO_STBY [3]
 * 0 immediate standby, 1 brake moving actuator before standby.
 * Ignored for calibration; bypassed by critical faults/NRST. See guide caveats. */
#define DRV2624_AUTO_BRK_INTO_STBY_SHIFT                    3U
#define DRV2624_AUTO_BRK_INTO_STBY_MASK                     0x08U
#define DRV2624_AUTO_BRK_INTO_STBY_DEFAULT                  1U

/* INPUT_SLOPE_CHECK [2]
 * 0 disable, 1 stay open loop until requested transition is large enough;
 * ignored unless HYBRID_LOOP enabled. No numeric transition threshold specified. */
#define DRV2624_INPUT_SLOPE_CHECK_SHIFT                     2U
#define DRV2624_INPUT_SLOPE_CHECK_MASK                      0x04U
#define DRV2624_INPUT_SLOPE_CHECK_DEFAULT                   0U

/* BATTERY_CONTROL: Battery clamp levels and undervoltage protection. */
#define DRV2624_REG_BATTERY_CONTROL                         0x09U
#define DRV2624_BATTERY_CONTROL_RESET_DEFAULT               0x00U
#define DRV2624_BATTERY_CONTROL_RESERVED_MASK               0x38U
#define DRV2624_BATTERY_CONTROL_WRITABLE_MASK               0xC7U

/* BAT_LIFE_EXT_LVL_EN [7:6]
 * Select battery preservation levels. Code 3 is undocumented. */
#define DRV2624_BAT_LIFE_EXT_LVL_EN_SHIFT                   6U
#define DRV2624_BAT_LIFE_EXT_LVL_EN_MASK                    0xC0U
#define DRV2624_BAT_LIFE_EXT_LVL_EN_DEFAULT                 0U
#define DRV2624_BAT_LIFE_EXT_LVL_EN_DISABLED                0U
#define DRV2624_BAT_LIFE_EXT_LVL_EN_LEVEL1                  1U
#define DRV2624_BAT_LIFE_EXT_LVL_EN_LEVEL1_AND_LEVEL2       2U

/* UVLO_THRES [2:0]
 * Output immediately disabled below threshold; device goes to standby. */
#define DRV2624_UVLO_THRES_SHIFT                            0U
#define DRV2624_UVLO_THRES_MASK                             0x07U
#define DRV2624_UVLO_THRES_DEFAULT                          0U
#define DRV2624_UVLO_THRES_2500_MV                          0U
#define DRV2624_UVLO_THRES_2600_MV                          1U
#define DRV2624_UVLO_THRES_2700_MV                          2U
#define DRV2624_UVLO_THRES_2800_MV                          3U
#define DRV2624_UVLO_THRES_2900_MV                          4U
#define DRV2624_UVLO_THRES_3000_MV                          5U
#define DRV2624_UVLO_THRES_3100_MV                          6U
#define DRV2624_UVLO_THRES_3200_MV                          7U

/* BAT_LIFE_EXT_LVL1: VDD threshold for the matching reduced overdrive clamp. */
#define DRV2624_REG_BAT_LIFE_EXT_LVL1                       0x0AU
#define DRV2624_BAT_LIFE_EXT_LVL1_RESET_DEFAULT             0x92U
#define DRV2624_BAT_LIFE_EXT_LVL1_RESERVED_MASK             0x00U
#define DRV2624_BAT_LIFE_EXT_LVL1_WRITABLE_MASK             0xFFU

/* BAT_LIFE_EXT_LVL1 [7:0]
 * Sampled at effect start only. Set LVL1 > LVL2; OD_CLAMP_LVL1 >= OD_CLAMP_LVL2.
 * Compared to VBAT codes; using the VBAT scale for thresholds is inferred
 * (code * 5600 / 255 mV), not an independently specified equation. */
#define DRV2624_BAT_LIFE_EXT_LVL1_SHIFT                     0U
#define DRV2624_BAT_LIFE_EXT_LVL1_MASK                      0xFFU
#define DRV2624_BAT_LIFE_EXT_LVL1_DEFAULT                   146U

/* BAT_LIFE_EXT_LVL2: VDD threshold for the matching reduced overdrive clamp. */
#define DRV2624_REG_BAT_LIFE_EXT_LVL2                       0x0BU
#define DRV2624_BAT_LIFE_EXT_LVL2_RESET_DEFAULT             0x8DU
#define DRV2624_BAT_LIFE_EXT_LVL2_RESERVED_MASK             0x00U
#define DRV2624_BAT_LIFE_EXT_LVL2_WRITABLE_MASK             0xFFU

/* BAT_LIFE_EXT_LVL2 [7:0]
 * Sampled at effect start only. Set LVL1 > LVL2; OD_CLAMP_LVL1 >= OD_CLAMP_LVL2.
 * Compared to VBAT codes; using the VBAT scale for thresholds is inferred
 * (code * 5600 / 255 mV), not an independently specified equation. */
#define DRV2624_BAT_LIFE_EXT_LVL2_SHIFT                     0U
#define DRV2624_BAT_LIFE_EXT_LVL2_MASK                      0xFFU
#define DRV2624_BAT_LIFE_EXT_LVL2_DEFAULT                   141U

/* GO: Process trigger and running state, not ordinary configuration. */
#define DRV2624_REG_GO                                      0x0CU
#define DRV2624_GO_RESET_DEFAULT                            0x00U
#define DRV2624_GO_RESERVED_MASK                            0xFEU
#define DRV2624_GO_WRITABLE_MASK                            0x01U

/* GO [0]
 * 1 start selected process, stays high while running, clears at completion.
 * 0 cancels (except trigger-controlled calibration, where it requests completion).
 * External triggers update GO too; software writes ignored in level-trigger mode. */
#define DRV2624_GO_SHIFT                                    0U
#define DRV2624_GO_MASK                                     0x01U
#define DRV2624_GO_DEFAULT                                  0U
#define DRV2624_GO_STOP                                     0U
#define DRV2624_GO_START                                    1U

/* PLAYBACK_CONTROL: RAM timing interval and library strength scaling. */
#define DRV2624_REG_PLAYBACK_CONTROL                        0x0DU
#define DRV2624_PLAYBACK_CONTROL_RESET_DEFAULT              0x00U
#define DRV2624_PLAYBACK_CONTROL_RESERVED_MASK              0xDCU
#define DRV2624_PLAYBACK_CONTROL_WRITABLE_MASK              0x23U

/* PLAYBACK_INTERVAL [5]
 * Code selects a physical tick duration; do not multiply by raw code. */
#define DRV2624_PLAYBACK_INTERVAL_SHIFT                     5U
#define DRV2624_PLAYBACK_INTERVAL_MASK                      0x20U
#define DRV2624_PLAYBACK_INTERVAL_DEFAULT                   0U
#define DRV2624_PLAYBACK_INTERVAL_5_MS                      0U
#define DRV2624_PLAYBACK_INTERVAL_1_MS                      1U

/* DIG_MEM_GAIN [1:0]
 * RAM waveform attenuation; ignored in RTP. */
#define DRV2624_DIG_MEM_GAIN_SHIFT                          0U
#define DRV2624_DIG_MEM_GAIN_MASK                           0x03U
#define DRV2624_DIG_MEM_GAIN_DEFAULT                        0U
#define DRV2624_DIG_MEM_GAIN_100_PERCENT                    0U
#define DRV2624_DIG_MEM_GAIN_75_PERCENT                     1U
#define DRV2624_DIG_MEM_GAIN_50_PERCENT                     2U
#define DRV2624_DIG_MEM_GAIN_25_PERCENT                     3U

/* RTP_INPUT: Signed 8-bit amplitude for an active RTP process. */
#define DRV2624_REG_RTP_INPUT                               0x0EU
#define DRV2624_RTP_INPUT_RESET_DEFAULT                     0x7FU
#define DRV2624_RTP_INPUT_RESERVED_MASK                     0x00U
#define DRV2624_RTP_INPUT_WRITABLE_MASK                     0xFFU

/* RTP_INPUT [7:0]
 * Two's complement: -128..127. Closed loop: positive = drive,
 * <= 0 = automatic brake. Open loop: signed drive polarity/phase unless AUTO_BRK_OL.
 * Full scale uses RATED_VOLTAGE (closed loop) or OD_CLAMP (open loop). */
#define DRV2624_RTP_INPUT_SHIFT                             0U
#define DRV2624_RTP_INPUT_MASK                              0xFFU
#define DRV2624_RTP_INPUT_DEFAULT                           127U

#define DRV2624_RTP_INPUT_MIN                               (-128)
#define DRV2624_RTP_INPUT_MAX                               127
#define DRV2624_RTP_INPUT_ZERO                              0U

/* WAV_FRM_SEQ1: Sequence slot 1: waveform identifier or timed wait. */
#define DRV2624_REG_WAV_FRM_SEQ1                            0x0FU
#define DRV2624_WAV_FRM_SEQ1_RESET_DEFAULT                  0x01U
#define DRV2624_WAV_FRM_SEQ1_RESERVED_MASK                  0x00U
#define DRV2624_WAV_FRM_SEQ1_WRITABLE_MASK                  0xFFU

/* WAIT1 [7]
 * Interpret slot as waveform ID or delay; delay = slot code * SEQUENCE_WAIT_TICK_MS. */
#define DRV2624_WAIT1_SHIFT                                 7U
#define DRV2624_WAIT1_MASK                                  0x80U
#define DRV2624_WAIT1_DEFAULT                               0U
#define DRV2624_WAIT1_WAVEFORM                              0U
#define DRV2624_WAIT1_DELAY                                 1U

/* WAV_FRM_SEQ1 [6:0]
 * WAIT clear: 0 terminates sequence, 1..127 select RAM effect.
 * WAIT set: 0..127 delay ticks; do not assume a zero-length wait terminates. */
#define DRV2624_WAV_FRM_SEQ1_SHIFT                          0U
#define DRV2624_WAV_FRM_SEQ1_MASK                           0x7FU
#define DRV2624_WAV_FRM_SEQ1_DEFAULT                        1U

/* WAV_FRM_SEQ2: Sequence slot 2: waveform identifier or timed wait. */
#define DRV2624_REG_WAV_FRM_SEQ2                            0x10U
#define DRV2624_WAV_FRM_SEQ2_RESET_DEFAULT                  0x00U
#define DRV2624_WAV_FRM_SEQ2_RESERVED_MASK                  0x00U
#define DRV2624_WAV_FRM_SEQ2_WRITABLE_MASK                  0xFFU

/* WAIT2 [7]
 * Interpret slot as waveform ID or delay; delay = slot code * SEQUENCE_WAIT_TICK_MS. */
#define DRV2624_WAIT2_SHIFT                                 7U
#define DRV2624_WAIT2_MASK                                  0x80U
#define DRV2624_WAIT2_DEFAULT                               0U
#define DRV2624_WAIT2_WAVEFORM                              0U
#define DRV2624_WAIT2_DELAY                                 1U

/* WAV_FRM_SEQ2 [6:0]
 * WAIT clear: 0 terminates sequence, 1..127 select RAM effect.
 * WAIT set: 0..127 delay ticks; do not assume a zero-length wait terminates. */
#define DRV2624_WAV_FRM_SEQ2_SHIFT                          0U
#define DRV2624_WAV_FRM_SEQ2_MASK                           0x7FU
#define DRV2624_WAV_FRM_SEQ2_DEFAULT                        0U

/* WAV_FRM_SEQ3: Sequence slot 3: waveform identifier or timed wait. */
#define DRV2624_REG_WAV_FRM_SEQ3                            0x11U
#define DRV2624_WAV_FRM_SEQ3_RESET_DEFAULT                  0x00U
#define DRV2624_WAV_FRM_SEQ3_RESERVED_MASK                  0x00U
#define DRV2624_WAV_FRM_SEQ3_WRITABLE_MASK                  0xFFU

/* WAIT3 [7]
 * Interpret slot as waveform ID or delay; delay = slot code * SEQUENCE_WAIT_TICK_MS. */
#define DRV2624_WAIT3_SHIFT                                 7U
#define DRV2624_WAIT3_MASK                                  0x80U
#define DRV2624_WAIT3_DEFAULT                               0U
#define DRV2624_WAIT3_WAVEFORM                              0U
#define DRV2624_WAIT3_DELAY                                 1U

/* WAV_FRM_SEQ3 [6:0]
 * WAIT clear: 0 terminates sequence, 1..127 select RAM effect.
 * WAIT set: 0..127 delay ticks; do not assume a zero-length wait terminates. */
#define DRV2624_WAV_FRM_SEQ3_SHIFT                          0U
#define DRV2624_WAV_FRM_SEQ3_MASK                           0x7FU
#define DRV2624_WAV_FRM_SEQ3_DEFAULT                        0U

/* WAV_FRM_SEQ4: Sequence slot 4: waveform identifier or timed wait. */
#define DRV2624_REG_WAV_FRM_SEQ4                            0x12U
#define DRV2624_WAV_FRM_SEQ4_RESET_DEFAULT                  0x00U
#define DRV2624_WAV_FRM_SEQ4_RESERVED_MASK                  0x00U
#define DRV2624_WAV_FRM_SEQ4_WRITABLE_MASK                  0xFFU

/* WAIT4 [7]
 * Interpret slot as waveform ID or delay; delay = slot code * SEQUENCE_WAIT_TICK_MS. */
#define DRV2624_WAIT4_SHIFT                                 7U
#define DRV2624_WAIT4_MASK                                  0x80U
#define DRV2624_WAIT4_DEFAULT                               0U
#define DRV2624_WAIT4_WAVEFORM                              0U
#define DRV2624_WAIT4_DELAY                                 1U

/* WAV_FRM_SEQ4 [6:0]
 * WAIT clear: 0 terminates sequence, 1..127 select RAM effect.
 * WAIT set: 0..127 delay ticks; do not assume a zero-length wait terminates. */
#define DRV2624_WAV_FRM_SEQ4_SHIFT                          0U
#define DRV2624_WAV_FRM_SEQ4_MASK                           0x7FU
#define DRV2624_WAV_FRM_SEQ4_DEFAULT                        0U

/* WAV_FRM_SEQ5: Sequence slot 5: waveform identifier or timed wait. */
#define DRV2624_REG_WAV_FRM_SEQ5                            0x13U
#define DRV2624_WAV_FRM_SEQ5_RESET_DEFAULT                  0x00U
#define DRV2624_WAV_FRM_SEQ5_RESERVED_MASK                  0x00U
#define DRV2624_WAV_FRM_SEQ5_WRITABLE_MASK                  0xFFU

/* WAIT5 [7]
 * Interpret slot as waveform ID or delay; delay = slot code * SEQUENCE_WAIT_TICK_MS. */
#define DRV2624_WAIT5_SHIFT                                 7U
#define DRV2624_WAIT5_MASK                                  0x80U
#define DRV2624_WAIT5_DEFAULT                               0U
#define DRV2624_WAIT5_WAVEFORM                              0U
#define DRV2624_WAIT5_DELAY                                 1U

/* WAV_FRM_SEQ5 [6:0]
 * WAIT clear: 0 terminates sequence, 1..127 select RAM effect.
 * WAIT set: 0..127 delay ticks; do not assume a zero-length wait terminates. */
#define DRV2624_WAV_FRM_SEQ5_SHIFT                          0U
#define DRV2624_WAV_FRM_SEQ5_MASK                           0x7FU
#define DRV2624_WAV_FRM_SEQ5_DEFAULT                        0U

/* WAV_FRM_SEQ6: Sequence slot 6: waveform identifier or timed wait. */
#define DRV2624_REG_WAV_FRM_SEQ6                            0x14U
#define DRV2624_WAV_FRM_SEQ6_RESET_DEFAULT                  0x00U
#define DRV2624_WAV_FRM_SEQ6_RESERVED_MASK                  0x00U
#define DRV2624_WAV_FRM_SEQ6_WRITABLE_MASK                  0xFFU

/* WAIT6 [7]
 * Interpret slot as waveform ID or delay; delay = slot code * SEQUENCE_WAIT_TICK_MS. */
#define DRV2624_WAIT6_SHIFT                                 7U
#define DRV2624_WAIT6_MASK                                  0x80U
#define DRV2624_WAIT6_DEFAULT                               0U
#define DRV2624_WAIT6_WAVEFORM                              0U
#define DRV2624_WAIT6_DELAY                                 1U

/* WAV_FRM_SEQ6 [6:0]
 * WAIT clear: 0 terminates sequence, 1..127 select RAM effect.
 * WAIT set: 0..127 delay ticks; do not assume a zero-length wait terminates. */
#define DRV2624_WAV_FRM_SEQ6_SHIFT                          0U
#define DRV2624_WAV_FRM_SEQ6_MASK                           0x7FU
#define DRV2624_WAV_FRM_SEQ6_DEFAULT                        0U

/* WAV_FRM_SEQ7: Sequence slot 7: waveform identifier or timed wait. */
#define DRV2624_REG_WAV_FRM_SEQ7                            0x15U
#define DRV2624_WAV_FRM_SEQ7_RESET_DEFAULT                  0x00U
#define DRV2624_WAV_FRM_SEQ7_RESERVED_MASK                  0x00U
#define DRV2624_WAV_FRM_SEQ7_WRITABLE_MASK                  0xFFU

/* WAIT7 [7]
 * Interpret slot as waveform ID or delay; delay = slot code * SEQUENCE_WAIT_TICK_MS. */
#define DRV2624_WAIT7_SHIFT                                 7U
#define DRV2624_WAIT7_MASK                                  0x80U
#define DRV2624_WAIT7_DEFAULT                               0U
#define DRV2624_WAIT7_WAVEFORM                              0U
#define DRV2624_WAIT7_DELAY                                 1U

/* WAV_FRM_SEQ7 [6:0]
 * WAIT clear: 0 terminates sequence, 1..127 select RAM effect.
 * WAIT set: 0..127 delay ticks; do not assume a zero-length wait terminates. */
#define DRV2624_WAV_FRM_SEQ7_SHIFT                          0U
#define DRV2624_WAV_FRM_SEQ7_MASK                           0x7FU
#define DRV2624_WAV_FRM_SEQ7_DEFAULT                        0U

/* WAV_FRM_SEQ8: Sequence slot 8: waveform identifier or timed wait. */
#define DRV2624_REG_WAV_FRM_SEQ8                            0x16U
#define DRV2624_WAV_FRM_SEQ8_RESET_DEFAULT                  0x00U
#define DRV2624_WAV_FRM_SEQ8_RESERVED_MASK                  0x00U
#define DRV2624_WAV_FRM_SEQ8_WRITABLE_MASK                  0xFFU

/* WAIT8 [7]
 * Interpret slot as waveform ID or delay; delay = slot code * SEQUENCE_WAIT_TICK_MS. */
#define DRV2624_WAIT8_SHIFT                                 7U
#define DRV2624_WAIT8_MASK                                  0x80U
#define DRV2624_WAIT8_DEFAULT                               0U
#define DRV2624_WAIT8_WAVEFORM                              0U
#define DRV2624_WAIT8_DELAY                                 1U

/* WAV_FRM_SEQ8 [6:0]
 * WAIT clear: 0 terminates sequence, 1..127 select RAM effect.
 * WAIT set: 0..127 delay ticks; do not assume a zero-length wait terminates. */
#define DRV2624_WAV_FRM_SEQ8_SHIFT                          0U
#define DRV2624_WAV_FRM_SEQ8_MASK                           0x7FU
#define DRV2624_WAV_FRM_SEQ8_DEFAULT                        0U

/* WAV_SEQ_LOOP1: Per-slot repeat counts, including wait slots. */
#define DRV2624_REG_WAV_SEQ_LOOP1                           0x17U
#define DRV2624_WAV_SEQ_LOOP1_RESET_DEFAULT                 0x00U
#define DRV2624_WAV_SEQ_LOOP1_RESERVED_MASK                 0x00U
#define DRV2624_WAV_SEQ_LOOP1_WRITABLE_MASK                 0xFFU

/* WAV1_SEQ_LOOP [1:0]
 * Code is extra repetitions; total plays = code + 1. */
#define DRV2624_WAV1_SEQ_LOOP_SHIFT                         0U
#define DRV2624_WAV1_SEQ_LOOP_MASK                          0x03U
#define DRV2624_WAV1_SEQ_LOOP_DEFAULT                       0U
#define DRV2624_WAV1_SEQ_LOOP_PLAY_1_TIMES                  0U
#define DRV2624_WAV1_SEQ_LOOP_PLAY_2_TIMES                  1U
#define DRV2624_WAV1_SEQ_LOOP_PLAY_3_TIMES                  2U
#define DRV2624_WAV1_SEQ_LOOP_PLAY_4_TIMES                  3U

/* WAV2_SEQ_LOOP [3:2]
 * Code is extra repetitions; total plays = code + 1. */
#define DRV2624_WAV2_SEQ_LOOP_SHIFT                         2U
#define DRV2624_WAV2_SEQ_LOOP_MASK                          0x0CU
#define DRV2624_WAV2_SEQ_LOOP_DEFAULT                       0U
#define DRV2624_WAV2_SEQ_LOOP_PLAY_1_TIMES                  0U
#define DRV2624_WAV2_SEQ_LOOP_PLAY_2_TIMES                  1U
#define DRV2624_WAV2_SEQ_LOOP_PLAY_3_TIMES                  2U
#define DRV2624_WAV2_SEQ_LOOP_PLAY_4_TIMES                  3U

/* WAV3_SEQ_LOOP [5:4]
 * Code is extra repetitions; total plays = code + 1. */
#define DRV2624_WAV3_SEQ_LOOP_SHIFT                         4U
#define DRV2624_WAV3_SEQ_LOOP_MASK                          0x30U
#define DRV2624_WAV3_SEQ_LOOP_DEFAULT                       0U
#define DRV2624_WAV3_SEQ_LOOP_PLAY_1_TIMES                  0U
#define DRV2624_WAV3_SEQ_LOOP_PLAY_2_TIMES                  1U
#define DRV2624_WAV3_SEQ_LOOP_PLAY_3_TIMES                  2U
#define DRV2624_WAV3_SEQ_LOOP_PLAY_4_TIMES                  3U

/* WAV4_SEQ_LOOP [7:6]
 * Code is extra repetitions; total plays = code + 1. */
#define DRV2624_WAV4_SEQ_LOOP_SHIFT                         6U
#define DRV2624_WAV4_SEQ_LOOP_MASK                          0xC0U
#define DRV2624_WAV4_SEQ_LOOP_DEFAULT                       0U
#define DRV2624_WAV4_SEQ_LOOP_PLAY_1_TIMES                  0U
#define DRV2624_WAV4_SEQ_LOOP_PLAY_2_TIMES                  1U
#define DRV2624_WAV4_SEQ_LOOP_PLAY_3_TIMES                  2U
#define DRV2624_WAV4_SEQ_LOOP_PLAY_4_TIMES                  3U

/* WAV_SEQ_LOOP2: Per-slot repeat counts, including wait slots. */
#define DRV2624_REG_WAV_SEQ_LOOP2                           0x18U
#define DRV2624_WAV_SEQ_LOOP2_RESET_DEFAULT                 0x00U
#define DRV2624_WAV_SEQ_LOOP2_RESERVED_MASK                 0x00U
#define DRV2624_WAV_SEQ_LOOP2_WRITABLE_MASK                 0xFFU

/* WAV5_SEQ_LOOP [1:0]
 * Code is extra repetitions; total plays = code + 1. */
#define DRV2624_WAV5_SEQ_LOOP_SHIFT                         0U
#define DRV2624_WAV5_SEQ_LOOP_MASK                          0x03U
#define DRV2624_WAV5_SEQ_LOOP_DEFAULT                       0U
#define DRV2624_WAV5_SEQ_LOOP_PLAY_1_TIMES                  0U
#define DRV2624_WAV5_SEQ_LOOP_PLAY_2_TIMES                  1U
#define DRV2624_WAV5_SEQ_LOOP_PLAY_3_TIMES                  2U
#define DRV2624_WAV5_SEQ_LOOP_PLAY_4_TIMES                  3U

/* WAV6_SEQ_LOOP [3:2]
 * Code is extra repetitions; total plays = code + 1. */
#define DRV2624_WAV6_SEQ_LOOP_SHIFT                         2U
#define DRV2624_WAV6_SEQ_LOOP_MASK                          0x0CU
#define DRV2624_WAV6_SEQ_LOOP_DEFAULT                       0U
#define DRV2624_WAV6_SEQ_LOOP_PLAY_1_TIMES                  0U
#define DRV2624_WAV6_SEQ_LOOP_PLAY_2_TIMES                  1U
#define DRV2624_WAV6_SEQ_LOOP_PLAY_3_TIMES                  2U
#define DRV2624_WAV6_SEQ_LOOP_PLAY_4_TIMES                  3U

/* WAV7_SEQ_LOOP [5:4]
 * Code is extra repetitions; total plays = code + 1. */
#define DRV2624_WAV7_SEQ_LOOP_SHIFT                         4U
#define DRV2624_WAV7_SEQ_LOOP_MASK                          0x30U
#define DRV2624_WAV7_SEQ_LOOP_DEFAULT                       0U
#define DRV2624_WAV7_SEQ_LOOP_PLAY_1_TIMES                  0U
#define DRV2624_WAV7_SEQ_LOOP_PLAY_2_TIMES                  1U
#define DRV2624_WAV7_SEQ_LOOP_PLAY_3_TIMES                  2U
#define DRV2624_WAV7_SEQ_LOOP_PLAY_4_TIMES                  3U

/* WAV8_SEQ_LOOP [7:6]
 * Code is extra repetitions; total plays = code + 1. */
#define DRV2624_WAV8_SEQ_LOOP_SHIFT                         6U
#define DRV2624_WAV8_SEQ_LOOP_MASK                          0xC0U
#define DRV2624_WAV8_SEQ_LOOP_DEFAULT                       0U
#define DRV2624_WAV8_SEQ_LOOP_PLAY_1_TIMES                  0U
#define DRV2624_WAV8_SEQ_LOOP_PLAY_2_TIMES                  1U
#define DRV2624_WAV8_SEQ_LOOP_PLAY_3_TIMES                  2U
#define DRV2624_WAV8_SEQ_LOOP_PLAY_4_TIMES                  3U

/* WAV_SEQ_MAIN_LOOP: Repeat entire sequence through terminator or last slot. */
#define DRV2624_REG_WAV_SEQ_MAIN_LOOP                       0x19U
#define DRV2624_WAV_SEQ_MAIN_LOOP_RESET_DEFAULT             0x00U
#define DRV2624_WAV_SEQ_MAIN_LOOP_RESERVED_MASK             0xF8U
#define DRV2624_WAV_SEQ_MAIN_LOOP_WRITABLE_MASK             0x07U

/* WAV_SEQ_MAIN_LOOP [2:0]
 * Codes 0..6 mean 1..7 total passes; code 7 repeats until stopped. */
#define DRV2624_WAV_SEQ_MAIN_LOOP_SHIFT                     0U
#define DRV2624_WAV_SEQ_MAIN_LOOP_MASK                      0x07U
#define DRV2624_WAV_SEQ_MAIN_LOOP_DEFAULT                   0U
#define DRV2624_WAV_SEQ_MAIN_LOOP_PLAY_1_TIMES              0U
#define DRV2624_WAV_SEQ_MAIN_LOOP_PLAY_2_TIMES              1U
#define DRV2624_WAV_SEQ_MAIN_LOOP_PLAY_3_TIMES              2U
#define DRV2624_WAV_SEQ_MAIN_LOOP_PLAY_4_TIMES              3U
#define DRV2624_WAV_SEQ_MAIN_LOOP_PLAY_5_TIMES              4U
#define DRV2624_WAV_SEQ_MAIN_LOOP_PLAY_6_TIMES              5U
#define DRV2624_WAV_SEQ_MAIN_LOOP_PLAY_7_TIMES              6U
#define DRV2624_WAV_SEQ_MAIN_LOOP_INFINITE                  7U

/* ODT: Signed RAM-library time offset; ignored in RTP. */
#define DRV2624_REG_ODT                                     0x1AU
#define DRV2624_ODT_RESET_DEFAULT                           0x00U
#define DRV2624_ODT_RESERVED_MASK                           0x00U
#define DRV2624_ODT_WRITABLE_MASK                           0xFFU

/* ODT [7:0]
 * Overdrive: largest positive amplitude; open loop only.
 * Two's complement -128..127; offset_ms = signed code * physical playback tick_ms.
 * Added to original time, including ramps. ODT/BRT automatic in closed loop. */
#define DRV2624_ODT_SHIFT                                   0U
#define DRV2624_ODT_MASK                                    0xFFU
#define DRV2624_ODT_DEFAULT                                 0U

/* SPT: Signed RAM-library time offset; ignored in RTP. */
#define DRV2624_REG_SPT                                     0x1BU
#define DRV2624_SPT_RESET_DEFAULT                           0x00U
#define DRV2624_SPT_RESERVED_MASK                           0x00U
#define DRV2624_SPT_WRITABLE_MASK                           0xFFU

/* SPT [7:0]
 * Positive sustain: other positive amplitudes.
 * Two's complement -128..127; offset_ms = signed code * physical playback tick_ms.
 * Added to original time, including ramps. ODT/BRT automatic in closed loop. */
#define DRV2624_SPT_SHIFT                                   0U
#define DRV2624_SPT_MASK                                    0xFFU
#define DRV2624_SPT_DEFAULT                                 0U

/* SNT: Signed RAM-library time offset; ignored in RTP. */
#define DRV2624_REG_SNT                                     0x1CU
#define DRV2624_SNT_RESET_DEFAULT                           0x00U
#define DRV2624_SNT_RESERVED_MASK                           0x00U
#define DRV2624_SNT_WRITABLE_MASK                           0xFFU

/* SNT [7:0]
 * Negative sustain: other negative amplitudes.
 * Two's complement -128..127; offset_ms = signed code * physical playback tick_ms.
 * Added to original time, including ramps. ODT/BRT automatic in closed loop. */
#define DRV2624_SNT_SHIFT                                   0U
#define DRV2624_SNT_MASK                                    0xFFU
#define DRV2624_SNT_DEFAULT                                 0U

/* BRT: Signed RAM-library time offset; ignored in RTP. */
#define DRV2624_REG_BRT                                     0x1DU
#define DRV2624_BRT_RESET_DEFAULT                           0x00U
#define DRV2624_BRT_RESERVED_MASK                           0x00U
#define DRV2624_BRT_WRITABLE_MASK                           0xFFU

/* BRT [7:0]
 * Brake: most negative amplitude; open loop only.
 * Two's complement -128..127; offset_ms = signed code * physical playback tick_ms.
 * Added to original time, including ramps. ODT/BRT automatic in closed loop. */
#define DRV2624_BRT_SHIFT                                   0U
#define DRV2624_BRT_MASK                                    0xFFU
#define DRV2624_BRT_DEFAULT                                 0U

/* RATED_VOLTAGE: Closed-loop full-scale steady-state reference and calibration input. */
#define DRV2624_REG_RATED_VOLTAGE                           0x1FU
#define DRV2624_RATED_VOLTAGE_RESET_DEFAULT                 0x3FU
#define DRV2624_RATED_VOLTAGE_RESERVED_MASK                 0x00U
#define DRV2624_RATED_VOLTAGE_WRITABLE_MASK                 0xFFU

/* RATED_VOLTAGE [7:0]
 * Ignored in open loop. Recalibrate after changes to set A_CAL_BEMF.
 * Eq. 6 ERM average_mV = 21.88 * code. Eq. 7 LRA RMS_mV =
 * 20.58 * code / sqrt(1 - (4 * sample_us + 300) * frequency_Hz / 1000000).
 * SAMPLE_TIME is physical time, not selector. Validate square-root domain. */
#define DRV2624_RATED_VOLTAGE_SHIFT                         0U
#define DRV2624_RATED_VOLTAGE_MASK                          0xFFU
#define DRV2624_RATED_VOLTAGE_DEFAULT                       63U

/* OD_CLAMP: Maximum overdrive/braking voltage; open-loop full-scale reference. */
#define DRV2624_REG_OD_CLAMP                                0x20U
#define DRV2624_OD_CLAMP_RESET_DEFAULT                      0x89U
#define DRV2624_OD_CLAMP_RESERVED_MASK                      0x00U
#define DRV2624_OD_CLAMP_WRITABLE_MASK                      0xFFU

/* OD_CLAMP [7:0]
 * Set before calibration; supply limits attainable voltage. Lowest clamp wins.
 * Eq. 11 LRA peak_mV = 21.22 * code. Eq. 10 ERM clamp_mV =
 * 21.64 * code * (drive_us - 300) / (drive_us + idiss_us + blanking_us).
 * Open-loop mappings, Eq. 8 ERM average_mV = 21.59 * code; Eq. 9 LRA
 * RMS_mV = 21.32 * code * sqrt(1 - frequency_Hz * 800 / 1000000).
 * These are distinct datasheet relationships, not interchangeable RMS/peak units. */
#define DRV2624_OD_CLAMP_SHIFT                              0U
#define DRV2624_OD_CLAMP_MASK                               0xFFU
#define DRV2624_OD_CLAMP_DEFAULT                            137U

/* A_CAL_COMP: R/W calibration result; can restore a previously validated result. */
#define DRV2624_REG_A_CAL_COMP                              0x21U
#define DRV2624_A_CAL_COMP_RESET_DEFAULT                    0x0DU
#define DRV2624_A_CAL_COMP_RESERVED_MASK                    0x00U
#define DRV2624_A_CAL_COMP_WRITABLE_MASK                    0xFFU

/* A_CAL_COMP [7:0]
 * Hardware calibration output: compensation for actuator/driver resistive loss.
 * Treat as opaque calibration code, not voltage or a user strength setting. */
#define DRV2624_A_CAL_COMP_SHIFT                            0U
#define DRV2624_A_CAL_COMP_MASK                             0xFFU
#define DRV2624_A_CAL_COMP_DEFAULT                          13U

/* A_CAL_BEMF: R/W calibration result; can restore a previously validated result. */
#define DRV2624_REG_A_CAL_BEMF                              0x22U
#define DRV2624_A_CAL_BEMF_RESET_DEFAULT                    0x6DU
#define DRV2624_A_CAL_BEMF_RESERVED_MASK                    0x00U
#define DRV2624_A_CAL_BEMF_WRITABLE_MASK                    0xFFU

/* A_CAL_BEMF [7:0]
 * Hardware calibration output: back-EMF normalization for closed-loop gain.
 * Treat as opaque calibration code, not voltage or a user strength setting. */
#define DRV2624_A_CAL_BEMF_SHIFT                            0U
#define DRV2624_A_CAL_BEMF_MASK                             0xFFU
#define DRV2624_A_CAL_BEMF_DEFAULT                          109U

/* FEEDBACK_CONTROL: Noise gate and feedback gains; BEMF_GAIN is calibration output. */
#define DRV2624_REG_FEEDBACK_CONTROL                        0x23U
#define DRV2624_FEEDBACK_CONTROL_RESET_DEFAULT              0x36U
#define DRV2624_FEEDBACK_CONTROL_RESERVED_MASK              0x00U
#define DRV2624_FEEDBACK_CONTROL_WRITABLE_MASK              0xFFU

/* NG_THRESH [7]
 * Below selected magnitude, driver output becomes zero. */
#define DRV2624_NG_THRESH_SHIFT                             7U
#define DRV2624_NG_THRESH_MASK                              0x80U
#define DRV2624_NG_THRESH_DEFAULT                           0U
#define DRV2624_NG_THRESH_VDD_4_PERCENT                     0U
#define DRV2624_NG_THRESH_VDD_8_PERCENT                     1U

/* FB_BRAKE_FACTOR [6:4]
 * Brake/drive feedback gain ratio. Higher gain trades stability for braking
 * speed; set before calibration. Code 7 removes braking feedback. */
#define DRV2624_FB_BRAKE_FACTOR_SHIFT                       4U
#define DRV2624_FB_BRAKE_FACTOR_MASK                        0x70U
#define DRV2624_FB_BRAKE_FACTOR_DEFAULT                     3U
#define DRV2624_FB_BRAKE_FACTOR_GAIN_1                      0U
#define DRV2624_FB_BRAKE_FACTOR_GAIN_2                      1U
#define DRV2624_FB_BRAKE_FACTOR_GAIN_3                      2U
#define DRV2624_FB_BRAKE_FACTOR_GAIN_4                      3U
#define DRV2624_FB_BRAKE_FACTOR_GAIN_6                      4U
#define DRV2624_FB_BRAKE_FACTOR_GAIN_8                      5U
#define DRV2624_FB_BRAKE_FACTOR_GAIN_16                     6U
#define DRV2624_FB_BRAKE_FACTOR_BRAKING_DISABLED            7U

/* LOOP_GAIN [3:2]
 * Higher gain settles faster but can be less stable; set before calibration. */
#define DRV2624_LOOP_GAIN_SHIFT                             2U
#define DRV2624_LOOP_GAIN_MASK                              0x0CU
#define DRV2624_LOOP_GAIN_DEFAULT                           1U
#define DRV2624_LOOP_GAIN_VERY_SLOW                         0U
#define DRV2624_LOOP_GAIN_SLOW                              1U
#define DRV2624_LOOP_GAIN_FAST                              2U
#define DRV2624_LOOP_GAIN_VERY_FAST                         3U

/* BEMF_GAIN [1:0]
 * Auto-calibration selects this; host may overwrite. LRA gains =
 * 5,10,20,30; ERM gains = 0.34,1.05,1.82,4 for codes 0,1,2,3. */
#define DRV2624_BEMF_GAIN_SHIFT                             0U
#define DRV2624_BEMF_GAIN_MASK                              0x03U
#define DRV2624_BEMF_GAIN_DEFAULT                           2U
#define DRV2624_BEMF_GAIN_LRA_5X_ERM_034X                   0U
#define DRV2624_BEMF_GAIN_LRA_10X_ERM_105X                  1U
#define DRV2624_BEMF_GAIN_LRA_20X_ERM_182X                  2U
#define DRV2624_BEMF_GAIN_LRA_30X_ERM_4X                    3U

/* RATED_VOLTAGE_CLAMP: Output voltage clamp code. */
#define DRV2624_REG_RATED_VOLTAGE_CLAMP                     0x24U
#define DRV2624_RATED_VOLTAGE_CLAMP_RESET_DEFAULT           0x64U
#define DRV2624_RATED_VOLTAGE_CLAMP_RESERVED_MASK           0x00U
#define DRV2624_RATED_VOLTAGE_CLAMP_WRITABLE_MASK           0xFFU

/* RATED_VOLTAGE_CLAMP [7:0]
 * Steady-state clamp enforced after OD_CLAMP_TIME; lower OD_CLAMP or
 * active battery clamp takes priority.
 * Expected OD_CLAMP code domain (inferred from replacement/clamp role);
 * no independent conversion formula is specified for this register.
 * Battery thresholds/clamps require ordered levels; see BAT_LIFE_EXT_LVL1. */
#define DRV2624_RATED_VOLTAGE_CLAMP_SHIFT                   0U
#define DRV2624_RATED_VOLTAGE_CLAMP_MASK                    0xFFU
#define DRV2624_RATED_VOLTAGE_CLAMP_DEFAULT                 100U

/* OD_CLAMP_LVL1: Output voltage clamp code. */
#define DRV2624_REG_OD_CLAMP_LVL1                           0x25U
#define DRV2624_OD_CLAMP_LVL1_RESET_DEFAULT                 0x80U
#define DRV2624_OD_CLAMP_LVL1_RESERVED_MASK                 0x00U
#define DRV2624_OD_CLAMP_LVL1_WRITABLE_MASK                 0xFFU

/* OD_CLAMP_LVL1 [7:0]
 * Replaces OD_CLAMP below BAT_LIFE_EXT_LVL1; ignored in calibration/diagnostics.
 * Expected OD_CLAMP code domain (inferred from replacement/clamp role);
 * no independent conversion formula is specified for this register.
 * Battery thresholds/clamps require ordered levels; see BAT_LIFE_EXT_LVL1. */
#define DRV2624_OD_CLAMP_LVL1_SHIFT                         0U
#define DRV2624_OD_CLAMP_LVL1_MASK                          0xFFU
#define DRV2624_OD_CLAMP_LVL1_DEFAULT                       128U

/* OD_CLAMP_LVL2: Output voltage clamp code. */
#define DRV2624_REG_OD_CLAMP_LVL2                           0x26U
#define DRV2624_OD_CLAMP_LVL2_RESET_DEFAULT                 0x00U
#define DRV2624_OD_CLAMP_LVL2_RESERVED_MASK                 0x00U
#define DRV2624_OD_CLAMP_LVL2_WRITABLE_MASK                 0xFFU

/* OD_CLAMP_LVL2 [7:0]
 * Replaces OD_CLAMP and LVL1 below BAT_LIFE_EXT_LVL2; ignored in calibration/diagnostics.
 * Expected OD_CLAMP code domain (inferred from replacement/clamp role);
 * no independent conversion formula is specified for this register.
 * Battery thresholds/clamps require ordered levels; see BAT_LIFE_EXT_LVL1. */
#define DRV2624_OD_CLAMP_LVL2_SHIFT                         0U
#define DRV2624_OD_CLAMP_LVL2_MASK                          0xFFU
#define DRV2624_OD_CLAMP_LVL2_DEFAULT                       0U

/* LRA_DRIVE_CONTROL: Minimum tracking frequency, resynchronization and initial drive time. */
#define DRV2624_REG_LRA_DRIVE_CONTROL                       0x27U
#define DRV2624_LRA_DRIVE_CONTROL_RESET_DEFAULT             0x10U
#define DRV2624_LRA_DRIVE_CONTROL_RESERVED_MASK             0x20U
#define DRV2624_LRA_DRIVE_CONTROL_WRITABLE_MASK             0xDFU

/* LRA_MIN_FREQ_SEL [7]
 * Minimum supported LRA frequency. */
#define DRV2624_LRA_MIN_FREQ_SEL_SHIFT                      7U
#define DRV2624_LRA_MIN_FREQ_SEL_MASK                       0x80U
#define DRV2624_LRA_MIN_FREQ_SEL_DEFAULT                    0U
#define DRV2624_LRA_MIN_FREQ_SEL_125_HZ                     0U
#define DRV2624_LRA_MIN_FREQ_SEL_45_HZ                      1U

/* LRA_RESYNC_FORMAT [6]
 * Resynchronization method. */
#define DRV2624_LRA_RESYNC_FORMAT_SHIFT                     6U
#define DRV2624_LRA_RESYNC_FORMAT_MASK                      0x40U
#define DRV2624_LRA_RESYNC_FORMAT_DEFAULT                   0U
#define DRV2624_LRA_RESYNC_FORMAT_MIN_FREQ                  0U
#define DRV2624_LRA_RESYNC_FORMAT_DRIVE_TIME_125_PERCENT    1U

/* DRIVE_TIME [4:0]
 * Code 0..31: LRA drive_us = 500 + 100*code; ERM drive_us = 1000 + 200*code.
 * LRA initial half-period guess, adjusted during tracking; also no-BEMF free-run.
 * ERM sets BEMF sample rate; short periods need more supply headroom.
 * Eq. 1 no-BEMF frequency_Hz approximately 1000000/(2*drive_us - zc_us). */
#define DRV2624_DRIVE_TIME_SHIFT                            0U
#define DRV2624_DRIVE_TIME_MASK                             0x1FU
#define DRV2624_DRIVE_TIME_DEFAULT                          16U

/* BEMF_TIMING: Current decay and signal settling before BEMF sampling. */
#define DRV2624_REG_BEMF_TIMING                             0x28U
#define DRV2624_BEMF_TIMING_RESET_DEFAULT                   0x11U
#define DRV2624_BEMF_TIMING_RESERVED_MASK                   0x00U
#define DRV2624_BEMF_TIMING_WRITABLE_MASK                   0xFFU

/* BLANKING_TIME [7:4]
 * Wait for back-EMF to settle before sampling.
 * LRA supports all listed codes; ERM supports codes 0..3 only. */
#define DRV2624_BLANKING_TIME_SHIFT                         4U
#define DRV2624_BLANKING_TIME_MASK                          0xF0U
#define DRV2624_BLANKING_TIME_DEFAULT                       1U
#define DRV2624_BLANKING_TIME_LRA_15_US_ERM_45_US           0U
#define DRV2624_BLANKING_TIME_LRA_25_US_ERM_75_US           1U
#define DRV2624_BLANKING_TIME_LRA_50_US_ERM_150_US          2U
#define DRV2624_BLANKING_TIME_LRA_75_US_ERM_225_US          3U
#define DRV2624_BLANKING_TIME_LRA_90_US                     4U
#define DRV2624_BLANKING_TIME_LRA_105_US                    5U
#define DRV2624_BLANKING_TIME_LRA_120_US                    6U
#define DRV2624_BLANKING_TIME_LRA_135_US                    7U
#define DRV2624_BLANKING_TIME_LRA_150_US                    8U
#define DRV2624_BLANKING_TIME_LRA_165_US                    9U
#define DRV2624_BLANKING_TIME_LRA_180_US                    10U
#define DRV2624_BLANKING_TIME_LRA_195_US                    11U
#define DRV2624_BLANKING_TIME_LRA_210_US                    12U
#define DRV2624_BLANKING_TIME_LRA_235_US                    13U
#define DRV2624_BLANKING_TIME_LRA_260_US                    14U
#define DRV2624_BLANKING_TIME_LRA_285_US                    15U

/* IDISS_TIME [3:0]
 * Wait for actuator inductive current to dissipate before high-impedance sampling.
 * LRA supports all listed codes; ERM supports codes 0..3 only. */
#define DRV2624_IDISS_TIME_SHIFT                            0U
#define DRV2624_IDISS_TIME_MASK                             0x0FU
#define DRV2624_IDISS_TIME_DEFAULT                          1U
#define DRV2624_IDISS_TIME_LRA_15_US_ERM_45_US              0U
#define DRV2624_IDISS_TIME_LRA_25_US_ERM_75_US              1U
#define DRV2624_IDISS_TIME_LRA_50_US_ERM_150_US             2U
#define DRV2624_IDISS_TIME_LRA_75_US_ERM_225_US             3U
#define DRV2624_IDISS_TIME_LRA_90_US                        4U
#define DRV2624_IDISS_TIME_LRA_105_US                       5U
#define DRV2624_IDISS_TIME_LRA_120_US                       6U
#define DRV2624_IDISS_TIME_LRA_135_US                       7U
#define DRV2624_IDISS_TIME_LRA_150_US                       8U
#define DRV2624_IDISS_TIME_LRA_165_US                       9U
#define DRV2624_IDISS_TIME_LRA_180_US                       10U
#define DRV2624_IDISS_TIME_LRA_195_US                       11U
#define DRV2624_IDISS_TIME_LRA_210_US                       12U
#define DRV2624_IDISS_TIME_LRA_235_US                       13U
#define DRV2624_IDISS_TIME_LRA_260_US                       14U
#define DRV2624_IDISS_TIME_LRA_285_US                       15U

/* TIMING_CONTROL: Overdrive duration limit, BEMF sampling and zero-crossing timing. */
#define DRV2624_REG_TIMING_CONTROL                          0x29U
#define DRV2624_TIMING_CONTROL_RESET_DEFAULT                0x0CU
#define DRV2624_TIMING_CONTROL_RESERVED_MASK                0xC0U
#define DRV2624_TIMING_CONTROL_WRITABLE_MASK                0x3FU

/* OD_CLAMP_TIME [5:4]
 * Limit overshoot time during driving/braking; then use RATED_VOLTAGE_CLAMP.
 * May clamp away from a zero crossing; ignored in calibration/diagnostics. */
#define DRV2624_OD_CLAMP_TIME_SHIFT                         4U
#define DRV2624_OD_CLAMP_TIME_MASK                          0x30U
#define DRV2624_OD_CLAMP_TIME_DEFAULT                       0U
#define DRV2624_OD_CLAMP_TIME_AUTOMATIC                     0U
#define DRV2624_OD_CLAMP_TIME_25_MS                         1U
#define DRV2624_OD_CLAMP_TIME_50_MS                         2U
#define DRV2624_OD_CLAMP_TIME_100_MS                        3U

/* SAMPLE_TIME [3:2]
 * Wait around zero crossing before BEMF ADC amplitude sampling.
 * Changing this also changes effective BEMF gain; used in rated-voltage formula. */
#define DRV2624_SAMPLE_TIME_SHIFT                           2U
#define DRV2624_SAMPLE_TIME_MASK                            0x0CU
#define DRV2624_SAMPLE_TIME_DEFAULT                         3U
#define DRV2624_SAMPLE_TIME_150_US                          0U
#define DRV2624_SAMPLE_TIME_200_US                          1U
#define DRV2624_SAMPLE_TIME_250_US                          2U
#define DRV2624_SAMPLE_TIME_300_US                          3U

/* ZC_DET_TIME [1:0]
 * Zero-crossing detection window. */
#define DRV2624_ZC_DET_TIME_SHIFT                           0U
#define DRV2624_ZC_DET_TIME_MASK                            0x03U
#define DRV2624_ZC_DET_TIME_DEFAULT                         0U
#define DRV2624_ZC_DET_TIME_100_US                          0U
#define DRV2624_ZC_DET_TIME_200_US                          1U
#define DRV2624_ZC_DET_TIME_300_US                          2U
#define DRV2624_ZC_DET_TIME_390_US                          3U

/* AUTO_CAL_TIME: Calibration settling duration or explicit stop-trigger completion. */
#define DRV2624_REG_AUTO_CAL_TIME                           0x2AU
#define DRV2624_AUTO_CAL_TIME_RESET_DEFAULT                 0x02U
#define DRV2624_AUTO_CAL_TIME_RESERVED_MASK                 0xFCU
#define DRV2624_AUTO_CAL_TIME_WRITABLE_MASK                 0x03U

/* AUTO_CAL_TIME [1:0]
 * Trigger-controlled mode must run at least AUTO_CAL_TRIGGER_MIN_MS;
 * stop requests measurements, which take additional milliseconds. Early stop in
 * fixed-duration modes aborts calibration without a valid completed result. */
#define DRV2624_AUTO_CAL_TIME_SHIFT                         0U
#define DRV2624_AUTO_CAL_TIME_MASK                          0x03U
#define DRV2624_AUTO_CAL_TIME_DEFAULT                       2U
#define DRV2624_AUTO_CAL_TIME_250_MS                        0U
#define DRV2624_AUTO_CAL_TIME_500_MS                        1U
#define DRV2624_AUTO_CAL_TIME_1000_MS                       2U
#define DRV2624_AUTO_CAL_TIME_TRIGGER_CONTROLLED            3U

/* LRA_OPEN_LOOP_CONTROL: Loss-of-sync fallback and commanded open-loop waveform shape. */
#define DRV2624_REG_LRA_OPEN_LOOP_CONTROL                   0x2CU
#define DRV2624_LRA_OPEN_LOOP_CONTROL_RESET_DEFAULT         0x00U
#define DRV2624_LRA_OPEN_LOOP_CONTROL_RESERVED_MASK         0x1EU
#define DRV2624_LRA_OPEN_LOOP_CONTROL_WRITABLE_MASK         0xE1U

/* LRA_AUTO_OPEN_LOOP [7]
 * 0 normal resynchronization behavior, 1 switch to open loop after failed
 * zero crossings; no resynchronization. Fallback always square, ignores LRA_WAVE_SHAPE. */
#define DRV2624_LRA_AUTO_OPEN_LOOP_SHIFT                    7U
#define DRV2624_LRA_AUTO_OPEN_LOOP_MASK                     0x80U
#define DRV2624_LRA_AUTO_OPEN_LOOP_DEFAULT                  0U

/* AUTO_OL_CNT [6:5]
 * Failed synchronization attempts before automatic open-loop fallback. */
#define DRV2624_AUTO_OL_CNT_SHIFT                           5U
#define DRV2624_AUTO_OL_CNT_MASK                            0x60U
#define DRV2624_AUTO_OL_CNT_DEFAULT                         0U
#define DRV2624_AUTO_OL_CNT_3_ATTEMPTS                      0U
#define DRV2624_AUTO_OL_CNT_4_ATTEMPTS                      1U
#define DRV2624_AUTO_OL_CNT_5_ATTEMPTS                      2U
#define DRV2624_AUTO_OL_CNT_6_ATTEMPTS                      3U

/* LRA_WAVE_SHAPE [0]
 * Commanded open-loop LRA drive only; ignored for ERM, closed loop and
 * automatic fallback. Changing during a process aborts it. */
#define DRV2624_LRA_WAVE_SHAPE_SHIFT                        0U
#define DRV2624_LRA_WAVE_SHAPE_MASK                         0x01U
#define DRV2624_LRA_WAVE_SHAPE_DEFAULT                      0U
#define DRV2624_LRA_WAVE_SHAPE_SQUARE                       0U
#define DRV2624_LRA_WAVE_SHAPE_SINE                         1U

/* OL_LRA_PERIOD_H: Open-loop LRA period high bits. */
#define DRV2624_REG_OL_LRA_PERIOD_H                         0x2EU
#define DRV2624_OL_LRA_PERIOD_H_RESET_DEFAULT               0x00U
#define DRV2624_OL_LRA_PERIOD_H_RESERVED_MASK               0xFCU
#define DRV2624_OL_LRA_PERIOD_H_WRITABLE_MASK               0x03U

/* OL_LRA_PERIOD_H [1:0]
 * Bits 9:8 of combined OL_LRA_PERIOD; configure while idle. */
#define DRV2624_OL_LRA_PERIOD_H_SHIFT                       0U
#define DRV2624_OL_LRA_PERIOD_H_MASK                        0x03U
#define DRV2624_OL_LRA_PERIOD_H_DEFAULT                     0U

/* OL_LRA_PERIOD_L: Open-loop LRA period low bits. */
#define DRV2624_REG_OL_LRA_PERIOD_L                         0x2FU
#define DRV2624_OL_LRA_PERIOD_L_RESET_DEFAULT               0xC6U
#define DRV2624_OL_LRA_PERIOD_L_RESERVED_MASK               0x00U
#define DRV2624_OL_LRA_PERIOD_L_WRITABLE_MASK               0xFFU

/* OL_LRA_PERIOD_L [7:0]
 * Combined reset code 198. Period_ns = code * 24615 (section 8.44).
 * frequency_mHz = 1000000000000 / period_ns with 64-bit math; reject zero.
 * Section 7.3.2.3 has a conflicting old 7-bit equation; use register definition. */
#define DRV2624_OL_LRA_PERIOD_L_SHIFT                       0U
#define DRV2624_OL_LRA_PERIOD_L_MASK                        0xFFU
#define DRV2624_OL_LRA_PERIOD_L_DEFAULT                     198U

/* CURRENT_K: Resistance coefficient (R), used with DIAG_Z_RESULT. */
#define DRV2624_REG_CURRENT_K                               0x30U
#define DRV2624_CURRENT_K_RESET_DEFAULT                     0x00U
#define DRV2624_CURRENT_K_RESERVED_MASK                     0x00U
#define DRV2624_CURRENT_K_WRITABLE_MASK                     0x00U

/* CURRENT_K [7:0]
 * R; denominator coefficient in DIAG_Z_RESULT resistance formula, not current ADC. */
#define DRV2624_CURRENT_K_SHIFT                             0U
#define DRV2624_CURRENT_K_MASK                              0xFFU
#define DRV2624_CURRENT_K_DEFAULT                           0U

/* RAM_ADDR_H: Internal waveform RAM access (R/W). */
#define DRV2624_REG_RAM_ADDR_H                              0xFDU
#define DRV2624_RAM_ADDR_H_RESET_DEFAULT                    0x00U
#define DRV2624_RAM_ADDR_H_RESERVED_MASK                    0x00U
#define DRV2624_RAM_ADDR_H_WRITABLE_MASK                    0xFFU

/* RAM_ADDR_H [7:0]
 * RAM address bits 15:8.
 * Load RAM_ADDR high then low before accessing RAM_DATA. */
#define DRV2624_RAM_ADDR_H_SHIFT                            0U
#define DRV2624_RAM_ADDR_H_MASK                             0xFFU
#define DRV2624_RAM_ADDR_H_DEFAULT                          0U

/* RAM_ADDR_L: Internal waveform RAM access (R/W). */
#define DRV2624_REG_RAM_ADDR_L                              0xFEU
#define DRV2624_RAM_ADDR_L_RESET_DEFAULT                    0x00U
#define DRV2624_RAM_ADDR_L_RESERVED_MASK                    0x00U
#define DRV2624_RAM_ADDR_L_WRITABLE_MASK                    0xFFU

/* RAM_ADDR_L [7:0]
 * RAM address bits 7:0.
 * Load RAM_ADDR high then low before accessing RAM_DATA. */
#define DRV2624_RAM_ADDR_L_SHIFT                            0U
#define DRV2624_RAM_ADDR_L_MASK                             0xFFU
#define DRV2624_RAM_ADDR_L_DEFAULT                          0U

/* RAM_DATA: Internal waveform RAM access (R/W). */
#define DRV2624_REG_RAM_DATA                                0xFFU
#define DRV2624_RAM_DATA_RESET_DEFAULT                      0x00U
#define DRV2624_RAM_DATA_RESERVED_MASK                      0x00U
#define DRV2624_RAM_DATA_WRITABLE_MASK                      0xFFU

/* RAM_DATA [7:0]
 * Data port at RAM_ADDR; each read/write increments RAM address,
 * including burst transfers. I2C register remains RAM_DATA for streamed RAM access.
 * Load RAM_ADDR high then low before accessing RAM_DATA. */
#define DRV2624_RAM_DATA_SHIFT                              0U
#define DRV2624_RAM_DATA_MASK                               0xFFU
#define DRV2624_RAM_DATA_DEFAULT                            0U

/* Programming scales/limits; equations above are reference notation, not C
 * floating-point expressions. Implement future conversions using integer/fixed
 * point with wide intermediates, explicit rounding and code/range validation.
 * No motor-specific values or chosen operating profile are defined here.
 */
#define DRV2624_VBAT_FULL_SCALE_MV                          5600U
#define DRV2624_VBAT_FULL_SCALE_CODE                        255U
#define DRV2624_DIAG_RESISTANCE_SCALE_MOHM                  478430U
#define DRV2624_DIAG_RESISTANCE_DEN_BASE                    719U
#define DRV2624_DIAG_RESISTANCE_DEN_STEP                    4U
#define DRV2624_LRA_PERIOD_TICK_NS                          24390U
#define DRV2624_OL_LRA_PERIOD_TICK_NS                       24615U
#define DRV2624_PERIOD_CODE_MAX                             1023U
#define DRV2624_OL_LRA_PERIOD_DEFAULT                       198U
#define DRV2624_DRIVE_TIME_CODE_MAX                         31U
#define DRV2624_LRA_DRIVE_TIME_BASE_US                      500U
#define DRV2624_LRA_DRIVE_TIME_STEP_US                      100U
#define DRV2624_ERM_DRIVE_TIME_BASE_US                      1000U
#define DRV2624_ERM_DRIVE_TIME_STEP_US                      200U
#define DRV2624_UVLO_BASE_MV                                2500U
#define DRV2624_UVLO_STEP_MV                                100U
#define DRV2624_PLAYBACK_TICK_SLOW_MS                       5U
#define DRV2624_PLAYBACK_TICK_FAST_MS                       1U
#define DRV2624_SEQUENCE_SLOT_COUNT                         8U
#define DRV2624_SEQUENCE_WAIT_TICK_MS                       10U
#define DRV2624_WAVEFORM_ID_MIN                             1U
#define DRV2624_WAVEFORM_ID_MAX                             127U
#define DRV2624_WAVEFORM_END                                0U
#define DRV2624_AUTO_CAL_TRIGGER_MIN_MS                     1000U
#define DRV2624_POWER_UP_WAIT_MS                            1U
#define DRV2624_TRIGGER_PULSE_MIN_US                        1U
#define DRV2624_AUTO_BRAKE_TICK_COUNT                       10U
#define DRV2624_AUTO_BRAKE_BUFFER_MS                        1U
#define DRV2624_I2C_WATCHDOG_US                             4330U
#define DRV2624_I2C_MAX_HZ                                  400000U

/* RAM library format (sections 7.6.9.2..7.6.9.3).
 * Revision byte, then 3-byte header entries: start address H, L, config.
 * Effect ID n header starts at 1 + 3*(n-1). No library effect-count field.
 * Config: repeats in bits 7:5, size in bits 4:0. Size counts data bytes and
 * must be even. Repeats 0..6 = 1..7 plays, 7 = infinite until stopped.
 * Data: amplitude byte, duration byte, repeated. Amplitude bits 6:0 are
 * signed two's complement; bit 7 ramps to NEXT point over this point's time.
 * Final ramp needs a following point. Time uses physical PLAYBACK_INTERVAL.
 * No unsigned-format selector is documented, despite prose mentioning it.
 * Datasheet repeatedly specifies 1KiB; Figure 7-14 instead ends at 0x7FF.
 * RAM_BYTES follows stated capacity; do not assume the figure proves 2KiB.
 */
#define DRV2624_RAM_BYTES                                   1024U
#define DRV2624_RAM_LAST_ADDRESS                            0x03FFU
#define DRV2624_RAM_LIBRARY_REVISION                        0U
#define DRV2624_RAM_REVISION_ADDRESS                        0U
#define DRV2624_RAM_HEADER_START                            1U
#define DRV2624_RAM_HEADER_ENTRY_BYTES                      3U
#define DRV2624_RAM_EFFECT_SIZE_SHIFT                       0U
#define DRV2624_RAM_EFFECT_SIZE_MASK                        0x1FU
#define DRV2624_RAM_EFFECT_SIZE_MIN                         2U
#define DRV2624_RAM_EFFECT_SIZE_MAX                         30U
#define DRV2624_RAM_WAVEFORM_REPEATS_SHIFT                  5U
#define DRV2624_RAM_WAVEFORM_REPEATS_MASK                   0xE0U
#define DRV2624_RAM_WAVEFORM_REPEATS_NONE                   0U
#define DRV2624_RAM_WAVEFORM_REPEATS_INFINITE               7U
#define DRV2624_RAM_RAMP_SHIFT                              7U
#define DRV2624_RAM_RAMP_MASK                               0x80U
#define DRV2624_RAM_RAMP_DISABLED                           0U
#define DRV2624_RAM_RAMP_ENABLED                            1U
#define DRV2624_RAM_AMPLITUDE_SHIFT                         0U
#define DRV2624_RAM_AMPLITUDE_MASK                          0x7FU
#define DRV2624_RAM_AMPLITUDE_MIN                           (-63)
#define DRV2624_RAM_AMPLITUDE_MAX                           63
#define DRV2624_RAM_TIME_MAX_TICKS                          255U
#define DRV2624_TIME_OFFSET_MIN                             (-128)
#define DRV2624_TIME_OFFSET_MAX                             127

#include "i2c_device.h"
/* Initialize once before starting; Update is foreground, callbacks are ISR.
 * Requests reject unavailable/busy/unacknowledged results and invalid masks.
 * Config preserves reserved bits and compares only requested fields. Caller
 * validates field encodings. Ordinary config rejects registers containing
 * autonomously updated fields regardless of the targeted mask. Exhaustion latches until reboot/init.
 * Commands are separate: ACCEPTED means write ACK and an observation, never
 * proof of side-effect completion. UNCERTAIN requires caller reconciliation;
 * automatic retries cannot replay a possibly transmitted command write.
 * GetResult is retained; Acknowledge permits the next request only after all
 * callback/buffer ownership has ended. recovering reports retained ownership;
 * exhaustion can become terminal before recovery drains. Keep calling Update.
 * Cancel polls/drains through Update.
 */
void DRV2624_Init(I2C_HandleTypeDef *handle, const I2C_DeviceOps *ops);
bool DRV2624_RequestRead(uint8_t reg);
bool DRV2624_RequestConfig(uint8_t reg, uint8_t mask, uint8_t value);
/* Caller asserts an idle ownership lease for the ENTIRE addressed register,
 * from prerequisite read through verification (including retries), or through
 * cancellation/abort drain. Hold hardware autonomous activity stopped until
 * result is terminal AND recovering=false; do not infer release from a terminal
 * command/health result alone. Every write of these registers needs the lease:
 * A_CAL_COMP, A_CAL_BEMF, FEEDBACK_CONTROL: calibration must be quiescent,
 * including gain changes even when only NG_THRESH or other fields are targeted.
 * Unqualified ordinary RequestConfig rejects these registers without transfer.
 * This API asserts caller ownership; it does not stop hardware autonomously. */
bool DRV2624_RequestConfigIdle(uint8_t reg, uint8_t mask, uint8_t value);
bool DRV2624_RequestCommand(uint8_t reg, uint8_t mask, uint8_t value);
void DRV2624_Update(void);
I2C_DeviceResult DRV2624_GetResult(void);
bool DRV2624_Acknowledge(void);
void DRV2624_Cancel(void);
void DRV2624_OnReadComplete(I2C_HandleTypeDef *handle);
void DRV2624_OnWriteComplete(I2C_HandleTypeDef *handle);
void DRV2624_OnError(I2C_HandleTypeDef *handle);

/* STATUS is read-to-clear: RequestRead(STATUS) rejects; only this acquisition
 * path reads it. Consumers share flags ORed since status acknowledgment and
 * an observation sequence. A request after acknowledging the operation result
 * begins a new observation. */
typedef struct { bool valid; uint8_t value; uint32_t sequence; } DRV2624_StatusSnapshot;
bool DRV2624_RequestStatus(void);
DRV2624_StatusSnapshot DRV2624_GetStatus(void);
/* Coordinator acknowledges only after every consumer processed this sequence.
 * Clears retained flags only if no newer observation arrived. */
bool DRV2624_AcknowledgeStatus(uint32_t sequence);

#endif /* USER_DRV2624_H */
