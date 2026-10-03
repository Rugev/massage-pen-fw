/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CHRG_I2C_SDA_Pin GPIO_PIN_9
#define CHRG_I2C_SDA_GPIO_Port GPIOB
#define LED_HEAT_3_Pin GPIO_PIN_14
#define LED_HEAT_3_GPIO_Port GPIOC
#define CHRG_INT_Pin GPIO_PIN_0
#define CHRG_INT_GPIO_Port GPIOA
#define TIP_HEAT_TEMP_SENS_Pin GPIO_PIN_1
#define TIP_HEAT_TEMP_SENS_GPIO_Port GPIOA
#define BAT_LED_R_Pin GPIO_PIN_4
#define BAT_LED_R_GPIO_Port GPIOA
#define BAT_LED_G_Pin GPIO_PIN_5
#define BAT_LED_G_GPIO_Port GPIOA
#define SYS_ON_Pin GPIO_PIN_6
#define SYS_ON_GPIO_Port GPIOA
#define SYS_PG_Pin GPIO_PIN_7
#define SYS_PG_GPIO_Port GPIOA
#define VBATT_SENS_Pin GPIO_PIN_0
#define VBATT_SENS_GPIO_Port GPIOB
#define TIP_HEAT_CTRL_Pin GPIO_PIN_8
#define TIP_HEAT_CTRL_GPIO_Port GPIOA
#define BUTTON_VIBRATION_Pin GPIO_PIN_9
#define BUTTON_VIBRATION_GPIO_Port GPIOA
#define BUTTON_HEAT_Pin GPIO_PIN_6
#define BUTTON_HEAT_GPIO_Port GPIOC
#define VIBR_I2C_SCL_Pin GPIO_PIN_11
#define VIBR_I2C_SCL_GPIO_Port GPIOA
#define VIBR_I2C_SDA_Pin GPIO_PIN_12
#define VIBR_I2C_SDA_GPIO_Port GPIOA
#define LED_HEAT_1_Pin GPIO_PIN_15
#define LED_HEAT_1_GPIO_Port GPIOA
#define LED_HEAT_2_Pin GPIO_PIN_3
#define LED_HEAT_2_GPIO_Port GPIOB
#define LED_VIBRATION_1_Pin GPIO_PIN_4
#define LED_VIBRATION_1_GPIO_Port GPIOB
#define BUTTON_PWR_ON_Pin GPIO_PIN_5
#define BUTTON_PWR_ON_GPIO_Port GPIOB
#define LED_VIBRATION_2_Pin GPIO_PIN_6
#define LED_VIBRATION_2_GPIO_Port GPIOB
#define LED_VIBRATION_3_Pin GPIO_PIN_7
#define LED_VIBRATION_3_GPIO_Port GPIOB
#define CHRG_I2C_SCL_Pin GPIO_PIN_8
#define CHRG_I2C_SCL_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
