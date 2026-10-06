#include "i2c_device.h"
#include <stddef.h>
#include <string.h>

enum { PHASE_READ, PHASE_PREPARE, PHASE_WRITE, PHASE_VERIFY };
enum { EVENT_NONE, EVENT_READ, EVENT_WRITE, EVENT_ERROR };

void I2C_DeviceInit(I2C_Device *d, I2C_HandleTypeDef *handle,
                    const I2C_DeviceOps *ops, uint16_t address)
{
    memset(d, 0, sizeof *d);
    d->handle = handle;
    d->ops = ops;
    d->address = address;
}

bool I2C_DeviceRequest(I2C_Device *d, uint8_t reg, uint8_t mask,
                       uint8_t value, uint8_t suppress, uint8_t guard,
                       bool command)
{
    if (d->fault || d->draining || d->result.state != I2C_RESULT_IDLE ||
        d->handle == NULL || d->ops == NULL || d->ops->read_it == NULL ||
        d->ops->write_it == NULL || d->ops->stop_quiescent == NULL) {
        return false;
    }
    d->result = (I2C_DeviceResult){.state = I2C_RESULT_PENDING, .reg = reg};
    d->phase = mask == 0 ? PHASE_READ : PHASE_PREPARE;
    d->mask = mask;
    d->requested = value;
    d->suppress = suppress;
    d->guard = guard;
    d->command = command;
    d->cancel = false;
    d->terminal = I2C_RESULT_IDLE;
    return true;
}

/* Disable callback acceptance atomically before abort or buffer reuse. */
static void stop_accepting(I2C_Device *d)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    d->accepting = false;
    d->event = EVENT_NONE;
    __set_PRIMASK(primask);
}

static void finish(I2C_Device *d, I2C_ResultState state)
{
    d->result.state = state;
    d->result.value = d->buffer;
    if (state == I2C_RESULT_READ || state == I2C_RESULT_VERIFIED ||
        state == I2C_RESULT_COMMAND_ACCEPTED) {
        d->result.failures = 0;
    }
}

static void failed(I2C_Device *d, uint32_t now)
{
    stop_accepting(d);
    d->active = false;
    d->draining = true;
    d->failed_ms = now;
    d->result.recovering = true;
    d->result.failures++;
    /* After a command ACK, retries can only repeat its observation. */
    d->retry_phase = d->command && d->phase == PHASE_VERIFY ? PHASE_VERIFY :
                     (d->mask == 0 ? PHASE_READ : PHASE_PREPARE);
    if (d->command && d->phase == PHASE_WRITE) {
        d->terminal = I2C_RESULT_COMMAND_UNCERTAIN;
        /* Expose uncertainty immediately, including the last allowed attempt;
         * ownership and health exhaustion are independently retained. */
        finish(d, d->terminal);
    }
    if (d->result.failures >= I2C_DEVICE_FAILURE_LIMIT) {
        d->fault = true;
        d->result.exhausted = true;
        if (d->terminal != I2C_RESULT_COMMAND_UNCERTAIN) {
            d->terminal = I2C_RESULT_FAILED;
        }
    }
    /* Initiate abort immediately, including failed starts. */
    if (d->ops->stop_quiescent(d->handle) &&
        d->terminal != I2C_RESULT_IDLE) {
        d->draining = false;
        d->result.recovering = false;
        finish(d, d->terminal);
    }
}

