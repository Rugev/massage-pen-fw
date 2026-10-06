#ifndef USER_I2C_DEVICE_H
#define USER_I2C_DEVICE_H
#include <stdbool.h>
#include <stdint.h>
#include "stm32g0xx_hal.h"
#define I2C_DEVICE_TIMEOUT_MS 5U
#define I2C_DEVICE_RETRY_DELAY_MS 5U
#define I2C_DEVICE_FAILURE_LIMIT 3U
/* Remaining attempt slots bound recovery health; ownership still needs drain. */
#define I2C_DEVICE_RECOVERY_LIMIT_MS \
    ((I2C_DEVICE_FAILURE_LIMIT - 1U) * \
     (I2C_DEVICE_TIMEOUT_MS + I2C_DEVICE_RETRY_DELAY_MS))
/* Nonblocking one-byte, 8-bit memory-address transfers, address already shifted.
 * start functions retain buffer until one terminal callback or successful stop_quiescent.
 * false may mean an uncertain partial transfer; commands will never replay it.
 * stop_quiescent starts/polls abort and drains all queued IRQs and callbacks
 * before returning true; it must be idempotent. false retains ownership and prevents buffer reuse.
 * No callback has an epoch: adapter MUST honor this drain contract. NULL gates
 * requests. Dedicated I2C handle, no other owner; ops/handle outlive driver. */
typedef struct {
    bool (*read_it)(I2C_HandleTypeDef *, uint16_t, uint8_t, uint8_t *);
    bool (*write_it)(I2C_HandleTypeDef *, uint16_t, uint8_t, uint8_t *);
    bool (*stop_quiescent)(I2C_HandleTypeDef *);
} I2C_DeviceOps;
typedef enum {
    I2C_RESULT_IDLE, I2C_RESULT_PENDING, I2C_RESULT_READ, I2C_RESULT_VERIFIED,
    I2C_RESULT_COMMAND_ACCEPTED, I2C_RESULT_COMMAND_UNCERTAIN,
    I2C_RESULT_FAILED, I2C_RESULT_CANCELLED, I2C_RESULT_REJECTED
} I2C_ResultState;
typedef struct {
    I2C_ResultState state;
    uint8_t reg, value, failures;
    bool exhausted; /* Transfer attempts OR bounded recovery exhausted. */
    bool recovering; /* Buffer/handle retained; terminal result cannot be ACKed. */
} I2C_DeviceResult;
/* Internal shared engine state. Use device APIs below through driver wrappers. */
typedef struct {
    I2C_HandleTypeDef *handle;
    const I2C_DeviceOps *ops;
    I2C_DeviceResult result;
    uint16_t address;
    uint8_t phase, retry_phase, mask, requested, suppress, guard, buffer;
    bool command, active, draining, fault, cancel;
    I2C_ResultState terminal;
    volatile bool accepting;
    volatile uint8_t event;
    uint32_t started_ms, failed_ms;
} I2C_Device;
void I2C_DeviceInit(I2C_Device *, I2C_HandleTypeDef *, const I2C_DeviceOps *, uint16_t);
bool I2C_DeviceRequest(I2C_Device *, uint8_t reg, uint8_t mask, uint8_t value,
                       uint8_t suppress, uint8_t guard, bool command);
void I2C_DeviceUpdate(I2C_Device *);
void I2C_DeviceOnComplete(I2C_Device *, I2C_HandleTypeDef *, bool write);
void I2C_DeviceOnError(I2C_Device *, I2C_HandleTypeDef *);
bool I2C_DeviceAcknowledge(I2C_Device *);
void I2C_DeviceCancel(I2C_Device *);
#endif
