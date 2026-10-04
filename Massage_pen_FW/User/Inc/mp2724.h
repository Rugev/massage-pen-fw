/* MP2724GRH programming reference: MP2724 Rev. 1.0, 2024-04-29, pp28-41.
 * Register names follow the datasheet. All field value/default macros are
 * UNSHIFTED codes; use MP2724_FIELD_PREP/GET with the field MASK and SHIFT.
 * Range constants describe programmable setpoints, not electrical tolerances.
 * Never write undocumented addresses/encodings. Preserve reserved bits and
 * suppress COMMAND_MASK bits during unrelated read-modify-write operations.
 * Defaults are factory option 0000; OTP variants can have different defaults.
 * Configuration fields below are R/W; STATUS fields and reserved bits are R/O.
 * No HAL transport, startup configuration or charging policy is implemented.
 */
#ifndef USER_MP2724_H
#define USER_MP2724_H

#include <stdint.h>

/* Unshifted slave address; STM32 HAL expects this shifted left by one. */
#define MP2724_I2C_ADDRESS_7BIT                 0x3FU
#define MP2724_REGISTER_COUNT                  22U
/* The gap in the public map has no documented command; do not access it. */
#define MP2724_REG_UNDOCUMENTED_0B              0x0BU

/* These macros mask/shift only; callers must validate codes and physical ranges.
 * Each argument is evaluated once. Replace a field with:
 * (old & ~MASK) | MP2724_FIELD_PREP(MASK, SHIFT, code)
 * Clear unrelated command bits before writing the resulting byte.
 */
#define MP2724_FIELD_PREP(mask, shift, code) \
    ((((uint32_t)(code)) << (shift)) & (uint32_t)(mask))
#define MP2724_FIELD_GET(mask, shift, byte) \
    ((((uint32_t)(byte)) & (uint32_t)(mask)) >> (shift))

/* CHG_CTRL0: Pin functions, parameter lock, switching frequency and input tracking. */
#define MP2724_REG_CHG_CTRL0                      0x00U
#define MP2724_CHG_CTRL0_RESERVED_MASK            0x00U
#define MP2724_CHG_CTRL0_WRITABLE_MASK            0xFFU
#define MP2724_CHG_CTRL0_COMMAND_MASK             0x80U
#define MP2724_CHG_CTRL0_RESET_DEFAULT            0x0BU

/* REG_RST [7], R/W; watchdog reset: no.
 * 0: keep settings; 1: reset all registers to defaults; self-clears.
 */
#define MP2724_REG_RST_SHIFT                      7U
#define MP2724_REG_RST_MASK                       0x80U
#define MP2724_REG_RST_DEFAULT                    0U

/* EN_STAT_IB [6], R/W; watchdog reset: no.
 * 0: open-drain charging status; 1: analog battery-current output.
 */
#define MP2724_EN_STAT_IB_SHIFT                   6U
#define MP2724_EN_STAT_IB_MASK                    0x40U
#define MP2724_EN_STAT_IB_DEFAULT                 0U
#define MP2724_EN_STAT_IB_STAT                    0U
#define MP2724_EN_STAT_IB_IB                      1U

/* EN_PG_NTC2 [5], R/W; watchdog reset: no.
 * 0: open-drain power-good output; 1: second NTC input.
 */
#define MP2724_EN_PG_NTC2_SHIFT                   5U
#define MP2724_EN_PG_NTC2_MASK                    0x20U
#define MP2724_EN_PG_NTC2_DEFAULT                 0U
#define MP2724_EN_PG_NTC2_PG                      0U
#define MP2724_EN_PG_NTC2_NTC2                    1U

/* LOCK_CHG [4], R/W; watchdog reset: no.
 * 0: unlocked; 1: only reductions allowed in VBATT, ICC, IPRE, JEITA_VSET and JEITA_ISET. Clear to unlock.
 */
#define MP2724_LOCK_CHG_SHIFT                     4U
#define MP2724_LOCK_CHG_MASK                      0x10U
#define MP2724_LOCK_CHG_DEFAULT                   0U

/* HOLDOFF_TMR [3], R/W; watchdog reset: yes.
 * 0: bypass input detection hold-off; 1: enable hold-off.
 */
#define MP2724_HOLDOFF_TMR_SHIFT                  3U
#define MP2724_HOLDOFF_TMR_MASK                   0x08U
#define MP2724_HOLDOFF_TMR_DEFAULT                1U

/* SW_FREQ [2:1], R/W; watchdog reset: no.
 * Frequency for both buck and boost.
 */
#define MP2724_SW_FREQ_SHIFT                      1U
#define MP2724_SW_FREQ_MASK                       0x06U
#define MP2724_SW_FREQ_DEFAULT                    1U
#define MP2724_SW_FREQ_750_KHZ                    0U
#define MP2724_SW_FREQ_1000_KHZ                   1U
#define MP2724_SW_FREQ_1250_KHZ                   2U
#define MP2724_SW_FREQ_1500_KHZ                   3U

/* EN_VIN_TRK [0], R/W; watchdog reset: no.
 * 0: fixed VIN_LIM; 1: max(VIN_LIM, battery voltage + 165mV).
 */
#define MP2724_EN_VIN_TRK_SHIFT                   0U
#define MP2724_EN_VIN_TRK_MASK                    0x01U
#define MP2724_EN_VIN_TRK_DEFAULT                 1U

/* IIN: Input current limit; detection may update IIN_LIM. */
#define MP2724_REG_IIN                            0x01U
#define MP2724_IIN_RESERVED_MASK                  0x00U
#define MP2724_IIN_WRITABLE_MASK                  0xFFU
#define MP2724_IIN_COMMAND_MASK                   0x00U
#define MP2724_IIN_RESET_DEFAULT                  0x04U

/* IIN_MODE [7:5], R/W; watchdog reset: no.
 * Follow IIN_LIM or force a fixed limit regardless of detection. Code 7 is undocumented.
 */
#define MP2724_IIN_MODE_SHIFT                     5U
#define MP2724_IIN_MODE_MASK                      0xE0U
#define MP2724_IIN_MODE_DEFAULT                   0U
#define MP2724_IIN_MODE_FOLLOW_IIN_LIM            0U
#define MP2724_IIN_MODE_100_MA                    1U
#define MP2724_IIN_MODE_500_MA                    2U
#define MP2724_IIN_MODE_900_MA                    3U
#define MP2724_IIN_MODE_1500_MA                   4U
#define MP2724_IIN_MODE_2000_MA                   5U
#define MP2724_IIN_MODE_3000_MA                   6U

