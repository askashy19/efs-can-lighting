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
 * CANManager: the pure, transport-agnostic, testable core of the CAN
 * receive pipeline. It owns NO PineCAN state and touches NO HAL — PineCAN
 * init, the 1 ms service, the RX handler, and the async vehicle_state cache
 * all live in Core/Src/can.c, mirroring the efs-pinecan single-servo driver
 * reference (its can.c owns initCAN()/handleArrayCommand). CANManager
 * exposes the pure mapVehicleState mapping, the change detector, and thin
 * forwarders (getLatestVehicleState / service) to can.c's entry points so
 * main.cpp has a single, stable call surface regardless of where PineCAN
 * ownership lives.
 */

#ifndef INC_CAN_MANAGER_HPP_
#define INC_CAN_MANAGER_HPP_

#include <cstdint>
#include "Lighting/Inc/conversions.hpp"

class CANManager {
public:
    /// Sentinel returned by mapVehicleState when vehicle_state is non-zero
    /// but contains no bit that maps to a LightingStateTransition.
    static constexpr uint8_t UNRECOGNIZED_STATE = 0xFF;

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

    /// Parameterless getter for the poll side.
    /// Forwards to canGetLatestVehicleState() (Core/Src/can.c), which owns
    /// the actual async cache written by handleNotifyState. Returns the
    /// most recently cached raw vehicle_state bitmask (last-write-wins), or
    /// 0 if no message has been received yet. Performs NO mapping — pass
    /// the result to vehicleStateChanged().
    static uint64_t getLatestVehicleState();

    /// Returns true if `raw_vehicle_state` differs from the value seen on the previous call.
    /// Stores `raw_vehicle_state` internally as prev_data for the next comparison.
    /// First call always returns false (establishes the baseline so the
    /// initial cached value of 0 is not delivered as a spurious change).
    /// Call this from the main while loop with the result of getLatestVehicleState().
    static bool vehicleStateChanged(uint64_t raw_vehicle_state);

    /// Called from the main loop every iteration. Forwards to canService()
    /// (Core/Src/can.c), which gates pinecan1ms() to fire once per
    /// millisecond and no-ops until initCAN() has succeeded.
    static void service();

private:
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

#endif /* INC_CAN_MANAGER_HPP_ */
