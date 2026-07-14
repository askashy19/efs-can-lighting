/*
 * pinecan_handlers.c
 *
 * Target-specific PineCAN RX handler implementations for efs-can-lighting.
 * Handler registration is declared in Core/Inc/pinecan_handlers.h via
 * RX_HANDLER_LIST / REGISTER_RX_HANDLER and compiled into pinecanCommon.c.
 *
 * This file contains ONLY the handler body. All mapping logic lives in
 * CANManager::mapVehicleState (can_manager.cpp).
 */

#include "pinecan_handlers.h"
#include "canard.h"
#include "ardupilot.indication.NotifyState.h"
#include "can_manager.hpp"

/*
 * handleNotifyState
 *
 * Called by PineCAN (via libcanard dispatch) when a complete
 * ardupilot.indication.NotifyState broadcast transfer is received.
 *
 * Responsibilities (thin handler — no mapping logic):
 *   1. Decode the transfer payload into a NotifyState struct.
 *   2. On success, cache vehicle_state via CANManager::cacheLatestState().
 *   3. On decode failure, discard silently — cache is unchanged.
 */
void handleNotifyState(CanardInstance *ins, CanardRxTransfer *transfer)
{
    (void)ins;

    struct ardupilot_indication_NotifyState decoded = {};

    /* ardupilot_indication_NotifyState_decode returns 0 on success,
     * non-zero on failure (DSDL convention). */
    if (ardupilot_indication_NotifyState_decode(transfer, &decoded) != 0) {
        return; /* decode failure — discard, cache unchanged */
    }

    CANManager_cacheLatestState(decoded.vehicle_state);
}
