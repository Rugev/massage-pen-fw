#include "firmware_hw.h"
#include "mp2724.h"
#include "drv2624.h"
#include "stm32g0xx_ll_adc.h"
#include <stddef.h>
#include <string.h>

static ADC_HandleTypeDef *sensor_adc;
static _Alignas(uint32_t) uint16_t raw_dma[SENSORS_DMA_WORD_COUNT];
static uint16_t *sensor_frames;
static volatile bool adc_accepting, adc_inflight, adc_fault;
static unsigned adc_half;
static TIM_HandleTypeDef *heater_timer;
typedef struct {
    I2C_HandleTypeDef *handle;
    uint16_t address;
    volatile bool accepting;
    bool aborting, write;
    uint32_t abort_started;
} Bus;
static Bus charger_bus, motor_bus;

static bool adc_config_valid(ADC_HandleTypeDef *a)
{
    if (a == NULL || a->Instance != ADC1 || a->DMA_Handle == NULL) return false;
    DMA_HandleTypeDef *d = a->DMA_Handle;
    return a->Init.Resolution == ADC_RESOLUTION_12B &&
        a->Init.DataAlign == ADC_DATAALIGN_RIGHT && a->Init.ScanConvMode == ADC_SCAN_ENABLE &&
        a->Init.ContinuousConvMode == DISABLE && a->Init.NbrOfConversion == SENSORS_FRAME_WORD_COUNT &&
        a->Init.DiscontinuousConvMode == DISABLE && a->Init.ExternalTrigConv == ADC_SOFTWARE_START &&
        a->Init.ExternalTrigConvEdge == ADC_EXTERNALTRIGCONVEDGE_NONE &&
        a->Init.DMAContinuousRequests == ENABLE && a->Init.LowPowerAutoWait == DISABLE &&
        a->Init.LowPowerAutoPowerOff == DISABLE && a->Init.OversamplingMode == ENABLE &&
        a->Init.Oversampling.Ratio == ADC_OVERSAMPLING_RATIO_16 &&
        a->Init.Oversampling.RightBitShift == ADC_RIGHTBITSHIFT_NONE &&
        a->Init.Oversampling.TriggeredMode == ADC_TRIGGEREDMODE_SINGLE_TRIGGER &&
        LL_ADC_REG_GetSequencerConfigurable(a->Instance) == LL_ADC_REG_SEQ_CONFIGURABLE &&
        LL_ADC_REG_GetSequencerLength(a->Instance) == LL_ADC_REG_SEQ_SCAN_ENABLE_2RANKS &&
        LL_ADC_REG_GetSequencerRanks(a->Instance, LL_ADC_REG_RANK_1) == LL_ADC_CHANNEL_1 &&
        LL_ADC_REG_GetSequencerRanks(a->Instance, LL_ADC_REG_RANK_2) == LL_ADC_CHANNEL_8 &&
        LL_ADC_GetChannelSamplingTime(a->Instance, LL_ADC_CHANNEL_1) == LL_ADC_SAMPLINGTIME_COMMON_1 &&
        LL_ADC_GetChannelSamplingTime(a->Instance, LL_ADC_CHANNEL_8) == LL_ADC_SAMPLINGTIME_COMMON_1 &&
        d->Parent == a && d->Instance == DMA1_Channel1 && d->Init.Request == DMA_REQUEST_ADC1 &&
        d->Init.Direction == DMA_PERIPH_TO_MEMORY && d->Init.PeriphInc == DMA_PINC_DISABLE &&
        d->Init.MemInc == DMA_MINC_ENABLE && d->Init.PeriphDataAlignment == DMA_PDATAALIGN_HALFWORD &&
        d->Init.MemDataAlignment == DMA_MDATAALIGN_HALFWORD && d->Init.Mode == DMA_CIRCULAR;
}
static void dma_half(DMA_HandleTypeDef *d)
{ if (sensor_adc != NULL && d == sensor_adc->DMA_Handle) HAL_ADC_ConvHalfCpltCallback(sensor_adc); }
static void dma_full(DMA_HandleTypeDef *d)
{ if (sensor_adc != NULL && d == sensor_adc->DMA_Handle) HAL_ADC_ConvCpltCallback(sensor_adc); }
static void dma_error(DMA_HandleTypeDef *d)
{ if (sensor_adc != NULL && d == sensor_adc->DMA_Handle) HAL_ADC_ErrorCallback(sensor_adc); }