/* IIN_LIM [4:0], R/W; watchdog reset: no.
 * 100 + 100 * code mA; codes 0..31. Detection overwrites this; host may override after VIN_RDY.
 */
#define MP2724_IIN_LIM_SHIFT                      0U
#define MP2724_IIN_LIM_MASK                       0x1FU
#define MP2724_IIN_LIM_DEFAULT                    4U

/* CHG_PARAMETER0: Pre-charge threshold and fast-charge current. */
#define MP2724_REG_CHG_PARAMETER0                 0x02U
#define MP2724_CHG_PARAMETER0_RESERVED_MASK       0x00U
#define MP2724_CHG_PARAMETER0_WRITABLE_MASK       0xFFU
#define MP2724_CHG_PARAMETER0_COMMAND_MASK        0x00U
#define MP2724_CHG_PARAMETER0_RESET_DEFAULT       0xC1U

/* VPRE [7:6], R/W; watchdog reset: no.
 * Pre-charge to fast-charge battery voltage threshold.
 */
#define MP2724_VPRE_SHIFT                         6U
#define MP2724_VPRE_MASK                          0xC0U
#define MP2724_VPRE_DEFAULT                       3U
#define MP2724_VPRE_2600_MV                       0U
#define MP2724_VPRE_2800_MV                       1U
#define MP2724_VPRE_3000_MV                       2U
#define MP2724_VPRE_3200_MV                       3U

/* ICC [5:0], R/W; watchdog reset: yes.
 * 40 * code mA; codes 0..55. Never program codes 56..63 (above 2200mA). Subject to LOCK_CHG.
 */
#define MP2724_ICC_SHIFT                          0U
#define MP2724_ICC_MASK                           0x3FU
#define MP2724_ICC_DEFAULT                        1U

/* CHG_PARAMETER1: Pre-charge and termination currents. */
#define MP2724_REG_CHG_PARAMETER1                 0x03U
#define MP2724_CHG_PARAMETER1_RESERVED_MASK       0x00U
#define MP2724_CHG_PARAMETER1_WRITABLE_MASK       0xFFU
#define MP2724_CHG_PARAMETER1_COMMAND_MASK        0x00U
#define MP2724_CHG_PARAMETER1_RESET_DEFAULT       0x11U

/* IPRE [7:4], R/W; watchdog reset: yes.
 * 20 * code mA; codes 0..15. Subject to LOCK_CHG.
 */
#define MP2724_IPRE_SHIFT                         4U
#define MP2724_IPRE_MASK                          0xF0U
#define MP2724_IPRE_DEFAULT                       1U

/* ITERM [3:0], R/W; watchdog reset: yes.
 * 15 + 15 * code mA; codes 0..15. Used only with EN_TERM.
 */
#define MP2724_ITERM_SHIFT                        0U
#define MP2724_ITERM_MASK                         0x0FU
#define MP2724_ITERM_DEFAULT                      1U

/* CHG_PARAMETER2: Recharge offset, trickle current and input voltage limit. */
#define MP2724_REG_CHG_PARAMETER2                 0x04U
#define MP2724_CHG_PARAMETER2_RESERVED_MASK       0x00U
#define MP2724_CHG_PARAMETER2_WRITABLE_MASK       0xFFU
#define MP2724_CHG_PARAMETER2_COMMAND_MASK        0x00U
#define MP2724_CHG_PARAMETER2_RESET_DEFAULT       0x16U

/* VRECHG [7], R/W; watchdog reset: yes.
 * Recharge starts below battery regulation target minus this offset.
 */
#define MP2724_VRECHG_SHIFT                       7U
#define MP2724_VRECHG_MASK                        0x80U
#define MP2724_VRECHG_DEFAULT                     0U
#define MP2724_VRECHG_100_MV                      0U
#define MP2724_VRECHG_200_MV                      1U

/* ITRICKLE [6:4], R/W; watchdog reset: yes.
 * 16 * code mA; codes 0..7.
 */
#define MP2724_ITRICKLE_SHIFT                     4U
#define MP2724_ITRICKLE_MASK                      0x70U
#define MP2724_ITRICKLE_DEFAULT                   1U

/* VIN_LIM [3:0], R/W; watchdog reset: no.
 * 3880 + 80 * code mV; codes 0..15. EN_VIN_TRK may raise the effective limit.
 */
#define MP2724_VIN_LIM_SHIFT                      0U
#define MP2724_VIN_LIM_MASK                       0x0FU
#define MP2724_VIN_LIM_DEFAULT                    6U

/* CHG_PARAMETER3: Battery regulation target and post-termination top-off timer. */
#define MP2724_REG_CHG_PARAMETER3                 0x05U
#define MP2724_CHG_PARAMETER3_RESERVED_MASK       0x00U
#define MP2724_CHG_PARAMETER3_WRITABLE_MASK       0xFFU
#define MP2724_CHG_PARAMETER3_COMMAND_MASK        0x00U
#define MP2724_CHG_PARAMETER3_RESET_DEFAULT       0x18U

/* TOPOFF_TMR [7:6], R/W; watchdog reset: yes.
 * Charging continues during top-off although CHG_STAT reports done.
 */
#define MP2724_TOPOFF_TMR_SHIFT                   6U
#define MP2724_TOPOFF_TMR_MASK                    0xC0U
#define MP2724_TOPOFF_TMR_DEFAULT                 0U
#define MP2724_TOPOFF_TMR_DISABLED                0U
#define MP2724_TOPOFF_TMR_15_MIN                  1U
#define MP2724_TOPOFF_TMR_30_MIN                  2U
#define MP2724_TOPOFF_TMR_45_MIN                  3U

/* VBATT [5:0], R/W; watchdog reset: no.
 * 3600 + 25 * code mV; codes 0..40. Higher codes clamp to code 40 (4600mV). Subject to LOCK_CHG.
 */
#define MP2724_VBATT_SHIFT                        0U
#define MP2724_VBATT_MASK                         0x3FU
#define MP2724_VBATT_DEFAULT                      24U

/* CHG_CTRL1: Minimum system voltage and die thermal regulation threshold. */
#define MP2724_REG_CHG_CTRL1                      0x06U
#define MP2724_CHG_CTRL1_RESERVED_MASK            0xC0U
#define MP2724_CHG_CTRL1_WRITABLE_MASK            0x3FU
#define MP2724_CHG_CTRL1_COMMAND_MASK             0x00U
#define MP2724_CHG_CTRL1_RESET_DEFAULT            0x24U

/* SYS_MIN [5:3], R/W; watchdog reset: no.
 * System minimum setting; actual regulated SYS includes VTRACK. Code 7 is undocumented.
 */
