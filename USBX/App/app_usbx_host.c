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
typedef enum
{
  USB_STATE_IDLE = 0,
  USB_STATE_CONNECTED,
  USB_STATE_LOGGING,
  USB_STATE_REMOVED
} USB_State_t;
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

/* ── Test instrumentation ── */
volatile USB_State_t usb_state        = USB_STATE_IDLE;
volatile uint32_t    missed_samples   = 0;
volatile uint32_t    reconnect_count  = 0;
volatile uint32_t    write_time_ms    = 0;
volatile uint32_t    total_written    = 0;
volatile int         buffer_level     = 0;
volatile uint32_t    last_ts          = 0;
volatile uint32_t    ts_errors        = 0;
         uint8_t     file_index       = 1;
volatile uint32_t    idle_count       = 0;
static   uint32_t    idle_snap        = 0;
static   uint32_t    session_start_ms = 0;
static   uint8_t     file_is_open     = 0;   /* tracks whether log_file is open */
static   FX_FILE     log_file;               /* persistent across sessions       */
static   FX_MEDIA   *active_media     = NULL;
static   uint32_t    idle_baseline    = 0;   /* idle ticks per 30s with no file I/O */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID app_ux_host_thread_entry(ULONG thread_input);
static UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance);
static VOID ux_host_error_callback(UINT system_level, UINT system_context, UINT error_code);
/* USER CODE BEGIN PFP */
static void logging_close(void);
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
  char                         batch[512];
  int                          offset;
  uint32_t                     sample_idx = 0;
  UINT                         status;
  char                         fname[16];

  printf("USB Host thread started\r\n");
  HAL_HCD_Start(&hhcd_USB_DRD_FS);
  printf("USB Host started\r\n");

  while (1)
  {
    /* ── Wait for USB MSC device ── */
    usb_state = USB_STATE_IDLE;
    while (usb_storage_instance == UX_NULL)
    {
      tx_thread_sleep(10);
    }
    usb_state = USB_STATE_CONNECTED;
    printf("USB MSC ready - opening media\r\n");

    if (usb_host_class == UX_NULL) { usb_storage_instance = UX_NULL; continue; }

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

    /* ── Find next unused log file index ── */
    file_index    = 1;
    total_written = 0;
    {
      FX_FILE probe;
      UINT    probe_status;
      while (file_index < 999)
      {
        snprintf(fname, sizeof(fname), "log_%03d.csv", file_index);
        memset(&probe, 0, sizeof(FX_FILE));
        probe_status = fx_file_open(media, &probe, fname, FX_OPEN_FOR_READ);
        if (probe_status == FX_NOT_FOUND)
          break;                         /* truly does not exist — use this index */
        if (probe_status == FX_SUCCESS)
          fx_file_close(&probe);         /* exists and opened cleanly — skip      */
        /* FX_ALREADY_OPEN also means file exists — skip                          */
        file_index++;
      }
    }
    /* ── Gracefully close any file left open from a previous session ── */
    logging_close();

    printf("New session file: %s\r\n", fname);
    fx_file_create(media, fname);
    status = fx_file_open(media, &log_file, fname, FX_OPEN_FOR_WRITE);
    if (status != FX_SUCCESS)
    {
      printf("File open FAIL: 0x%lX\r\n", (ULONG)status);
      usb_storage_instance = UX_NULL;
      continue;
    }
    active_media = media;
    file_is_open = 1;
    char *hdr = "timestamp_ms,voltage_v,current_a,flow_lpm\r\n";
    fx_file_write(&log_file, hdr, (ULONG)strlen(hdr));

    usb_state        = USB_STATE_LOGGING;
    offset           = 0;
    session_start_ms = HAL_GetTick();

    /* Capture true baseline: system is stable, USB connected, but no file I/O yet */
    {
      uint32_t t0 = idle_count;
      tx_thread_sleep(300);                        /* measure over 3 seconds      */
      idle_baseline = (idle_count - t0) * 10;     /* scale up to 30s window      */
    }
    idle_snap = idle_count;
    printf("Logging started -> %s  [FREQ=%d ticks] baseline=%lu\r\n",
           fname, TEST_FREQ, idle_baseline);

    /* ── Logging loop ── */
    while (!usb_disconnected)
    {
      uint32_t ts      = HAL_GetTick();
      float    voltage = (float)(sample_idx % 400)  / 10.0f;
      float    current = (float)(sample_idx % 6000) / 10.0f;
      float    flow    = (float)(sample_idx % 50)   / 10.0f;
      sample_idx++;

      /* T02: timestamp monotonicity */
      if (ts < last_ts) ts_errors++;
      last_ts = ts;

      /* T06: missed sample detection */
      int line_len = snprintf(NULL, 0, "%lu,%.1f,%.1f,%.1f\r\n", ts, voltage, current, flow);
      if (offset + line_len >= (int)sizeof(batch))
      {
        missed_samples++;
      }
      else
      {
        offset += snprintf(batch + offset, (int)sizeof(batch) - offset,
                           "%lu,%.1f,%.1f,%.1f\r\n", ts, voltage, current, flow);
      }
      buffer_level = offset;

      /* ── Batch flush ── */
      if (offset >= 480)
      {
        uint32_t t0 = HAL_GetTick();
        fx_file_write(&log_file, batch, (ULONG)offset);
        fx_media_flush(media);
        write_time_ms  = HAL_GetTick() - t0;
        total_written += (uint32_t)offset;
        offset         = 0;
        buffer_level   = 0;

        /* T09/T10: file rotation */
        if (total_written >= (uint32_t)FILE_ROTATE_SIZE_KB * 1024UL)
        {
          logging_close();
          file_index++;
          total_written = 0;
          snprintf(fname, sizeof(fname), "log_%03d.csv", file_index);
          fx_file_create(media, fname);
          fx_file_open(media, &log_file, fname, FX_OPEN_FOR_WRITE);
          active_media = media;
          file_is_open = 1;
          fx_file_write(&log_file, hdr, (ULONG)strlen(hdr));
          printf("Rotated -> %s\r\n", fname);
        }
      }

      /* T14: time-based telemetry — single compact line every TELEMETRY_INTERVAL_MS */
      {
        static uint32_t last_telem_ms = 0;
        uint32_t now = HAL_GetTick();
        if (now - last_telem_ms >= TELEMETRY_INTERVAL_MS)
        {
          last_telem_ms = now;
          ULONG64  free_space = 0;
          uint32_t elapsed_ms = now - session_start_ms;
          uint32_t throughput = elapsed_ms > 0 ? (total_written / (elapsed_ms / 1000 + 1)) : 0;
          uint32_t idle_delta  = idle_count - idle_snap;
          idle_snap            = idle_count;
          /* CPU% = fraction of 30s window NOT spent in idle thread.
           * idle_delta ticks / TELEMETRY_INTERVAL_MS ms gives idle rate.
           * We report idle% directly — easier to interpret.
           * idle_rate: ticks per ms in this window vs ticks per ms at system start. */
          /* Idle ratio: idle_delta vs baseline — lower ratio = higher CPU load */
          uint32_t idle_ratio_pct = (idle_baseline > 0)
                                    ? (uint32_t)((uint64_t)idle_delta * 100ULL / idle_baseline)
                                    : 0;
          if (idle_ratio_pct > 100) idle_ratio_pct = 100;
          uint32_t cpu_pct = 100 - idle_ratio_pct;
          fx_media_extended_space_available(media, &free_space);

          printf("[TELEM1] smp=%lu file=%s wkb=%lu free=%luGB reconn=%lu\r\n",
                 sample_idx, fname, total_written / 1024,
                 (ULONG)(free_space / (1024ULL * 1024ULL * 1024ULL)),
                 reconnect_count);
          printf("[TELEM2] wt=%lums tp=%luB/s buf=%d/512 miss=%lu tserr=%lu idle=%lu(ref=%lu)\r\n",
                 write_time_ms, throughput, buffer_level,
                 missed_samples, ts_errors, idle_delta, idle_baseline);
        }
      }

#if TEST_FREQ > 0
      tx_thread_sleep(TEST_FREQ);
#else
      tx_thread_relinquish();   /* yield to same-priority threads, no sleep */
#endif
    }

    /* ── Graceful disconnect: flush remaining batch then close ── */
    if (offset > 0 && file_is_open)
    {
      fx_file_write(&log_file, batch, (ULONG)offset);
      total_written += (uint32_t)offset;
      offset = 0;
    }
    logging_close();
    reconnect_count++;
    printf("USB safe to remove | %lu KB written | reconnects=%lu\r\n",
           total_written / 1024, reconnect_count);

    /* reset for next insertion */
    missed_samples = 0;
    ts_errors      = 0;
    last_ts        = 0;
    usb_disconnected = 0;
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
static void logging_close(void)
{
  if (!file_is_open) return;
  fx_file_close(&log_file);
  if (active_media != NULL) fx_media_flush(active_media);
  file_is_open  = 0;
  active_media  = NULL;
  usb_state     = USB_STATE_REMOVED;
  printf("File closed gracefully\r\n");
}
/* USER CODE END 1 */