static bool adc_arm(ADC_HandleTypeDef *a, uint16_t *frames, uint32_t count)
{
    if (a != sensor_adc || !adc_config_valid(a) || frames == NULL ||
        count != SENSORS_DMA_WORD_COUNT || LL_ADC_IsEnabled(a->Instance) ||
        LL_ADC_REG_IsConversionOngoing(a->Instance)) return false;
    adc_accepting = adc_inflight = adc_fault = false;
    sensor_frames = frames;
    adc_half = 0U;
    DMA_HandleTypeDef *d = a->DMA_Handle;
    __HAL_ADC_CLEAR_FLAG(a, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR | ADC_FLAG_RDY);
    SET_BIT(a->Instance->CFGR1, ADC_CFGR1_DMAEN);
    /* Pinned local HAL enable helper performs readiness timeout, never ADSTART.
     * Calibration is done once in board init; no internal sensor stabilization. */
    if (ADC_Enable(a) != HAL_OK) return false;
    a->State = HAL_ADC_STATE_REG_BUSY;
    a->ErrorCode = HAL_ADC_ERROR_NONE;
    d->XferHalfCpltCallback = dma_half;
    d->XferCpltCallback = dma_full;
    d->XferErrorCallback = dma_error;
    __HAL_ADC_ENABLE_IT(a, ADC_IT_OVR);
    if (HAL_DMA_Start_IT(d, (uintptr_t)&a->Instance->DR, (uintptr_t)raw_dma, count) != HAL_OK) return false;
    adc_accepting = true;
    return true;
}
static bool mux_error(DMA_HandleTypeDef *d)
{
    return d->DMAmuxChannelStatus != NULL &&
        READ_BIT(d->DMAmuxChannelStatus->CSR, d->DMAmuxChannelStatusMask) != 0U;
}
static bool adc_trigger(ADC_HandleTypeDef *a)
{
    if (a != sensor_adc || !adc_accepting || adc_inflight || adc_fault ||
        !LL_ADC_IsEnabled(a->Instance) ||
        READ_BIT(a->Instance->CR, ADC_CR_ADSTP | ADC_CR_ADDIS) != 0U ||
        a->DMA_Handle->State != HAL_DMA_STATE_BUSY ||
        READ_BIT(a->DMA_Handle->Instance->CCR, DMA_CCR_EN) == 0U || mux_error(a->DMA_Handle) ||
        LL_ADC_REG_IsConversionOngoing(a->Instance) ||
        READ_BIT(a->Instance->ISR, ADC_FLAG_OVR) != 0U ||
        __HAL_DMA_GET_FLAG(a->DMA_Handle, __HAL_DMA_GET_TE_FLAG_INDEX(a->DMA_Handle)) != 0U ||
        __HAL_DMA_GET_FLAG(a->DMA_Handle, __HAL_DMA_GET_HT_FLAG_INDEX(a->DMA_Handle)) != 0U ||
        __HAL_DMA_GET_FLAG(a->DMA_Handle, __HAL_DMA_GET_TC_FLAG_INDEX(a->DMA_Handle)) != 0U) return false;
    __HAL_ADC_CLEAR_FLAG(a, ADC_FLAG_EOC | ADC_FLAG_EOS);
    adc_inflight = true;
    LL_ADC_REG_StartConversion(a->Instance);
    return true;
}
static bool adc_stop(ADC_HandleTypeDef *a)
{
    if (a != sensor_adc) return false;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    adc_accepting = adc_inflight = false;
    __HAL_ADC_DISABLE_IT(a, ADC_IT_OVR);
    DMA_HandleTypeDef *d = a->DMA_Handle;
    if (LL_ADC_REG_IsConversionOngoing(a->Instance)) {
        LL_ADC_REG_StopConversion(a->Instance);
        if (LL_ADC_REG_IsConversionOngoing(a->Instance)) {
            __set_PRIMASK(mask); return false;
        }
    }
    if (READ_BIT(a->Instance->CR, ADC_CR_ADSTP | ADC_CR_ADDIS) != 0U) {
        __set_PRIMASK(mask); return false;
    }
    if (LL_ADC_IsEnabled(a->Instance)) {
        LL_ADC_Disable(a->Instance);
        if (LL_ADC_IsEnabled(a->Instance) || READ_BIT(a->Instance->CR, ADC_CR_ADDIS) != 0U) {
            __set_PRIMASK(mask); return false;
        }
    }
    /* HAL DMA abort is synchronous register cleanup; it has no polling wait. */
    if (d->State == HAL_DMA_STATE_BUSY) (void)HAL_DMA_Abort(d);
    CLEAR_BIT(d->Instance->CCR, DMA_CCR_EN | DMA_CCR_HTIE | DMA_CCR_TCIE | DMA_CCR_TEIE);
    __HAL_DMA_CLEAR_FLAG(d, __HAL_DMA_GET_GI_FLAG_INDEX(d));
    if (d->DMAmuxChannelStatus != NULL)
        d->DMAmuxChannelStatus->CFR = d->DMAmuxChannelStatusMask;
    CLEAR_BIT(a->Instance->CFGR1, ADC_CFGR1_DMAEN);
    __HAL_ADC_CLEAR_FLAG(a, ADC_FLAG_EOC | ADC_FLAG_EOS | ADC_FLAG_OVR | ADC_FLAG_RDY);
    HAL_NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    HAL_NVIC_ClearPendingIRQ(ADC1_IRQn);
    sensor_frames = NULL;
    adc_fault = false;
    a->State = HAL_ADC_STATE_READY;
    a->ErrorCode = HAL_ADC_ERROR_NONE;
    __set_PRIMASK(mask);
    return true;
}
static const Sensors_AcquisitionOps sensor_ops = {adc_arm, adc_trigger, adc_stop};

static void publish_adc(ADC_HandleTypeDef *a, unsigned half)
{
    if (a != sensor_adc || !adc_accepting || !adc_inflight || adc_fault) return;
    if (half != adc_half || mux_error(a->DMA_Handle) || LL_ADC_REG_IsConversionOngoing(a->Instance) ||
        READ_BIT(a->Instance->ISR, ADC_FLAG_OVR) != 0U ||
        __HAL_DMA_GET_FLAG(a->DMA_Handle, __HAL_DMA_GET_TE_FLAG_INDEX(a->DMA_Handle)) != 0U ||
        __HAL_DMA_GET_COUNTER(a->DMA_Handle) != (half == 0U ? SENSORS_FRAME_WORD_COUNT : SENSORS_DMA_WORD_COUNT)) {
        HAL_ADC_ErrorCallback(a); return;
    }
    unsigned base = half * SENSORS_FRAME_WORD_COUNT;
    sensor_frames[base + SENSORS_FRAME_BATTERY_INDEX] = raw_dma[base + FIRMWARE_HW_ADC_BATTERY_INDEX];
    sensor_frames[base + SENSORS_FRAME_TIP_INDEX] = raw_dma[base + FIRMWARE_HW_ADC_TIP_INDEX];
    if (half == 0U) Sensors_OnDmaHalf(a, SENSORS_CHANNEL_ALL);
    else Sensors_OnDmaFull(a, SENSORS_CHANNEL_ALL);
    adc_half ^= 1U;
    adc_inflight = false;
}
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *a) { publish_adc(a, 0U); }
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *a) { publish_adc(a, 1U); }
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *a)
{
    if (a != sensor_adc || !adc_accepting) return;
    adc_fault = true;
    Sensors_OnDmaError(a);
}

