/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_usbx_host.c
  * @author  MCD Application Team
  * @brief   USBX host applicative file
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
#include "app_usbx_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ux_host_class_storage.h"
#include "ux_hcd_stm32.h"
#include <stdio.h>
#include <string.h>
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
static TX_THREAD ux_host_app_thread;
/* USER CODE BEGIN PV */
UX_HOST_CLASS_STORAGE *usb_storage_instance = UX_NULL;
UX_HOST_CLASS         *usb_host_class        = UX_NULL;
volatile uint8_t       usb_disconnected      = 0;
extern HCD_HandleTypeDef hhcd_USB_DRD_FS;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID app_ux_host_thread_entry(ULONG thread_input);
static UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance);
static VOID ux_host_error_callback(UINT system_level, UINT system_context, UINT error_code);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
  * @brief  Application USBX Host Initialization.
  * @param  memory_ptr: memory pointer
  * @retval status
  */
UINT MX_USBX_Host_Init(VOID *memory_ptr)
{
  UINT ret = UX_SUCCESS;
  UCHAR *pointer;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

  /* USER CODE BEGIN MX_USBX_Host_Init0 */

  /* USER CODE END MX_USBX_Host_Init0 */

  /* Allocate the stack for USBX Memory */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer,
                       USBX_HOST_MEMORY_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_ALLOCATE_STACK_ERROR */
    return TX_POOL_ERROR;
    /* USER CODE END USBX_ALLOCATE_STACK_ERROR */
  }

  /* Initialize USBX Memory */
  if (ux_system_initialize(pointer, USBX_HOST_MEMORY_STACK_SIZE, UX_NULL, 0) != UX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_SYSTEM_INITIALIZE_ERROR */
    return UX_ERROR;
    /* USER CODE END USBX_SYSTEM_INITIALIZE_ERROR */
  }

  /* Install the host portion of USBX */
  if (ux_host_stack_initialize(ux_host_event_callback) != UX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_HOST_INITIALIZE_ERROR */
    return UX_ERROR;
    /* USER CODE END USBX_HOST_INITIALIZE_ERROR */
  }

  /* Register a callback error function */
  ux_utility_error_callback_register(&ux_host_error_callback);

  /* Initialize the host storage class */
  if (ux_host_stack_class_register(_ux_system_host_class_storage_name,
                                   ux_host_class_storage_entry) != UX_SUCCESS)
  {
    /* USER CODE BEGIN USBX_HOST_STORAGE_REGISTER_ERROR */
    return UX_ERROR;
    /* USER CODE END USBX_HOST_STORAGE_REGISTER_ERROR */
  }

  /* Allocate the stack for host application main thread */
  if (tx_byte_allocate(byte_pool, (VOID **) &pointer, UX_HOST_APP_THREAD_STACK_SIZE,
                       TX_NO_WAIT) != TX_SUCCESS)
  {
    /* USER CODE BEGIN MAIN_THREAD_ALLOCATE_STACK_ERROR */
    return TX_POOL_ERROR;
    /* USER CODE END MAIN_THREAD_ALLOCATE_STACK_ERROR */
  }

  /* Create the host application main thread */
  if (tx_thread_create(&ux_host_app_thread, UX_HOST_APP_THREAD_NAME, app_ux_host_thread_entry,
                       0, pointer, UX_HOST_APP_THREAD_STACK_SIZE, UX_HOST_APP_THREAD_PRIO,
                       UX_HOST_APP_THREAD_PREEMPTION_THRESHOLD, UX_HOST_APP_THREAD_TIME_SLICE,
                       UX_HOST_APP_THREAD_START_OPTION) != TX_SUCCESS)
  {
    /* USER CODE BEGIN MAIN_THREAD_CREATE_ERROR */
    return TX_THREAD_ERROR;
    /* USER CODE END MAIN_THREAD_CREATE_ERROR */
  }

  /* USER CODE BEGIN MX_USBX_Host_Init1 */
  if (ux_host_stack_hcd_register((UCHAR *)"STM32 HCD",
                                  _ux_hcd_stm32_initialize,
                                  USB_DRD_BASE,
                                  (ULONG)&hhcd_USB_DRD_FS) != UX_SUCCESS)
  {
    printf("HCD register FAIL\r\n");
    return UX_ERROR;
  }
  printf("HCD registered\r\n");
  /* HAL_HCD_Start moved to host thread - safer after scheduler starts */
  /* USER CODE END MX_USBX_Host_Init1 */

  return ret;
}

