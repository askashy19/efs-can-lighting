#include "can_manager.hpp"
#include "ardupilot.indication.NotifyState.h"
#include "pinecan.h"

// ---------------------------------------------------------------------------
// Singleton storage
// ---------------------------------------------------------------------------

union MaybeCANManager {
    struct {} uninitialized;
    CANManager manager;

    MaybeCANManager() {}
    ~MaybeCANManager() {}
} can_manager_instance;

static bool can_manager_initialized = false;

// ---------------------------------------------------------------------------
// pinecan1ms_flag
// Set to true by HAL_TIM_PeriodElapsedCallback (htim6) every 1 ms.
// Cleared by CANManager::service() before calling pinecan1ms().
// volatile: written in ISR context, read in main loop.
// ---------------------------------------------------------------------------
volatile bool pinecan1ms_flag = false;

// ---------------------------------------------------------------------------
// CANManager::initialize
// Called once at startup from main.cpp. Calls pinecanInit and stores the
// optional set_control_state callback for future push-delivery use.
// ---------------------------------------------------------------------------

PineCAN_Status CANManager::initialize(uint32_t node_id,
    CAN_HandleTypeDef *hcan,
    std::function<void(uint8_t)> set_control_state_callback) {
    if (can_manager_initialized) {
        return PINECAN_OK;
    }

    can_manager_instance.manager = CANManager(node_id, hcan, set_control_state_callback);

    PinecanInit init_params = {
        .hcan       = hcan,
        .canard     = &can_manager_instance.manager.canard_,
        .nodeStatus = &can_manager_instance.manager.node_status_
    };

    PineCAN_Status result = pinecanInit(&init_params);
    if (result != PINECAN_OK) {
        return PINECAN_ERROR;
    }

    can_manager_initialized = true;
    return PINECAN_OK;
}

// ---------------------------------------------------------------------------
// CANManager private constructor
// ---------------------------------------------------------------------------

CANManager::CANManager(
    uint32_t node_id,
    CAN_HandleTypeDef *hcan,
    std::function<void(uint8_t)> set_control_state_cb
    ) : node_id_(node_id), canard_{}, node_status_{}, callback_(set_control_state_cb) {
    (void)hcan; // passed to pinecanInit via PinecanInit

    node_status_.health    = UAVCAN_PROTOCOL_NODESTATUS_HEALTH_OK;
    node_status_.mode      = UAVCAN_PROTOCOL_NODESTATUS_MODE_OPERATIONAL;
    node_status_.sub_mode  = 0;
    node_status_.vendor_specific_status_code = 0;
}

// ===========================================================================
// INTERRUPT SIDE — runs in CAN RX FIFO interrupt context
// ===========================================================================

// ---------------------------------------------------------------------------
// CANManager::cacheLatestState
// Stores the decoded vehicle_state into the singleton cache.
// Called by handleNotifyState in pinecan_handlers.c (interrupt context).
// No mapping, no callback — the main-loop poll reads this later.
// ---------------------------------------------------------------------------

void CANManager::cacheLatestState(uint64_t vehicle_state) {
    if (!can_manager_initialized) return;

    // Brief critical section: 64-bit write is two instructions on Cortex-M4.
    __disable_irq();
    can_manager_instance.manager.latest_vehicle_state_ = vehicle_state;
    __enable_irq();
}

// ===========================================================================
// POLL SIDE — runs in main loop
// ===========================================================================

// ---------------------------------------------------------------------------
// CANManager::service                                              [1 ms tick]
// Called every main-loop iteration. When pinecan1ms_flag is set by the TIM6
// ISR, clears it and calls pinecan1ms() to service PineCAN housekeeping
// (node-status heartbeat, internal timers). No-op if not initialized.
// ---------------------------------------------------------------------------

void CANManager::service() {
    if (!can_manager_initialized) return;

    if (pinecan1ms_flag) {
        pinecan1ms_flag = false;
        pinecan1ms();
    }
}

// ---------------------------------------------------------------------------
// CANManager::getLatestVehicleState                                [STEP 1]
// Returns the most recently cached raw vehicle_state bitmask.
// No parameters, no mapping. Pass the result to vehicleStateChanged().
// ---------------------------------------------------------------------------

uint64_t CANManager::getLatestVehicleState() {
    if (!can_manager_initialized) return 0;

    // Brief critical section: 64-bit read is two instructions on Cortex-M4.
    __disable_irq();
    uint64_t raw_vehicle_state = can_manager_instance.manager.latest_vehicle_state_;
    __enable_irq();
    return raw_vehicle_state;
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

// ===========================================================================
// RETAINED — push-delivery path (not active in current poll model)
// ===========================================================================

// ---------------------------------------------------------------------------
// CANManager::deliverState
// Retained for future use. Calls mapVehicleState and invokes the stored
// callback if the result is valid. Not called in the current poll model.
// ---------------------------------------------------------------------------

void CANManager::deliverState(uint64_t vehicle_state) {
    if (!can_manager_initialized) return;

    const uint8_t flight_state_number = mapVehicleState(vehicle_state);
    if (flight_state_number == UNRECOGNIZED_STATE) return;

    auto &cb = can_manager_instance.manager.callback_;
    if (cb) {
        cb(flight_state_number);
    }
}