#define MP2724_SYS_MIN_SHIFT                      3U
#define MP2724_SYS_MIN_MASK                       0x38U
#define MP2724_SYS_MIN_DEFAULT                    4U
#define MP2724_SYS_MIN_2975_MV                    0U
#define MP2724_SYS_MIN_3150_MV                    1U
#define MP2724_SYS_MIN_3325_MV                    2U
#define MP2724_SYS_MIN_3500_MV                    3U
#define MP2724_SYS_MIN_3588_MV                    4U
#define MP2724_SYS_MIN_3675_MV                    5U
#define MP2724_SYS_MIN_3763_MV                    6U

/* TREG [2:0], R/W; watchdog reset: yes.
 * Charge-mode thermal regulation / boost-mode thermal protection. Code 7 is undocumented.
 */
#define MP2724_TREG_SHIFT                         0U
#define MP2724_TREG_MASK                          0x07U
#define MP2724_TREG_DEFAULT                       4U
#define MP2724_TREG_60_C                          0U
#define MP2724_TREG_70_C                          1U
#define MP2724_TREG_80_C                          2U
#define MP2724_TREG_90_C                          3U
#define MP2724_TREG_100_C                         4U
#define MP2724_TREG_110_C                         5U
#define MP2724_TREG_120_C                         6U

/* CHG_CTRL2: Current-output gating, watchdog, termination and safety timers. */
#define MP2724_REG_CHG_CTRL2                      0x07U
#define MP2724_CHG_CTRL2_RESERVED_MASK            0x00U
#define MP2724_CHG_CTRL2_WRITABLE_MASK            0xFFU
#define MP2724_CHG_CTRL2_COMMAND_MASK             0x40U
#define MP2724_CHG_CTRL2_RESET_DEFAULT            0x1EU

/* IB_EN [7], R/W; watchdog reset: yes.
 * 0: IB active only while switching; 1: IB also active on battery-only power. Requires IB pin mode.
 */
#define MP2724_IB_EN_SHIFT                        7U
#define MP2724_IB_EN_MASK                         0x80U
#define MP2724_IB_EN_DEFAULT                      0U

/* WATCHDOG_RST [6], R/W; watchdog reset: no.
 * 0: no action; write 1 to start/restart watchdog. Do not replay in read-modify-write.
 */
#define MP2724_WATCHDOG_RST_SHIFT                 6U
#define MP2724_WATCHDOG_RST_MASK                  0x40U
#define MP2724_WATCHDOG_RST_DEFAULT               0U

/* WATCHDOG [5:4], R/W; watchdog reset: yes.
 * Watchdog timeout selection.
 */
#define MP2724_WATCHDOG_SHIFT                     4U
#define MP2724_WATCHDOG_MASK                      0x30U
#define MP2724_WATCHDOG_DEFAULT                   1U
#define MP2724_WATCHDOG_DISABLED                  0U
#define MP2724_WATCHDOG_40_S                      1U
#define MP2724_WATCHDOG_80_S                      2U
#define MP2724_WATCHDOG_160_S                     3U

/* EN_TERM [3], R/W; watchdog reset: yes.
 * 0: disable charge termination; 1: enable.
 */
#define MP2724_EN_TERM_SHIFT                      3U
#define MP2724_EN_TERM_MASK                       0x08U
#define MP2724_EN_TERM_DEFAULT                    1U

/* EN_TMR2X [2], R/W; watchdog reset: yes.
 * 0: normal safety-timer rate; 1: half rate under input/thermal regulation or JEITA current reduction.
 */
#define MP2724_EN_TMR2X_SHIFT                     2U
#define MP2724_EN_TMR2X_MASK                      0x04U
#define MP2724_EN_TMR2X_DEFAULT                   1U

/* CHG_TIMER [1:0], R/W; watchdog reset: yes.
 * Fast-charge safety timer; disabling also disables the fixed pre-charge safety timer.
 */
#define MP2724_CHG_TIMER_SHIFT                    0U
#define MP2724_CHG_TIMER_MASK                     0x03U
#define MP2724_CHG_TIMER_DEFAULT                  2U
#define MP2724_CHG_TIMER_DISABLED                 0U
#define MP2724_CHG_TIMER_5_H                      1U
#define MP2724_CHG_TIMER_10_H                     2U
#define MP2724_CHG_TIMER_15_H                     3U

/* CHG_CTRL3: BATTFET shipping/reset behavior and boost settings. */
#define MP2724_REG_CHG_CTRL3                      0x08U
#define MP2724_CHG_CTRL3_RESERVED_MASK            0x00U
#define MP2724_CHG_CTRL3_WRITABLE_MASK            0xFFU
#define MP2724_CHG_CTRL3_COMMAND_MASK             0x00U
#define MP2724_CHG_CTRL3_RESET_DEFAULT            0x7FU

/* BATTFET_DIS [7], R/W; watchdog reset: no.
 * 0: allow BATTFET on; 1: turn off (shipping mode). Reads status; battery discharge OCP also sets this.
 */
#define MP2724_BATTFET_DIS_SHIFT                  7U
#define MP2724_BATTFET_DIS_MASK                   0x80U
#define MP2724_BATTFET_DIS_DEFAULT                0U

/* BATTFET_DLY [6], R/W; watchdog reset: no.
 * 0: immediate BATTFET disable; 1: delayed disable. Table says 10s; timing table gives 12s typical.
 */
#define MP2724_BATTFET_DLY_SHIFT                  6U
#define MP2724_BATTFET_DLY_MASK                   0x40U
#define MP2724_BATTFET_DLY_DEFAULT                1U
#define MP2724_BATTFET_DLY_IMMEDIATE              0U
#define MP2724_BATTFET_DLY_DELAYED                1U

/* BATTFET_RST_EN [5], R/W; watchdog reset: yes.
 * 0: disable RST system-power reset; 1: enable reset on battery-only power.
 */
#define MP2724_BATTFET_RST_EN_SHIFT               5U
#define MP2724_BATTFET_RST_EN_MASK                0x20U
#define MP2724_BATTFET_RST_EN_DEFAULT             1U

/* OLIM [4:3], R/W; watchdog reset: yes.
 * Boost output current limit.
 */
#define MP2724_OLIM_SHIFT                         3U
#define MP2724_OLIM_MASK                          0x18U
#define MP2724_OLIM_DEFAULT                       3U
#define MP2724_OLIM_500_MA                        0U
#define MP2724_OLIM_1500_MA                       1U
#define MP2724_OLIM_2100_MA                       2U
#define MP2724_OLIM_3000_MA                       3U

