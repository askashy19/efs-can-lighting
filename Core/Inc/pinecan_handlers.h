/*
 * pinecan_handlers.h
 *
 * Target-provided RX handler registration for efs-can-lighting.
 * Included by pinecanCommon.c (PineCAN library) via #include "pinecan_handlers.h".
 * PineCAN uses the RX_HANDLER_LIST macro to register handlers at compile time —
 * no changes to the PineCAN source tree are needed.
 *
 * Header-only, matching the efs-pinecan single-servo driver reference exactly
 * (efs-pinecan/targets/single_servo_driver/rev1/Core/Inc/pinecan_handlers.h):
 * this file contains ONLY the registration macro and the handler prototype.
 * There is NO accompanying pinecan_handlers.c — the handler body, PineCAN
 * init (initCAN), and the 1 ms service (canService) all live in
 * Core/Src/can.c (in its USER CODE sections), exactly as the single-servo
 * target defines handleArrayCommand and initCAN in its own can.c.
 *
 * REGISTER_RX_HANDLER(TYPE, HANDLER, TRANSFER_KIND)
 *   TYPE          — base name of the DSDL type (TYPE_ID and TYPE_SIGNATURE must exist)
 *   HANDLER       — function called when a matching transfer is received
 *   TRANSFER_KIND — one of: REQUEST, RESPONSE, BROADCAST
 */

#pragma once
#include "canard.h"

#define RX_HANDLER_LIST \
    REGISTER_RX_HANDLER(ARDUPILOT_INDICATION_NOTIFYSTATE, handleNotifyState, BROADCAST)

#ifdef __cplusplus
extern "C" {
#endif

/* Defined in Core/Src/can.c */
void handleNotifyState(CanardInstance *ins, CanardRxTransfer *transfer);

#ifdef __cplusplus
}
#endif