/**
  * @brief  Function implementing app_ux_host_thread_entry.
  * @param  thread_input: User thread input parameter.
  * @retval none
  */
static VOID app_ux_host_thread_entry(ULONG thread_input)
{
  /* USER CODE BEGIN app_ux_host_thread_entry */
  TX_PARAMETER_NOT_USED(thread_input);

  UX_HOST_CLASS_STORAGE_MEDIA *storage_media;
  FX_MEDIA                    *media;
  FX_FILE                      log_file;
  char                         batch[512];
  int                          offset;
  uint32_t                     sample_idx = 0;
  UINT                         status;

  printf("USB Host thread started\r\n");
  HAL_HCD_Start(&hhcd_USB_DRD_FS);   /* start USB host port now that scheduler is running */
  printf("USB Host started\r\n");

  while (1)
  {
    /* ── Wait for USB MSC device ── */
    while (usb_storage_instance == UX_NULL)
    {
      tx_thread_sleep(10);
    }

    printf("USB MSC ready - opening media\r\n");

    if (usb_host_class == UX_NULL) { usb_storage_instance = UX_NULL; continue; }

    /* ── Media is already mounted by USBX when UX_DEVICE_INSERTION fires ── */
    storage_media = (UX_HOST_CLASS_STORAGE_MEDIA *)usb_host_class->ux_host_class_media;

    if (storage_media == UX_NULL ||
        storage_media->ux_host_class_storage_media_status != UX_HOST_CLASS_STORAGE_MEDIA_MOUNTED)
    {
      printf("Media not ready (status=%lu)\r\n",
             storage_media ? (ULONG)storage_media->ux_host_class_storage_media_status : 99UL);
      usb_storage_instance = UX_NULL;
      usb_host_class       = UX_NULL;
      continue;
    }

    media = &storage_media->ux_host_class_storage_media;

    /* ── Create CSV file (ignore FX_ALREADY_CREATED) ── */
    fx_file_create(media, "log.csv");

    status = fx_file_open(media, &log_file, "log.csv", FX_OPEN_FOR_WRITE);
    if (status != FX_SUCCESS)
    {
      printf("File open FAIL: 0x%lX\r\n", (ULONG)status);
      usb_storage_instance = UX_NULL;
      continue;
    }

    /* ── Write CSV header ── */
    char *hdr = "timestamp_ms,voltage_v,current_a,flow_lpm\r\n";
    fx_file_write(&log_file, hdr, (ULONG)strlen(hdr));
    printf("Logging started\r\n");

    offset = 0;

    /* ── Logging loop ── */
    while (!usb_disconnected)
    {
      uint32_t ts       = HAL_GetTick();
      float    voltage  = (float)(sample_idx % 400)  / 10.0f;   /* 0.0 – 39.9 V  */
      float    current  = (float)(sample_idx % 6000) / 10.0f;   /* 0.0 – 599.9 A */
      float    flow     = (float)(sample_idx % 50)   / 10.0f;   /* 0.0 – 4.9 LPM */
      sample_idx++;

      offset += snprintf(batch + offset, (int)sizeof(batch) - offset,
                         "%lu,%.1f,%.1f,%.1f\r\n",
                         ts, voltage, current, flow);

      if (offset >= 480)
      {
        fx_file_write(&log_file, batch, (ULONG)offset);
        fx_media_flush(media);
        offset = 0;
      }

      tx_thread_sleep(10);  /* 100 ms per sample (ThreadX tick = 10 ms) */
    }

    /* ── Flush remaining data before removing device ── */
    if (offset > 0)
    {
      fx_file_write(&log_file, batch, (ULONG)offset);
    }

    fx_file_close(&log_file);
    fx_media_flush(media);
    printf("File closed - USB safe to remove\r\n");
  }
  /* USER CODE END app_ux_host_thread_entry */
}

