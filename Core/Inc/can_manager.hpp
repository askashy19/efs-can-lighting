// ---------------------------------------------------------------------------
// Main Loop Planning
// ---------------------------------------------------------------------------

/*
bool new_data = false;

while (1) {
    while (new_data) {
        //placeholder: push the domain pattern
        start_pattern();

        // Re-read the cache each inner iteration so new CAN data is visible
        raw_vehicle_state = CANManager::getLatestVehicleState();

        // Step 2: check if data changed since last call
        bool new_data = CANManager::vehicleStateChanged(raw_vehicle_state);

        if (new_data) {
            no_data = false; // new state arrived — break to outer loop
        }

        // every once in a while: inactivity check here
    }
    
    // read cache and decode — gives us the current flight state
    uint8_t flight_state = interpretVehicleState(raw_vehicle_state);

    // placeholder: takes flight_state,
    // changes LED domain to start pushing new data

    bool new_data = true;
}
*/

/*
 * can_manager.hpp
 *
 * CANManager: single owner of all CAN receive functionality.
 * Structured to absorb transmit and other CAN responsibilities later
 * without changing this interface.
 */

#ifndef INC_CAN_MANAGER_HPP_
#define INC_CAN_MANAGER_HPP_

#include <cstdint>
#include <functional>
#include "stm32l4xx_hal.h"
#include "pinecan.h"
#include "uavcan.protocol.NodeStatus.h"
#include "Lighting/Inc/conversions.hpp"

class CANManager {
public:
    /// Sentinel returned by mapVehicleState when vehicle_state is non-zero
    /// but contains no bit that maps to a LightingStateTransition.
    static constexpr uint8_t UNRECOGNIZED_STATE = 0xFF;

    /// Initialize PineCAN with the given node ID, CAN peripheral, and
    /// state-consumer callback. Returns PINECAN_OK on success.
    /// If pinecanInit fails, returns PINECAN_ERROR and the instance is left
    /// uninitialised — pinecan1ms will not be called.
    static PineCAN_Status initialize(
        uint32_t node_id,
        CAN_HandleTypeDef *hcan,
        std::function<void(uint8_t)> set_control_state_callback
    );

    /// Pure mapping function.
    /// Converts a 64-bit vehicle_state bitmask from
    /// ardupilot.indication.NotifyState into a uint8_t result.
    ///
    /// Mapped bits and their LightingStateTransition values:
    ///   bit 23 IS_TAKING_OFF → TRANSITION_TAKEOFF  (3)
    ///   bit 22 IS_LANDING    → TRANSITION_LANDING  (6)
    ///   bit  2 FLYING        → TRANSITION_FLIGHT   (4)
    ///   bit  1 ARMED         → TRANSITION_STANDBY  (1)
    ///   bit  0 INITIALISING  → TRANSITION_STARTUP  (8)
    ///
    /// Precedence (highest wins):
    ///   IS_TAKING_OFF > IS_LANDING > FLYING > ARMED > INITIALISING
    ///
    /// Special cases:
    ///   vehicle_state == 0                   → TRANSITION_GROUND (0)
    ///   non-zero, no mapped bit set          → UNRECOGNIZED_STATE (0xFF)
    ///
    /// HAL-free, PineCAN-free, and side-effect-free.
    static uint8_t mapVehicleState(uint64_t vehicle_state);

    /// Called by the PineCAN handler (handleNotifyState) after a successful
    /// decode. Stores the raw vehicle_state bitmask into the singleton cache.
    /// Performs NO mapping and does NOT invoke the consumer callback —
    /// delivery is done by the main-loop poll via getLatestVehicleState().
    static void cacheLatestState(uint64_t vehicle_state);

    /// Parameterless getter for the poll side.
    /// Returns the most recently cached raw vehicle_state bitmask
    /// (last-write-wins). Returns 0 if no message has been received yet.
    /// Performs NO mapping — pass the result to vehicleStateChanged().
    static uint64_t getLatestVehicleState();

    /// Returns true if `raw_vehicle_state` differs from the value seen on the previous call.
    /// Stores `raw_vehicle_state` internally as prev_data for the next comparison.
    /// First call always returns false (establishes the baseline so the
    /// initial cached value of 0 is not delivered as a spurious change).
    /// Call this from the main while loop with the result of getLatestVehicleState().
    static bool vehicleStateChanged(uint64_t raw_vehicle_state);

    /// Called from the main loop every iteration. Checks pinecan1ms_flag
    /// and calls pinecan1ms() when set. No-op if not initialized.
    static void service();

    /// (Retained) Bridge to the stored callback, used if a push-delivery
    /// path is re-enabled in future. Not called from handleNotifyState
    /// in the current poll model.
    static void deliverState(uint64_t vehicle_state);

private:
    CANManager(
        uint32_t node_id,
        CAN_HandleTypeDef *hcan,
        std::function<void(uint8_t)> cb
    );

    uint32_t node_id_;
    CanardInstance canard_;
    uavcan_protocol_NodeStatus node_status_;
    std::function<void(uint8_t)> callback_;

    /// Cache of the most recently decoded NotifyState.vehicle_state.
    /// Written by cacheLatestState() in interrupt context; read by
    /// getLatestVehicleState() in the main loop. volatile prevents the
    /// compiler from hoisting the read out of the loop.
    /// NOTE: 64-bit access is not atomic on Cortex-M4 — bracket reads and
    /// writes with a brief IRQ critical section (__disable_irq / __enable_irq)
    /// to prevent torn values.
    volatile uint64_t latest_vehicle_state_ = 0;

    /// Previous vehicle_state seen by vehicleStateChanged().
    /// Compared against the current value each poll iteration.
    static uint64_t prev_data_;
    static bool     have_prev_;
};

/// Free function: interprets a raw vehicle_state bitmask into a flight-state
/// number by delegating to CANManager::mapVehicleState.
/// Returns a LightingStateTransition value (0–8), or
/// CANManager::UNRECOGNIZED_STATE (0xFF) if no mapped bit is set.
/// Call this from the main loop after CANManager::vehicleStateChanged() returns true.
uint8_t interpretVehicleState(uint64_t vehicle_state);

/// Defined in can_manager.cpp. Set true by HAL_TIM_PeriodElapsedCallback
/// every 1 ms; cleared and consumed by CANManager::service().
extern volatile bool pinecan1ms_flag;

#endif /* INC_CAN_MANAGER_HPP_ */