static Bus *find_bus(I2C_HandleTypeDef *h)
{
    if (h != NULL && h == charger_bus.handle) return &charger_bus;
    if (h != NULL && h == motor_bus.handle) return &motor_bus;
    return NULL;
}
static bool bus_stop(I2C_HandleTypeDef *h);
static bool bus_start(I2C_HandleTypeDef *h, uint16_t address, uint8_t reg, uint8_t *data, bool write)
{
    Bus *b = find_bus(h);
    if (b == NULL || b->aborting || b->accepting || h->State != HAL_I2C_STATE_READY ||
        address != b->address || data == NULL) return false;
    /* Even a successful prior transfer may have queued IRQs. Drain while
     * HAL READY before publishing acceptance for the next buffer. */
    if (!bus_stop(h)) return false;
    b->write = write;
    b->accepting = true;
    HAL_StatusTypeDef status = write ?
        HAL_I2C_Mem_Write_IT(h, address, reg, I2C_MEMADD_SIZE_8BIT, data, 1U) :
        HAL_I2C_Mem_Read_IT(h, address, reg, I2C_MEMADD_SIZE_8BIT, data, 1U);
    if (status != HAL_OK) b->accepting = false;
    return status == HAL_OK;
}
static bool bus_read(I2C_HandleTypeDef *h,uint16_t a,uint8_t r,uint8_t *v)
{ return bus_start(h,a,r,v,false); }
static bool bus_write(I2C_HandleTypeDef *h,uint16_t a,uint8_t r,uint8_t *v)
{ return bus_start(h,a,r,v,true); }
static bool bus_stop(I2C_HandleTypeDef *h)
{
    Bus *b = find_bus(h);
    if (b == NULL) return false;
    b->accepting = false;
    if (!b->aborting && h->State != HAL_I2C_STATE_READY &&
        (h->Mode == HAL_I2C_MODE_MASTER || h->Mode == HAL_I2C_MODE_MEM)) {
        b->aborting = true;
        b->abort_started = HAL_GetTick();
        (void)HAL_I2C_Master_Abort_IT(h, b->address);
        return false;
    }
    if (b->aborting && h->State != HAL_I2C_STATE_READY &&
        (uint32_t)(HAL_GetTick() - b->abort_started) < FIRMWARE_HW_I2C_ABORT_TIMEOUT_MS) return false;
    /* Dedicated bus: detach callbacks/buffer atomically. PE reset also drains
     * an abort with no STOP (held bus), preserving timing/filter configuration.
     * Local LL documents >=3 APB clocks with PE low; APB equals CPU here. */
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    CLEAR_BIT(h->Instance->CR1, I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_ADDRIE |
        I2C_CR1_NACKIE | I2C_CR1_STOPIE | I2C_CR1_TCIE | I2C_CR1_ERRIE |
        I2C_CR1_TXDMAEN | I2C_CR1_RXDMAEN);
    __HAL_I2C_DISABLE(h);
    (void)h->Instance->CR1;
    __DSB(); __NOP(); __NOP(); __NOP();
    h->Instance->CR2 = 0U;
    __HAL_I2C_CLEAR_FLAG(h, I2C_FLAG_STOPF | I2C_FLAG_AF | I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR);
    h->XferISR = NULL;
    h->pBuffPtr = NULL;
    h->XferCount = h->XferSize = h->PreviousState = 0U;
    h->Mode = HAL_I2C_MODE_NONE;
    h->State = HAL_I2C_STATE_READY;
    h->ErrorCode = HAL_I2C_ERROR_NONE;
    h->Lock = HAL_UNLOCKED;
    HAL_NVIC_ClearPendingIRQ(h->Instance == I2C1 ? I2C1_IRQn : I2C2_IRQn);
    __HAL_I2C_ENABLE(h);
    b->aborting = false;
    __set_PRIMASK(mask);
    return true;
}
static const I2C_DeviceOps bus_ops = {bus_read, bus_write, bus_stop};
static void bus_complete(I2C_HandleTypeDef *h, bool write, bool error)
{
    Bus *b = find_bus(h);
    if (b == NULL || !b->accepting || b->aborting || (!error && write != b->write)) return;
    b->accepting = false;
    if (b == &charger_bus) {
        if (error) MP2724_OnError(h);
        else if (write) MP2724_OnWriteComplete(h);
        else MP2724_OnReadComplete(h);
    } else {
        if (error) DRV2624_OnError(h);
        else if (write) DRV2624_OnWriteComplete(h);
        else DRV2624_OnReadComplete(h);
    }
}
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *h) { bus_complete(h,false,false); }
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *h) { bus_complete(h,true,false); }
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *h) { bus_complete(h,false,true); }
/* Abort owns no terminal driver callback. stop_quiescent observes HAL READY. */
void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *h) { (void)h; }