void I2C_DeviceUpdate(I2C_Device *d)
{
    uint32_t now = HAL_GetTick();
    if (d->draining) {
        if (!d->ops->stop_quiescent(d->handle)) {
            if ((uint32_t)(now - d->failed_ms) >= I2C_DEVICE_RECOVERY_LIMIT_MS) {
                d->fault = true;
                d->result.exhausted = true;
                if (d->terminal != I2C_RESULT_COMMAND_UNCERTAIN) {
                    d->terminal = I2C_RESULT_FAILED;
                }
                /* Health is terminal, but retain ownership and keep polling.
                 * No synthetic transfer failures or retries are invented. */
                finish(d, d->terminal);
            }
            return;
        }
        d->result.recovering = false;
        if (d->terminal != I2C_RESULT_IDLE) {
            d->draining = false;
            finish(d, d->terminal);
            return;
        }
        if (d->cancel) {
            d->draining = false;
            finish(d, I2C_RESULT_CANCELLED);
            return;
        }
        if ((uint32_t)(now - d->failed_ms) < I2C_DEVICE_RETRY_DELAY_MS) {
            return;
        }
        d->draining = false;
        d->phase = d->retry_phase;
    }
    if (d->result.state != I2C_RESULT_PENDING) {
        return;
    }
    if (d->active) {
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        uint8_t event = d->event;
        if (event != EVENT_NONE) {
            d->accepting = false;
            d->event = EVENT_NONE;
        }
        __set_PRIMASK(primask);
        if (event == EVENT_ERROR ||
            (event == EVENT_NONE &&
             (uint32_t)(now - d->started_ms) >= I2C_DEVICE_TIMEOUT_MS)) {
            failed(d, now);
            return;
        }
        if (event == EVENT_NONE) {
            return;
        }
        d->active = false;
        if (d->phase == PHASE_READ) {
            finish(d, I2C_RESULT_READ);
            return;
        }
        if (d->phase == PHASE_PREPARE) {
            /* Writing back an active battery-disconnect state could replay
             * shipping; suppressing it could reconnect after OCP. Neither is
             * an ordinary configuration update. Leave the register untouched. */
            if ((d->buffer & d->guard) != 0) {
                finish(d, I2C_RESULT_REJECTED);
                return;
            }
            d->buffer = (uint8_t)((d->buffer & ~(d->mask | d->suppress)) |
                                  d->requested);
            d->phase = PHASE_WRITE;
        } else if (d->phase == PHASE_WRITE) {
            d->phase = PHASE_VERIFY;
        } else {
            /* Commands can self-clear, reset registers or finish immediately.
             * Readback is an observation, not proof of side-effect completion.
             * Configuration success requires all requested fields match. */
            if (!d->command && (d->buffer & d->mask) != d->requested) {
                failed(d, now);
                return;
            }
            finish(d, d->command ? I2C_RESULT_COMMAND_ACCEPTED :
                                   I2C_RESULT_VERIFIED);
            return;
        }
    }
    /* Own callbacks before starting: the ISR may precede HAL start's return. */
    d->event = EVENT_NONE;
    d->started_ms = now;
    d->active = true;
    d->accepting = true;
    bool started = d->phase == PHASE_WRITE ?
        d->ops->write_it(d->handle, d->address, d->result.reg, &d->buffer) :
        d->ops->read_it(d->handle, d->address, d->result.reg, &d->buffer);
    if (!started) {
        failed(d, now);
    }
}

void I2C_DeviceOnComplete(I2C_Device *d, I2C_HandleTypeDef *handle, bool write)
{
    if (handle == d->handle && d->accepting && d->event == EVENT_NONE &&
        write == (d->phase == PHASE_WRITE)) {
        d->event = write ? EVENT_WRITE : EVENT_READ;
    }
}

void I2C_DeviceOnError(I2C_Device *d, I2C_HandleTypeDef *handle)
{
    if (handle == d->handle && d->accepting && d->event == EVENT_NONE) {
        d->event = EVENT_ERROR;
    }
}

bool I2C_DeviceAcknowledge(I2C_Device *d)
{
    if (d->draining || d->result.state == I2C_RESULT_IDLE ||
        d->result.state == I2C_RESULT_PENDING) {
        return false;
    }
    d->result.state = I2C_RESULT_IDLE;
    return true;
}

void I2C_DeviceCancel(I2C_Device *d)
{
    if (d->result.state != I2C_RESULT_PENDING) {
        return;
    }
    stop_accepting(d);
    d->active = false;
    d->cancel = true;
    if (!d->draining) {
        d->failed_ms = HAL_GetTick();
    }
    d->draining = true;
    d->result.recovering = true;
}
