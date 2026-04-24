/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_usbx_host.h
  * @author  MCD Application Team
  * @brief   USBX Host applicative header file
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
#ifndef __APP_USBX_HOST_H__
#define __APP_USBX_HOST_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ux_api.h"
#include "main.h"
#include "ux_host_msc.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
#define USBX_HOST_MEMORY_STACK_SIZE     24576

#define UX_HOST_APP_THREAD_STACK_SIZE   4096
#define UX_HOST_APP_THREAD_PRIO         10

/* USER CODE BEGIN EC */
/* ── Test frequency modes (tx_thread_sleep ticks, 1 tick = 10 ms) ──
 * TX_TIMER_TICKS_PER_SECOND = 100, so:
 *   100 ticks =  1 Hz  (1 sample/sec)
 *    10 ticks = 10 Hz  (10 samples/sec)
 *     1 tick  = 100 Hz (100 samples/sec, minimum ThreadX resolution)
 *     0       = max speed (no sleep, ~kHz range, stress only)
 * True 1 kHz requires bypassing ThreadX sleep — use TEST_FREQ_MAX for stress.
 */
#define TEST_FREQ_1HZ          100
#define TEST_FREQ_10HZ          10
#define TEST_FREQ_100HZ          1
#define TEST_FREQ_MAX            0   /* no sleep — stress/buffer test only */
#define TEST_FREQ               TEST_FREQ_10HZ   /* 100Hz needs 2-thread design (T12) */

/* File rotation threshold in KB (10240 = 10 MB, 51200 = 50 MB) */
#define FILE_ROTATE_SIZE_KB     10240

/* Telemetry print interval in milliseconds */
#define TELEMETRY_INTERVAL_MS    30000UL
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
UINT MX_USBX_Host_Init(VOID *memory_ptr);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

#ifndef UX_HOST_APP_THREAD_NAME
#define UX_HOST_APP_THREAD_NAME  "USBX App Host Main Thread"
#endif

#ifndef UX_HOST_APP_THREAD_PREEMPTION_THRESHOLD
#define UX_HOST_APP_THREAD_PREEMPTION_THRESHOLD  UX_HOST_APP_THREAD_PRIO
#endif

#ifndef UX_HOST_APP_THREAD_TIME_SLICE
#define UX_HOST_APP_THREAD_TIME_SLICE  TX_NO_TIME_SLICE
#endif

#ifndef UX_HOST_APP_THREAD_START_OPTION
#define UX_HOST_APP_THREAD_START_OPTION  TX_AUTO_START
#endif

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

#ifdef __cplusplus
}
#endif
#endif /* __APP_USBX_HOST_H__ */