/* VBOOST [2:0], R/W; watchdog reset: no.
 * Boost voltage encoding is non-monotonic.
 */
#define MP2724_VBOOST_SHIFT                       0U
#define MP2724_VBOOST_MASK                        0x07U
#define MP2724_VBOOST_DEFAULT                     7U
#define MP2724_VBOOST_5200_MV                     0U
#define MP2724_VBOOST_5250_MV                     1U
#define MP2724_VBOOST_5300_MV                     2U
#define MP2724_VBOOST_5350_MV                     3U
#define MP2724_VBOOST_5000_MV                     4U
#define MP2724_VBOOST_5050_MV                     5U
#define MP2724_VBOOST_5100_MV                     6U
#define MP2724_VBOOST_5150_MV                     7U

/* CHG_CTRL4: USB-C sink configuration and converter/charging enables. */
#define MP2724_REG_CHG_CTRL4                      0x09U
#define MP2724_CHG_CTRL4_RESERVED_MASK            0x88U
#define MP2724_CHG_CTRL4_WRITABLE_MASK            0x77U
#define MP2724_CHG_CTRL4_COMMAND_MASK             0x00U
#define MP2724_CHG_CTRL4_RESET_DEFAULT            0x53U

/* CC_CFG [6:4], R/W; watchdog reset: yes.
 * Only sink and disabled encodings are documented. FORCE_CC must allow automatic configuration.
 */
#define MP2724_CC_CFG_SHIFT                       4U
#define MP2724_CC_CFG_MASK                        0x70U
#define MP2724_CC_CFG_DEFAULT                     5U
#define MP2724_CC_CFG_SINK                        0U
#define MP2724_CC_CFG_DISABLED                    5U

/* EN_BOOST [2], R/W; watchdog reset: yes.
 * 0: disable boost and clear BOOST_FAULT; 1: request boost when start conditions permit.
 */
#define MP2724_EN_BOOST_SHIFT                     2U
#define MP2724_EN_BOOST_MASK                      0x04U
#define MP2724_EN_BOOST_DEFAULT                   0U

/* EN_BUCK [1], R/W; watchdog reset: yes.
 * 0: disable buck; 1: allow buck when input is ready.
 */
#define MP2724_EN_BUCK_SHIFT                      1U
#define MP2724_EN_BUCK_MASK                       0x02U
#define MP2724_EN_BUCK_DEFAULT                    1U

/* EN_CHG [0], R/W; watchdog reset: yes.
 * 0: disable charging; 1: allow charging. Toggle to restart charge cycle/safety timer.
 */
#define MP2724_EN_CHG_SHIFT                       0U
#define MP2724_EN_CHG_MASK                        0x01U
#define MP2724_EN_CHG_DEFAULT                     1U

/* VIN_DET: D+/D- detection control and CC override. */
#define MP2724_REG_VIN_DET                        0x0AU
#define MP2724_VIN_DET_RESERVED_MASK              0xCCU
#define MP2724_VIN_DET_WRITABLE_MASK              0x33U
#define MP2724_VIN_DET_COMMAND_MASK               0x10U
#define MP2724_VIN_DET_RESET_DEFAULT              0x23U

/* AUTODPDM [5], R/W; watchdog reset: yes.
 * 0: manual D+/D- detection; 1: automatic after VIN_GD and hold-off.
 */
#define MP2724_AUTODPDM_SHIFT                     5U
#define MP2724_AUTODPDM_MASK                      0x20U
#define MP2724_AUTODPDM_DEFAULT                   1U

/* FORCEDPDM [4], R/W; watchdog reset: no.
 * Write 1 to restart D+/D- detection with input present; self-clears. Result may overwrite IIN_LIM.
 */
#define MP2724_FORCEDPDM_SHIFT                    4U
#define MP2724_FORCEDPDM_MASK                     0x10U
#define MP2724_FORCEDPDM_DEFAULT                  0U

/* FORCE_CC [1:0], R/W; watchdog reset: yes.
 * Only automatic and forced high-impedance encodings are documented.
 */
#define MP2724_FORCE_CC_SHIFT                     0U
#define MP2724_FORCE_CC_MASK                      0x03U
#define MP2724_FORCE_CC_DEFAULT                   3U
#define MP2724_FORCE_CC_AUTO                      0U
#define MP2724_FORCE_CC_HI_Z                      3U

/* CHG_CTRL5: NTC action and battery/boost protection controls. */
#define MP2724_REG_CHG_CTRL5                      0x0CU
#define MP2724_CHG_CTRL5_RESERVED_MASK            0x80U
#define MP2724_CHG_CTRL5_WRITABLE_MASK            0x7FU
#define MP2724_CHG_CTRL5_COMMAND_MASK             0x00U
#define MP2724_CHG_CTRL5_RESET_DEFAULT            0x11U

/* NTC1_ACTION [6], R/W; watchdog reset: no.
 * 0: NTC1 status interrupts only; 1: apply JEITA charging and boost temperature protection.
 */
#define MP2724_NTC1_ACTION_SHIFT                  6U
#define MP2724_NTC1_ACTION_MASK                   0x40U
#define MP2724_NTC1_ACTION_DEFAULT                0U

/* NTC2_ACTION [5], R/W; watchdog reset: no.
 * 0: NTC2 status interrupts only; 1: apply protection. Requires EN_PG_NTC2.
 */
#define MP2724_NTC2_ACTION_SHIFT                  5U
#define MP2724_NTC2_ACTION_MASK                   0x20U
#define MP2724_NTC2_ACTION_DEFAULT                0U

/* BATT_OVP_EN [4], R/W; watchdog reset: yes.
 * 0: ignore battery OVP; 1: enable battery OVP.
 */
#define MP2724_BATT_OVP_EN_SHIFT                  4U
#define MP2724_BATT_OVP_EN_MASK                   0x10U
#define MP2724_BATT_OVP_EN_DEFAULT                1U

/* BATT_LOW [3:2], R/W; watchdog reset: no.
 * Falling battery-low threshold; status is BATT_LOW_STAT (10ms debounce).
 */
#define MP2724_BATT_LOW_SHIFT                     2U
#define MP2724_BATT_LOW_MASK                      0x0CU
#define MP2724_BATT_LOW_DEFAULT                   0U
#define MP2724_BATT_LOW_3000_MV                   0U
#define MP2724_BATT_LOW_3100_MV                   1U
#define MP2724_BATT_LOW_3200_MV                   2U
#define MP2724_BATT_LOW_3300_MV                   3U

/* BOOST_STP_EN [1], R/W; watchdog reset: yes.
 * 0: battery-low interrupt only; 1: stop and latch boost at BATT_LOW (SYS remains powered).
 */
#define MP2724_BOOST_STP_EN_SHIFT                 1U
#define MP2724_BOOST_STP_EN_MASK                  0x02U
#define MP2724_BOOST_STP_EN_DEFAULT               0U

/* BOOST_OTP_EN [0], R/W; watchdog reset: yes.
 * 0: ignore boost TREG protection; 1: stop and latch boost at TREG (SYS remains powered).
 */
#define MP2724_BOOST_OTP_EN_SHIFT                 0U
#define MP2724_BOOST_OTP_EN_MASK                  0x01U
#define MP2724_BOOST_OTP_EN_DEFAULT               1U

/* NTC_ACTION: JEITA warm/cool actions and reductions. */
#define MP2724_REG_NTC_ACTION                     0x0DU
#define MP2724_NTC_ACTION_RESERVED_MASK           0x00U
#define MP2724_NTC_ACTION_WRITABLE_MASK           0xFFU
#define MP2724_NTC_ACTION_COMMAND_MASK            0x00U
#define MP2724_NTC_ACTION_RESET_DEFAULT           0x60U

/* WARM_ACT [7:6], R/W; watchdog reset: no.
 * Action in warm window when NTC protection is enabled.
 */
#define MP2724_WARM_ACT_SHIFT                     6U
#define MP2724_WARM_ACT_MASK                      0xC0U
#define MP2724_WARM_ACT_DEFAULT                   1U
#define MP2724_WARM_ACT_NONE                      0U
#define MP2724_WARM_ACT_REDUCE_VOLTAGE            1U
#define MP2724_WARM_ACT_REDUCE_CURRENT            2U
#define MP2724_WARM_ACT_REDUCE_BOTH               3U

/* COOL_ACT [5:4], R/W; watchdog reset: no.
 * Action in cool window when NTC protection is enabled.
 */
#define MP2724_COOL_ACT_SHIFT                     4U
#define MP2724_COOL_ACT_MASK                      0x30U
#define MP2724_COOL_ACT_DEFAULT                   2U
#define MP2724_COOL_ACT_NONE                      0U
#define MP2724_COOL_ACT_REDUCE_VOLTAGE            1U
#define MP2724_COOL_ACT_REDUCE_CURRENT            2U
#define MP2724_COOL_ACT_REDUCE_BOTH               3U

/* JEITA_VSET [3:2], R/W; watchdog reset: yes.
 * Subtract from VBATT regulation target. Subject to LOCK_CHG; lock wording is ambiguous for reduction encodings.
 */
#define MP2724_JEITA_VSET_SHIFT                   2U
#define MP2724_JEITA_VSET_MASK                    0x0CU
#define MP2724_JEITA_VSET_DEFAULT                 0U
#define MP2724_JEITA_VSET_MINUS_100_MV            0U
#define MP2724_JEITA_VSET_MINUS_150_MV            1U
#define MP2724_JEITA_VSET_MINUS_200_MV            2U
#define MP2724_JEITA_VSET_MINUS_250_MV            3U

/* JEITA_ISET [1:0], R/W; watchdog reset: yes.
 * Fraction of ICC. Code 3 is undocumented. Subject to LOCK_CHG.
 */
#define MP2724_JEITA_ISET_SHIFT                   0U
#define MP2724_JEITA_ISET_MASK                    0x03U
#define MP2724_JEITA_ISET_DEFAULT                 0U
#define MP2724_JEITA_ISET_50_PERCENT              0U
#define MP2724_JEITA_ISET_33_PERCENT              1U
#define MP2724_JEITA_ISET_20_PERCENT              2U

/* NTC_TH: Shared NTC voltage thresholds; ratios to VRNTC, not direct temperatures. */
#define MP2724_REG_NTC_TH                         0x0EU
#define MP2724_NTC_TH_RESERVED_MASK               0x00U
#define MP2724_NTC_TH_WRITABLE_MASK               0xFFU
#define MP2724_NTC_TH_COMMAND_MASK                0x00U
#define MP2724_NTC_TH_RESET_DEFAULT               0x99U

/* VHOT [7:6], R/W; watchdog reset: yes.
 * Hot falling threshold. Temperature labels assume beta=3435 NTC and matching 25C pull-up.
 */
#define MP2724_VHOT_SHIFT                         6U
#define MP2724_VHOT_MASK                          0xC0U
#define MP2724_VHOT_DEFAULT                       2U
#define MP2724_VHOT_291_PERMILLE                  0U
#define MP2724_VHOT_259_PERMILLE                  1U
#define MP2724_VHOT_230_PERMILLE                  2U
#define MP2724_VHOT_204_PERMILLE                  3U

/* VWARM [5:4], R/W; watchdog reset: yes.
 * Warm falling threshold.
 */
#define MP2724_VWARM_SHIFT                        4U
#define MP2724_VWARM_MASK                         0x30U
#define MP2724_VWARM_DEFAULT                      1U
#define MP2724_VWARM_365_PERMILLE                 0U
#define MP2724_VWARM_326_PERMILLE                 1U
#define MP2724_VWARM_291_PERMILLE                 2U
#define MP2724_VWARM_259_PERMILLE                 3U

/* VCOOL [3:2], R/W; watchdog reset: yes.
 * Cool rising threshold.
 */
#define MP2724_VCOOL_SHIFT                        2U
#define MP2724_VCOOL_MASK                         0x0CU
#define MP2724_VCOOL_DEFAULT                      2U
#define MP2724_VCOOL_742_PERMILLE                 0U
#define MP2724_VCOOL_696_PERMILLE                 1U
#define MP2724_VCOOL_648_PERMILLE                 2U
#define MP2724_VCOOL_599_PERMILLE                 3U

/* VCOLD [1:0], R/W; watchdog reset: yes.
 * Cold rising threshold.
 */
#define MP2724_VCOLD_SHIFT                        0U
#define MP2724_VCOLD_MASK                         0x03U
#define MP2724_VCOLD_DEFAULT                      1U
#define MP2724_VCOLD_784_PERMILLE                 0U
#define MP2724_VCOLD_742_PERMILLE                 1U
#define MP2724_VCOLD_696_PERMILLE                 2U
#define MP2724_VCOLD_648_PERMILLE                 3U

/* VIN_IMPD: IN impedance test current source and comparator. */
#define MP2724_REG_VIN_IMPD                       0x0FU
#define MP2724_VIN_IMPD_RESERVED_MASK             0x80U
#define MP2724_VIN_IMPD_WRITABLE_MASK             0x7FU
#define MP2724_VIN_IMPD_COMMAND_MASK              0x00U
#define MP2724_VIN_IMPD_RESET_DEFAULT             0x00U