static void heater_duty(void *context, uint32_t duty)
{
    TIM_HandleTypeDef *t = context;
    if (t == NULL || t != heater_timer) return;
    if (duty > HEATER_DUTY_SCALE) duty = HEATER_DUTY_SCALE;
    uint32_t compare = (uint32_t)((uint64_t)(__HAL_TIM_GET_AUTORELOAD(t) + 1U) * duty / HEATER_DUTY_SCALE);
    __HAL_TIM_SET_COMPARE(t, TIM_CHANNEL_1, compare);
    /* OC1 preload must not leave a shutdown duty electrically active. */
    if (duty == 0U) (void)HAL_TIM_GenerateEvent(t, TIM_EVENTSOURCE_UPDATE);
}
static const Heater_PwmOps pwm_ops = {heater_duty};
void FirmwareHW_Init(App_Bindings *b, ADC_HandleTypeDef *adc,
                     I2C_HandleTypeDef *charger, I2C_HandleTypeDef *motor, TIM_HandleTypeDef *heater)
{
    if (b == NULL) return;
    memset(b, 0, sizeof *b);
    sensor_adc = NULL;
    sensor_frames = NULL;
    adc_accepting = adc_inflight = adc_fault = false;
    heater_timer = NULL;
    charger_bus = (Bus){0}; motor_bus = (Bus){0};
    if (adc_config_valid(adc) && HAL_ADCEx_Calibration_Start(adc) == HAL_OK) {
        sensor_adc = b->adc = adc;
        b->sensors = &sensor_ops;
    }
    if (charger != NULL && charger->Instance == I2C1 && charger->Init.AddressingMode == I2C_ADDRESSINGMODE_7BIT) {
        charger_bus = (Bus){.handle=charger,.address=MP2724_I2C_ADDRESS_7BIT << 1U};
        b->charger_i2c = charger; b->charger_transport = &bus_ops;
    }
    if (motor != NULL && motor->Instance == I2C2 && motor->Init.AddressingMode == I2C_ADDRESSINGMODE_7BIT) {
        motor_bus = (Bus){.handle=motor,.address=DRV2624_I2C_ADDRESS_7BIT << 1U};
        b->vibration_i2c = motor; b->vibration_transport = &bus_ops;
    }
    if (heater != NULL && heater->Instance == TIM1 &&
        READ_BIT(heater->Instance->CCMR1, TIM_CCMR1_OC1M) == TIM_OCMODE_PWM1 &&
        READ_BIT(heater->Instance->CCER, TIM_CCER_CC1P) == 0U &&
        READ_BIT(heater->Instance->CR1, TIM_CR1_DIR | TIM_CR1_CMS) == 0U &&
        __HAL_TIM_GET_AUTORELOAD(heater) < UINT16_MAX) {
        __HAL_TIM_SET_COMPARE(heater, TIM_CHANNEL_1, 0U);
        (void)HAL_TIM_GenerateEvent(heater, TIM_EVENTSOURCE_UPDATE);
        if (HAL_TIM_PWM_Start(heater, TIM_CHANNEL_1) == HAL_OK) {
            heater_timer = heater;
            b->heater_pwm = &pwm_ops; b->heater_context = heater;
        }
    }
}
