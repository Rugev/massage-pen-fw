#include "drv2624.h"

/* Register reference only. Constants, field encodings and conversion notes are
 * in drv2624.h; the operating guide is docs/architecture/drv2624.md.
 *
 * Future implementation order (no transport/API chosen here):
 * - Release NRST and allow POWER_UP_WAIT_MS before addressing the device.
 * - Verify CHIPID/REV; configure actuator and trigger function while idle.
 * - Set validated motor voltage/timing inputs, then calibrate or restore the
 *   matched A_CAL_COMP, A_CAL_BEMF and BEMF_GAIN results.
 * - Select process, prepare RTP_INPUT or RAM/sequencer, and trigger explicitly.
 * - Capture STATUS once per observation: reads clear all sticky event flags.
 * - Account for auto-brake completion and the pseudo-standby workaround before
 *   considering a software-stopped playback fully in low-power standby.
 *
 * No I2C functions, runtime floating point, initialization sequence, motor
 * profile, polling/interrupt integration or recovery policy is implemented.
 */