/* VIN_SRC_EN [6], R/W; watchdog reset: yes.
 * 0: stop test and clear VIN_TEST_HIGH; 1: source test current. Effective only with buck and boost off.
 */
#define MP2724_VIN_SRC_EN_SHIFT                   6U
#define MP2724_VIN_SRC_EN_MASK                    0x40U
#define MP2724_VIN_SRC_EN_DEFAULT                 0U

/* IVIN_SRC [5:2], R/W; watchdog reset: yes.
 * Test current; codes 9..15 are undocumented.
 */
#define MP2724_IVIN_SRC_SHIFT                     2U
#define MP2724_IVIN_SRC_MASK                      0x3CU
#define MP2724_IVIN_SRC_DEFAULT                   0U
#define MP2724_IVIN_SRC_5_UA                      0U
#define MP2724_IVIN_SRC_10_UA                     1U
#define MP2724_IVIN_SRC_20_UA                     2U
#define MP2724_IVIN_SRC_40_UA                     3U
#define MP2724_IVIN_SRC_80_UA                     4U
#define MP2724_IVIN_SRC_160_UA                    5U
#define MP2724_IVIN_SRC_320_UA                    6U
#define MP2724_IVIN_SRC_640_UA                    7U
#define MP2724_IVIN_SRC_1280_UA                   8U

/* VIN_TEST [1:0], R/W; watchdog reset: yes.
 * Comparator threshold; reaching it latches VIN_TEST_HIGH.
 */
#define MP2724_VIN_TEST_SHIFT                     0U
#define MP2724_VIN_TEST_MASK                      0x03U
#define MP2724_VIN_TEST_DEFAULT                   0U
#define MP2724_VIN_TEST_300_MV                    0U
#define MP2724_VIN_TEST_500_MV                    1U
#define MP2724_VIN_TEST_1000_MV                   2U
#define MP2724_VIN_TEST_1500_MV                   3U

/* INT_MASK: Mask selected interrupt pulses; status bits remain readable. */
#define MP2724_REG_INT_MASK                       0x10U
#define MP2724_INT_MASK_RESERVED_MASK             0xC0U
#define MP2724_INT_MASK_WRITABLE_MASK             0x3FU
#define MP2724_INT_MASK_COMMAND_MASK              0x00U
#define MP2724_INT_MASK_RESET_DEFAULT             0x44U

/* MASK_THERM [5], R/W; watchdog reset: no.
 * 0: allow THERM_STAT pulse; 1: mask.
 */
#define MP2724_MASK_THERM_SHIFT                   5U
#define MP2724_MASK_THERM_MASK                    0x20U
#define MP2724_MASK_THERM_DEFAULT                 0U

/* MASK_DPM [4], R/W; watchdog reset: no.
 * 0: allow VINDPM/IINDPM pulses; 1: mask both.
 */
#define MP2724_MASK_DPM_SHIFT                     4U
#define MP2724_MASK_DPM_MASK                      0x10U
#define MP2724_MASK_DPM_DEFAULT                   0U

/* MASK_TOPOFF [3], R/W; watchdog reset: no.
 * 0: allow top-off start/end pulses; 1: mask.
 */
#define MP2724_MASK_TOPOFF_SHIFT                  3U
#define MP2724_MASK_TOPOFF_MASK                   0x08U
#define MP2724_MASK_TOPOFF_DEFAULT                0U

/* MASK_CC_INT [2], R/W; watchdog reset: no.
 * 0: allow CC sink-advertisement changes; 1: mask.
 */
#define MP2724_MASK_CC_INT_SHIFT                  2U
#define MP2724_MASK_CC_INT_MASK                   0x04U
#define MP2724_MASK_CC_INT_DEFAULT                1U

/* MASK_BATT_LOW [1], R/W; watchdog reset: no.
 * 0: allow battery-low pulse; 1: mask.
 */
#define MP2724_MASK_BATT_LOW_SHIFT                1U
#define MP2724_MASK_BATT_LOW_MASK                 0x02U
#define MP2724_MASK_BATT_LOW_DEFAULT              0U

/* MASK_DEBUG [0], R/W; watchdog reset: no.
 * 0: allow DebugAccessory.SNK entry/exit pulse; 1: mask.
 */
#define MP2724_MASK_DEBUG_SHIFT                   0U
#define MP2724_MASK_DEBUG_MASK                    0x01U
#define MP2724_MASK_DEBUG_DEFAULT                 0U

/* STATUS0: Read-only D+/D- source classification and input regulation. */
#define MP2724_REG_STATUS0                        0x11U
#define MP2724_STATUS0_RESERVED_MASK              0x0CU
#define MP2724_STATUS0_WRITABLE_MASK              0x00U
#define MP2724_STATUS0_COMMAND_MASK               0x00U
/* Status defaults are unspecified; do not assume a reset byte. */

/* DPDM_STAT [7:4], R/O.
 * Source classification. Unlisted codes are undocumented.
 * Initial limits: not-started/SDP/unknown 500mA; DCP/DCP_ALT 2000mA;
 * CDP 1500mA; divider 1/2/3/4/5: 1000/2100/2400/2000/3000mA.
 * CC advertisements and IIN_MODE may override these limits.
 * Detection completion interrupts per p39 despite p36 saying No.
 */
#define MP2724_DPDM_STAT_SHIFT                    4U
#define MP2724_DPDM_STAT_MASK                     0xF0U
#define MP2724_DPDM_STAT_NOT_STARTED              0U
#define MP2724_DPDM_STAT_SDP                      1U
#define MP2724_DPDM_STAT_DCP                      2U
#define MP2724_DPDM_STAT_CDP                      3U
#define MP2724_DPDM_STAT_DIVIDER1                 4U
#define MP2724_DPDM_STAT_DIVIDER2                 5U
#define MP2724_DPDM_STAT_DIVIDER3                 6U
#define MP2724_DPDM_STAT_DIVIDER4                 7U
#define MP2724_DPDM_STAT_UNKNOWN                  8U
#define MP2724_DPDM_STAT_DCP_ALT                  9U
#define MP2724_DPDM_STAT_DIVIDER5                 14U

/* VINDPM_STAT [1], R/O.
 * 0: inactive; 1: input voltage regulation active. Rising edge interrupt masked by MASK_DPM.
 */
#define MP2724_VINDPM_STAT_SHIFT                  1U
#define MP2724_VINDPM_STAT_MASK                   0x02U