/**
  * @brief  ux_host_event_callback
  *         This callback is invoked to notify application of instance changes.
  * @param  event: event code.
  * @param  current_class: Pointer to class.
  * @param  current_instance: Pointer to class instance.
  * @retval status
  */
UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance)
{
  UINT status = UX_SUCCESS;

  /* USER CODE BEGIN ux_host_event_callback0 */
  /* USER CODE END ux_host_event_callback0 */

  switch (event)
  {
    case UX_DEVICE_INSERTION:

      /* USER CODE BEGIN UX_DEVICE_INSERTION */
      usb_storage_instance = (UX_HOST_CLASS_STORAGE *)current_instance;
      usb_host_class       = current_class;
      usb_disconnected     = 0;
      printf("USB MSC Inserted\r\n");
      /* USER CODE END UX_DEVICE_INSERTION */

      break;

    case UX_DEVICE_REMOVAL:

      /* USER CODE BEGIN UX_DEVICE_REMOVAL */
      if (usb_storage_instance != UX_NULL &&
          (UX_HOST_CLASS_STORAGE *)current_instance == usb_storage_instance)
      {
        usb_disconnected     = 1;
        usb_storage_instance = UX_NULL;
        usb_host_class       = UX_NULL;
        printf("USB MSC Removed\r\n");
      }
      /* USER CODE END UX_DEVICE_REMOVAL */

      break;

    case UX_DEVICE_CONNECTION:

      /* USER CODE BEGIN UX_DEVICE_CONNECTION */

      /* USER CODE END UX_DEVICE_CONNECTION */

      break;

    case UX_DEVICE_DISCONNECTION:

      /* USER CODE BEGIN UX_DEVICE_DISCONNECTION */

      /* USER CODE END UX_DEVICE_DISCONNECTION */

      break;

    default:

      /* USER CODE BEGIN EVENT_DEFAULT */

      /* USER CODE END EVENT_DEFAULT */

      break;
  }

  /* USER CODE BEGIN ux_host_event_callback1 */

  /* USER CODE END ux_host_event_callback1 */

  return status;
}

/**
  * @brief ux_host_error_callback
  *         This callback is invoked to notify application of error changes.
  * @param  system_level: system level parameter.
  * @param  system_context: system context code.
  * @param  error_code: error event code.
  * @retval Status
  */
VOID ux_host_error_callback(UINT system_level, UINT system_context, UINT error_code)
{
  /* USER CODE BEGIN ux_host_error_callback0 */
  /* USER CODE END ux_host_error_callback0 */

  switch (error_code)
  {
    case UX_DEVICE_ENUMERATION_FAILURE:

      /* USER CODE BEGIN UX_DEVICE_ENUMERATION_FAILURE */

      /* USER CODE END UX_DEVICE_ENUMERATION_FAILURE */

      break;

    case  UX_NO_DEVICE_CONNECTED:

      /* USER CODE BEGIN UX_NO_DEVICE_CONNECTED */

      /* USER CODE END UX_NO_DEVICE_CONNECTED */

      break;

    default:

      /* USER CODE BEGIN ERROR_DEFAULT */

      /* USER CODE END ERROR_DEFAULT */

      break;
  }

  /* USER CODE BEGIN ux_host_error_callback1 */

  /* USER CODE END ux_host_error_callback1 */
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */
