#include "can_manager.hpp"
#include "can.h"

// ---------------------------------------------------------------------------
// CANManager owns NO PineCAN state. PineCAN init (initCAN), the 1 ms
// service (canService), the RX handler (handleNotifyState), and the async
// vehicle_state cache (canGetLatestVehicleState) all live in Core/Src/can.c,
// mirroring the efs-pinecan single-servo driver reference. The two methods
// below are thin forwarders so main.cpp has a single, stable call surface.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// CANManager::service                                              [1 ms tick]
// Forwards to canService() (Core/Src/can.c), which gates pinecan1ms() to
// fire once per millisecond and no-ops until initCAN() has succeeded.
// ---------------------------------------------------------------------------

void CANManager::service() {
    canService();
}

// ---------------------------------------------------------------------------
// CANManager::getLatestVehicleState                                [STEP 1]
// Forwards to canGetLatestVehicleState() (Core/Src/can.c), which owns the
// actual cache written by handleNotifyState. No parameters, no mapping.
// Pass the result to vehicleStateChanged().
// ---------------------------------------------------------------------------

uint64_t CANManager::getLatestVehicleState() {
    return canGetLatestVehicleState();
}

// ---------------------------------------------------------------------------
// CANManager::vehicleStateChanged                                  [STEP 2]
// Returns true if `raw_vehicle_state` differs from the previous call.
// First call always returns false — establishes the baseline so the initial
// cached value of 0 is not delivered as a spurious state change.
// ---------------------------------------------------------------------------

uint64_t CANManager::prev_data_ = 0;
bool     CANManager::have_prev_ = false;

bool CANManager::vehicleStateChanged(uint64_t raw_vehicle_state) {
    if (!have_prev_) {
        prev_data_ = raw_vehicle_state;
        have_prev_ = true;
        return false;
    }

    if (raw_vehicle_state != prev_data_) {
        prev_data_ = raw_vehicle_state;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// CANManager::mapVehicleState  (pure — no globals, no HAL, no PineCAN)   [STEP 3a]
// Converts a 64-bit vehicle_state bitmask into a LightingStateTransition.
// Called by interpretVehicleState. Safe to call directly in unit tests.
//
// Bit mapping (ArduPilot vehicle_state flags):
//   bit 23 IS_TAKING_OFF → TRANSITION_TAKEOFF  (3)  — highest priority
//   bit 22 IS_LANDING    → TRANSITION_LANDING  (6)
//   bit  2 FLYING        → TRANSITION_FLIGHT   (4)
//   bit  1 ARMED         → TRANSITION_STANDBY  (1)
//   bit  0 INITIALISING  → TRANSITION_STARTUP  (8)  — lowest priority
//   == 0                 → TRANSITION_GROUND   (0)
//   no mapped bit        → UNRECOGNIZED_STATE  (0xFF)
// ---------------------------------------------------------------------------

uint8_t CANManager::mapVehicleState(uint64_t vehicle_state) {
    if (vehicle_state == 0) {
        return static_cast<uint8_t>(TRANSITION_GROUND);
    }

    constexpr uint64_t MAPPED_BITS =
        (1ULL << 23) | (1ULL << 22) | (1ULL << 2) | (1ULL << 1) | (1ULL << 0);

    if ((vehicle_state & MAPPED_BITS) == 0) {
        return UNRECOGNIZED_STATE;
    }

    if (vehicle_state & (1ULL << 23)) return static_cast<uint8_t>(TRANSITION_TAKEOFF);
    if (vehicle_state & (1ULL << 22)) return static_cast<uint8_t>(TRANSITION_LANDING);
    if (vehicle_state & (1ULL <<  2)) return static_cast<uint8_t>(TRANSITION_FLIGHT);
    if (vehicle_state & (1ULL <<  1)) return static_cast<uint8_t>(TRANSITION_STANDBY);
    if (vehicle_state & (1ULL <<  0)) return static_cast<uint8_t>(TRANSITION_STARTUP);

    return UNRECOGNIZED_STATE;
}

// ---------------------------------------------------------------------------
// interpretVehicleState  (free function, declared in can_manager.hpp)     [STEP 3]
// Thin delegator to mapVehicleState. Call from the main loop when
// vehicleStateChanged() returns true.
// ---------------------------------------------------------------------------

uint8_t interpretVehicleState(uint64_t vehicle_state) {
    return CANManager::mapVehicleState(vehicle_state);
}