/* IINDPM_STAT [0], R/O.
 * 0: inactive; 1: input current regulation active. Rising edge interrupt masked by MASK_DPM.
 */
#define MP2724_IINDPM_STAT_SHIFT                  0U
#define MP2724_IINDPM_STAT_MASK                   0x01U

/* STATUS1: Read-only input readiness, power path, thermal and watchdog state. */
#define MP2724_REG_STATUS1                        0x12U
#define MP2724_STATUS1_RESERVED_MASK              0x80U
#define MP2724_STATUS1_WRITABLE_MASK              0x00U
#define MP2724_STATUS1_COMMAND_MASK               0x00U
/* Status defaults are unspecified; do not assume a reset byte. */

/* VIN_GD [6], R/O.
 * 0: invalid input; 1: valid input after debounce. Changes interrupt; drives PG low in PG mode.
 */
#define MP2724_VIN_GD_SHIFT                       6U
#define MP2724_VIN_GD_MASK                        0x40U

/* VIN_RDY [5], R/O.
 * 0: not ready; 1: source detection complete and IIN_LIM updated. Rising edge interrupts.
 */
#define MP2724_VIN_RDY_SHIFT                      5U
#define MP2724_VIN_RDY_MASK                       0x20U

/* LEGACYCABLE [4], R/O.
 * 0: normal; 1: legacy cable detected. No interrupt.
 */
#define MP2724_LEGACYCABLE_SHIFT                  4U
#define MP2724_LEGACYCABLE_MASK                   0x10U

/* THERM_STAT [3], R/O.
 * 0: inactive; 1: thermal regulation active. Rising edge interrupt masked by MASK_THERM.
 */
#define MP2724_THERM_STAT_SHIFT                   3U
#define MP2724_THERM_STAT_MASK                    0x08U

/* VSYS_STAT [2], R/O.
 * 0: battery below SYS_MIN; 1: battery above SYS_MIN. No interrupt.
 */
#define MP2724_VSYS_STAT_SHIFT                    2U
#define MP2724_VSYS_STAT_MASK                     0x04U

/* WATCHDOG_FAULT [1], R/O.
 * 0: normal; 1: watchdog expired (also default startup state). Rising edge interrupts.
 */
#define MP2724_WATCHDOG_FAULT_SHIFT               1U
#define MP2724_WATCHDOG_FAULT_MASK                0x02U

/* WATCHDOG_BARK [0], R/O.
 * 0: normal; 1: watchdog reached three quarters of timeout. Rising edge interrupts.
 */
#define MP2724_WATCHDOG_BARK_SHIFT                0U
#define MP2724_WATCHDOG_BARK_MASK                 0x01U

/* STATUS2: Read-only charging phase and converter faults. */
#define MP2724_REG_STATUS2                        0x13U
#define MP2724_STATUS2_RESERVED_MASK              0x00U
#define MP2724_STATUS2_WRITABLE_MASK              0x00U
#define MP2724_STATUS2_COMMAND_MASK               0x00U
/* Status defaults are unspecified; do not assume a reset byte. */

/* CHG_STAT [7:5], R/O.
 * Charging phase; done/recharge transitions interrupt per p39 despite p37 saying No. Codes 6..7 undocumented.
 */
#define MP2724_CHG_STAT_SHIFT                     5U
#define MP2724_CHG_STAT_MASK                      0xE0U
#define MP2724_CHG_STAT_NOT_CHARGING              0U
#define MP2724_CHG_STAT_TRICKLE                   1U
#define MP2724_CHG_STAT_PRECHARGE                 2U
#define MP2724_CHG_STAT_FAST                      3U
#define MP2724_CHG_STAT_CONSTANT_VOLTAGE          4U
#define MP2724_CHG_STAT_DONE                      5U

/* BOOST_FAULT [4:2], R/O.
 * Fault entry interrupts; OVP recovery also interrupts. Latches except OVP; clear with EN_BOOST=0. Codes 5..7 undocumented.
 */
#define MP2724_BOOST_FAULT_SHIFT                  2U
#define MP2724_BOOST_FAULT_MASK                   0x1CU
#define MP2724_BOOST_FAULT_NORMAL                 0U
#define MP2724_BOOST_FAULT_OVERLOAD               1U
#define MP2724_BOOST_FAULT_OVP                    2U
#define MP2724_BOOST_FAULT_OVER_TEMPERATURE       3U
#define MP2724_BOOST_FAULT_BATTERY_LOW            4U

/* CHG_FAULT [1:0], R/O.
 * Fault entry interrupts.
 */
#define MP2724_CHG_FAULT_SHIFT                    0U
#define MP2724_CHG_FAULT_MASK                     0x03U
#define MP2724_CHG_FAULT_NORMAL                   0U
#define MP2724_CHG_FAULT_INPUT_OVP                1U
#define MP2724_CHG_FAULT_TIMER_EXPIRED            2U
#define MP2724_CHG_FAULT_BATTERY_OVP              3U

/* STATUS3: Read-only missing-component indications and NTC windows. */
#define MP2724_REG_STATUS3                        0x14U
#define MP2724_STATUS3_RESERVED_MASK              0x00U
#define MP2724_STATUS3_WRITABLE_MASK              0x00U
#define MP2724_STATUS3_COMMAND_MASK               0x00U
/* Status defaults are unspecified; do not assume a reset byte. */

/* NTC_MISSING [7], R/O.
 * 0: normal; 1: NTC voltage above 95% of VRNTC. Changes interrupt.
 */
#define MP2724_NTC_MISSING_SHIFT                  7U
#define MP2724_NTC_MISSING_MASK                   0x80U

/* BATT_MISSING [6], R/O.
 * 0: normal; 1: two charge terminations within 3s. Changes interrupt.
 */
#define MP2724_BATT_MISSING_SHIFT                 6U
#define MP2724_BATT_MISSING_MASK                  0x40U

/* NTC1_FAULT [5:3], R/O.
 * NTC1 temperature window; changes interrupt. Codes 5..7 undocumented.
 */
#define MP2724_NTC1_FAULT_SHIFT                   3U
#define MP2724_NTC1_FAULT_MASK                    0x38U
#define MP2724_NTC1_FAULT_NORMAL                  0U
#define MP2724_NTC1_FAULT_WARM                    1U
#define MP2724_NTC1_FAULT_COOL                    2U
#define MP2724_NTC1_FAULT_COLD                    3U
#define MP2724_NTC1_FAULT_HOT                     4U

/* NTC2_FAULT [2:0], R/O.
 * NTC2 temperature window; changes interrupt. Codes 5..7 undocumented.
 */
#define MP2724_NTC2_FAULT_SHIFT                   0U
#define MP2724_NTC2_FAULT_MASK                    0x07U
#define MP2724_NTC2_FAULT_NORMAL                  0U
#define MP2724_NTC2_FAULT_WARM                    1U
#define MP2724_NTC2_FAULT_COOL                    2U
#define MP2724_NTC2_FAULT_COLD                    3U
#define MP2724_NTC2_FAULT_HOT                     4U

/* STATUS4: Read-only CC1/CC2 sink power advertisements. */
#define MP2724_REG_STATUS4                        0x15U
#define MP2724_STATUS4_RESERVED_MASK              0x0FU
#define MP2724_STATUS4_WRITABLE_MASK              0x00U
#define MP2724_STATUS4_COMMAND_MASK               0x00U
/* Status defaults are unspecified; do not assume a reset byte. */

/* CC1_SNK_STAT [7:6], R/O.
 * CC1 advertisement; debounced changes interrupt unless MASK_CC_INT.
 */
#define MP2724_CC1_SNK_STAT_SHIFT                 6U
#define MP2724_CC1_SNK_STAT_MASK                  0xC0U
#define MP2724_CC1_SNK_STAT_VRA                   0U
#define MP2724_CC1_SNK_STAT_DEFAULT_USB           1U
#define MP2724_CC1_SNK_STAT_1500_MA               2U
#define MP2724_CC1_SNK_STAT_3000_MA               3U

/* CC2_SNK_STAT [5:4], R/O.
 * CC2 advertisement; debounced changes interrupt unless MASK_CC_INT.
 */
#define MP2724_CC2_SNK_STAT_SHIFT                 4U
#define MP2724_CC2_SNK_STAT_MASK                  0x30U
#define MP2724_CC2_SNK_STAT_VRA                   0U
#define MP2724_CC2_SNK_STAT_DEFAULT_USB           1U
#define MP2724_CC2_SNK_STAT_1500_MA               2U
#define MP2724_CC2_SNK_STAT_3000_MA               3U

/* STATUS5: Read-only top-off, battery direction/low, impedance and debug status. */
#define MP2724_REG_STATUS5                        0x16U
#define MP2724_STATUS5_RESERVED_MASK              0x89U
#define MP2724_STATUS5_WRITABLE_MASK              0x00U
#define MP2724_STATUS5_COMMAND_MASK               0x00U
/* Status defaults are unspecified; do not assume a reset byte. */

/* TOPOFF_ACTIVE [6], R/O.
 * 0: top-off timer inactive; 1: counting. Changes interrupt unless MASK_TOPOFF.
 */
#define MP2724_TOPOFF_ACTIVE_SHIFT                6U
#define MP2724_TOPOFF_ACTIVE_MASK                 0x40U

/* BFET_STAT [5], R/O.
 * 0: charging or disabled; 1: discharging. Direction for IB measurement; no interrupt.
 */
#define MP2724_BFET_STAT_SHIFT                    5U
#define MP2724_BFET_STAT_MASK                     0x20U

/* BATT_LOW_STAT [4], R/O.
 * 0: above BATT_LOW; 1: below; 200mV hysteresis. Rising edge interrupt unless MASK_BATT_LOW.
 */
#define MP2724_BATT_LOW_STAT_SHIFT                4U
#define MP2724_BATT_LOW_STAT_MASK                 0x10U

/* VIN_TEST_HIGH [2], R/O.
 * 0: below test threshold; 1: reached threshold (latched). Rising edge interrupts; clear via VIN_SRC_EN=0.
 */
#define MP2724_VIN_TEST_HIGH_SHIFT                2U
#define MP2724_VIN_TEST_HIGH_MASK                 0x04U

/* DEBUGACC [1], R/O.
 * 0: normal; 1: DebugAccessory.SNK. Entry/exit interrupts unless MASK_DEBUG.
 */
#define MP2724_DEBUGACC_SHIFT                     1U
#define MP2724_DEBUGACC_MASK                      0x02U

/* Linear fields: physical setpoint = OFFSET + STEP * unshifted code.
 * No rounding or clamping is performed by FIELD_PREP.
 */
#define MP2724_IIN_LIM_OFFSET_MA                  100U
#define MP2724_IIN_LIM_STEP_MA                    100U
#define MP2724_IIN_LIM_CODE_MIN                   0U
#define MP2724_IIN_LIM_CODE_MAX                   31U
#define MP2724_ICC_OFFSET_MA                      0U
#define MP2724_ICC_STEP_MA                        40U
#define MP2724_ICC_CODE_MIN                       0U
#define MP2724_ICC_CODE_MAX                       55U
#define MP2724_IPRE_OFFSET_MA                     0U
#define MP2724_IPRE_STEP_MA                       20U
#define MP2724_IPRE_CODE_MIN                      0U
#define MP2724_IPRE_CODE_MAX                      15U
#define MP2724_ITERM_OFFSET_MA                    15U
#define MP2724_ITERM_STEP_MA                      15U
#define MP2724_ITERM_CODE_MIN                     0U
#define MP2724_ITERM_CODE_MAX                     15U
#define MP2724_ITRICKLE_OFFSET_MA                 0U
#define MP2724_ITRICKLE_STEP_MA                   16U
#define MP2724_ITRICKLE_CODE_MIN                  0U
#define MP2724_ITRICKLE_CODE_MAX                  7U
#define MP2724_VIN_LIM_OFFSET_MV                  3880U
#define MP2724_VIN_LIM_STEP_MV                    80U
#define MP2724_VIN_LIM_CODE_MIN                   0U
#define MP2724_VIN_LIM_CODE_MAX                   15U
#define MP2724_VBATT_OFFSET_MV                    3600U
#define MP2724_VBATT_STEP_MV                      25U
#define MP2724_VBATT_CODE_MIN                     0U
#define MP2724_VBATT_CODE_MAX                     40U

/* Data-only register catalogue, in ascending address order (not address-indexed).
 * reset_default_valid_mask is zero for status bytes, 0xFF for config bytes.
 * reset_default is a reference only, never an initialization recipe.
 * command_mask marks write-one actions, not necessarily self-clearing bits.
 */
typedef struct {
    uint8_t address;
    uint8_t reset_default;
    uint8_t reset_default_valid_mask;
    uint8_t writable_mask;
    uint8_t command_mask;
} mp2724_register_info_t;

extern const mp2724_register_info_t mp2724_registers[MP2724_REGISTER_COUNT];

#endif /* USER_MP2724_H */
