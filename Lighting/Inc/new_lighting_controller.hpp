/*
 * new_lighting_controller.hpp
 *
 * Public entry points for the PineCAN-driven lighting pipeline (see
 * LIGHTING_MIGRATION_SPEC.md §9). Three separate functions, no 3-in-1
 * wrapper — the main loop's placeholder split (outer: on state change only;
 * inner: every rate-gated iteration) requires it.
 *
 * No LightingController class and no per-LED object hierarchy are ported
 * from the legacy stack: Push_Leds is a flat, config-driven byte-encoder
 * (§9.5), so there is nothing here for a class to own beyond module-static
 * state private to new_lighting_controller.cpp.
 *
 *  Author: Avi
 */

#ifndef INC_NEW_LIGHTING_CONTROLLER_HPP_
#define INC_NEW_LIGHTING_CONTROLLER_HPP_

#include <cstdint>

#include "conversions.hpp"
#include "tim.h"

// Descriptive names, chosen to avoid the legacy SK6812/WS2812 naming
// inversion (in this repo SK6812 is actually 3ch/24-bit RGB, and WS2812 is
// actually 4ch/32-bit RGBW -- backwards from real-world part naming).
// Legacy class mapping (do NOT rename the legacy classes -- shared, and
// Tejas builds against them):
//   CHIP_RGB   <- legacy class `SK6812`  (3 channels, 24 bits)
//   CHIP_RGBW  <- legacy class `WS2812`  (4 channels, 32 bits)
enum ChipType {
	CHIP_RGB  = 0,   // 3 channels, 24 bits
	CHIP_RGBW = 1    // 4 channels, 32 bits
};

// The #if selects which config header is included -- nothing else.
// Board-specific facts (LED count, chip types, zone->LED membership, buffer
// sizes, timer/channel) live entirely in the selected config file.
//
// NOTE: legacy lighting_controller.hpp spells this `#ifdef REV5 / #elifdef
// REV4` -- `#elifdef` is a GNU/C23 extension, unrecognized under strict ISO
// dialects (verified: it silently breaks the branch and both config headers
// get included). `#if defined(...)` is the portable ISO C++ equivalent with
// identical behaviour, used here instead for that reason.
#if defined(REV5)
#include "new_rev5_config.hpp"
#elif defined(REV4)
#include "new_rev4_config.hpp"
#endif

/**
 * Initialises the bank/DMA output buffers and starts the PWM+DMA stream.
 *
 * Call once from main setup, after initCAN() succeeds.
 */
void led_init();

/**
 * Selects the pattern row for the given flight state.
 *
 * Bounds-checks flight_state before indexing pattern_table: any value
 * >= STATE_COUNT (including CANManager::UNRECOGNIZED_STATE, 0xFF) holds the
 * previous appearance unchanged rather than indexing out of bounds.
 *
 * Call from main's OUTER placeholder -- on state change only.
 *
 * @param flight_state : a LightingStateTransition value (0-8), or 0xFF
 */
void Select_Pattern(uint8_t flight_state);

/**
 * Advances animations for this tick and expands the active zones into their
 * physical LEDs, writing the per-LED colour buffer used by Push_Leds.
 *
 * Call from main's INNER loop -- every iteration (rate-gated, ~50 Hz).
 *
 * @param tick : monotonically increasing tick counter, incremented once per
 *               rate-gated call by the caller
 */
void Generate_Leds(uint32_t tick);

/**
 * Encodes the current colour buffer into the bank output buffer as a
 * bit-timed PWM signal (DMA streams it continuously; this function only
 * updates what the next DMA pass will send).
 *
 * Call from main's INNER loop -- every iteration, immediately after
 * Generate_Leds.
 */
void Push_Leds();

#endif /* INC_NEW_LIGHTING_CONTROLLER_HPP_ */
