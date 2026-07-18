/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "can.h"

/* USER CODE BEGIN 0 */
#include <stdbool.h>
#include <stdint.h>
#include "pinecan.h"
#include "dronecan_msgs.h"
#include "ardupilot.indication.NotifyState.h"

/*
 * PineCAN state — can.c is the single owner of all PineCAN interaction for
 * this target, mirroring efs-pinecan's single-servo driver reference (its
 * can.c owns initCAN(), the CanardInstance/NodeStatus, and the RX handler
 * body). Handler registration is declared in pinecan_handlers.h via
 * RX_HANDLER_LIST; the handler body is defined below.
 */
static CanardInstance canard;
static struct uavcan_protocol_NodeStatus nodeStatus;
static bool pinecan_initialized = false;

/*
 * Async cache: written by handleNotifyState() (interrupt context), read by
 * canGetLatestVehicleState() (main loop, via CANManager::getLatestVehicleState).
 * volatile prevents the compiler from hoisting the poll read out of the loop.
 * 64-bit access is not atomic on Cortex-M4, so reads/writes are bracketed
 * with a brief IRQ critical section to avoid torn values.
 */
static volatile uint64_t latest_vehicle_state = 0;

/* USER CODE END 0 */

CAN_HandleTypeDef hcan1;

/* CAN1 init function */
void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 12;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_2TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN1 GPIO Configuration
    PA11     ------> CAN1_RX
    PA12     ------> CAN1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* CAN1 interrupt Init */
    HAL_NVIC_SetPriority(CAN1_RX0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(CAN1_RX0_IRQn);
  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN1 GPIO Configuration
    PA11     ------> CAN1_RX
    PA12     ------> CAN1_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(CAN1_RX0_IRQn);
  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/*
 * initCAN
 *
 * Builds a PinecanInit from the CubeMX-generated hcan1, sets NodeStatus
 * defaults, and calls pinecanInit exactly once. Mirrors the single-servo
 * driver reference's initCAN(). On failure, leaves pinecan_initialized
 * false so canService() never calls pinecan1ms().
 */
PineCAN_Status initCAN(void)
{
    nodeStatus.health = UAVCAN_PROTOCOL_NODESTATUS_HEALTH_OK;
    nodeStatus.mode   = UAVCAN_PROTOCOL_NODESTATUS_MODE_OPERATIONAL;
    nodeStatus.sub_mode = 0;
    nodeStatus.vendor_specific_status_code = 0;

    PinecanInit initParams = {
        .hcan       = &hcan1,
        .canard     = &canard,
        .nodeStatus = &nodeStatus
    };

    PineCAN_Status result = pinecanInit(&initParams);
    pinecan_initialized = (result == PINECAN_OK);
    return result;
}

/*
 * canService
 *
 * Called from the main loop each iteration. Gates pinecan1ms() to fire
 * once per HAL_GetTick() change (once per millisecond). No-op until
 * initCAN() has succeeded.
 */
void canService(void)
{
    static uint32_t lastTick = 0;

    if (!pinecan_initialized) {
        return;
    }

    uint32_t now = HAL_GetTick();
    if (now != lastTick) {
        lastTick = now;
        pinecan1ms(); /* TX-queue drain + stale-transfer cleanup + NodeStatus */
    }
}

/*
 * handleNotifyState
 *
 * Registered via pinecan_handlers.h (RX_HANDLER_LIST) and dispatched by
 * PineCAN's onTransferReceived (pinecanCommon.c) when a complete
 * ardupilot.indication.NotifyState broadcast transfer is received.
 *
 * Thin handler — no mapping logic:
 *   1. Decode the transfer payload into a NotifyState struct.
 *   2. On success, cache vehicle_state directly (plain C write).
 *   3. On decode failure, discard silently — cache is unchanged.
 *
 * Plain C, no extern "C" needed — can.c is a C translation unit, and
 * pinecan_handlers.h already declares this prototype for both C and C++
 * includers via its own extern "C" guard.
 */
void handleNotifyState(CanardInstance *ins, CanardRxTransfer *transfer)
{
    (void)ins;

    struct ardupilot_indication_NotifyState decoded = {0};

    /* ardupilot_indication_NotifyState_decode returns 0 on success,
     * non-zero on failure (DSDL convention). */
    if (ardupilot_indication_NotifyState_decode(transfer, &decoded) != 0) {
        return; /* decode failure — discard, cache unchanged */
    }

    __disable_irq();
    latest_vehicle_state = decoded.vehicle_state;
    __enable_irq();
}

/*
 * canGetLatestVehicleState
 *
 * Parameterless getter polled by the main loop (via
 * CANManager::getLatestVehicleState). Returns 0 before any message has
 * been received. Performs no mapping.
 */
uint64_t canGetLatestVehicleState(void)
{
    uint64_t v;
    __disable_irq();
    v = latest_vehicle_state;
    __enable_irq();
    return v;
}

/* USER CODE END 1 */
