/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
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

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
static TX_THREAD idle_thread;
static UCHAR     idle_stack[512];
static TX_THREAD dummy_load_thread;
static UCHAR     dummy_load_stack[512];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
static VOID idle_thread_entry(ULONG arg);
static VOID dummy_load_entry(ULONG arg);
/* USER CODE END PFP */

/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */
  /* USER CODE BEGIN App_ThreadX_Init */
  tx_thread_create(&idle_thread, "Idle", idle_thread_entry, 0,
                   idle_stack, sizeof(idle_stack),
                   31, 31, TX_NO_TIME_SLICE, TX_AUTO_START);

  /* Dummy load thread: priority 15 (below USBX@8-9, below logging@10, above idle@31)
   * Burns ~33% CPU to validate CPU% measurement without starving USB transfers */
  tx_thread_create(&dummy_load_thread, "DummyLoad", dummy_load_entry, 0,
                   dummy_load_stack, sizeof(dummy_load_stack),
                   15, 15, TX_NO_TIME_SLICE, TX_AUTO_START);
  /* USER CODE END App_ThreadX_Init */

  return ret;
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
static VOID idle_thread_entry(ULONG arg)
{
  extern volatile uint32_t idle_count;
  TX_PARAMETER_NOT_USED(arg);
  while (1) { idle_count++; }
}

static VOID dummy_load_entry(ULONG arg)
{
  TX_PARAMETER_NOT_USED(arg);
  /* Burns CPU for 5ms then sleeps 5ms → ~50% load at priority 5 */
  while (1)
  {
    uint32_t t = HAL_GetTick();
    while (HAL_GetTick() - t < 5) { __NOP(); }   /* active 5 ms */
    tx_thread_sleep(1);                            /* sleep 10 ms (1 tick) */
  }
}
/* USER CODE END 1 */
