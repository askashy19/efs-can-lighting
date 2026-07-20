/*
 * new_pattern_table.cpp
 *
 * THIS IS THE ONLY FILE TO EDIT WHEN CHANGING COLOURS, BRIGHTNESS, OR
 * ANIMATIONS. See LIGHTING_MIGRATION_SPEC.md §7 for the source tables.
 *
 * Colour + brightness per zone come from Tejas's main.cpp lines 145-152.
 * Active-zones-per-state come from the same source, cross-referenced against
 * legacy's LC_State_* classes. Only two cells use an animation other than
 * ANIM_SOLID: GROUND's CD_BEACON (breathing) and every active CD_STROBE
 * (double-flash) — those are the only two animations legacy ever
 * implemented.
 *
 * Only 6 of the 9 rows are reachable today (ardupilot.indication.NotifyState
 * has no bits for taxiing / braking / comms-lost) — TAXI, BRAKE, and SEARCH
 * are defined anyway so pattern_table[state] stays a direct index with no
 * remapping layer, and marked unreachable below.
 *
 *  Author: Avi
 */

#include "new_pattern_table.hpp"

namespace {

constexpr AnimParams NO_ANIM = {0, 0, 0};

constexpr ZoneAppearance OFF() {
	return { false, {0, 0, 0}, 0, ANIM_SOLID, NO_ANIM };
}

constexpr ZoneAppearance SOLID(RGB_colour_t colour, uint8_t brightness) {
	return { true, colour, brightness, ANIM_SOLID, NO_ANIM };
}

// GROUND's CD_BEACON: mirrors legacy's ramp 0->50 at +-1 per ~20ms tick.
// brightness is fully animation-owned for BREATHE (update_animations()
// overwrites it every call) -- initial value is just min_pct, the ramp's
// starting point, so there's no separate brightness parameter to pass here.
constexpr ZoneAppearance BREATHE(RGB_colour_t colour,
		uint16_t period_ms, uint8_t min_pct, uint8_t max_pct) {
	return { true, colour, min_pct, ANIM_BREATHE, { period_ms, min_pct, max_pct } };
}

// Any active CD_STROBE: mirrors legacy's 8-stage x 100ms sequence.
constexpr ZoneAppearance STROBE(RGB_colour_t colour, uint8_t brightness, uint16_t period_ms) {
	return { true, colour, brightness, ANIM_STROBE, { period_ms, 0, 0 } };
}

} // namespace

// Column order matches ControlDomain exactly:
// CD_MAIN, CD_TAXI, CD_LANDING, CD_NAV, CD_BEACON, CD_STROBE, CD_BRAKE, CD_SEARCH
const ZoneAppearance pattern_table[STATE_COUNT][CD_LENGTH] = {

	// TRANSITION_GROUND (0) -- active: MAIN, BEACON
	{
		SOLID(PURPLE, 5),                  // CD_MAIN
		OFF(),                              // CD_TAXI
		OFF(),                              // CD_LANDING
		OFF(),                              // CD_NAV
		BREATHE(RED, 2000, 0, 50),          // CD_BEACON
		OFF(),                              // CD_STROBE
		OFF(),                              // CD_BRAKE
		OFF(),                              // CD_SEARCH
	},

	// TRANSITION_STANDBY (1) -- active: MAIN, BEACON, STROBE
	{
		SOLID(PURPLE, 5),                  // CD_MAIN
		OFF(),                              // CD_TAXI
		OFF(),                              // CD_LANDING
		OFF(),                              // CD_NAV
		SOLID(RED, 99),                    // CD_BEACON
		STROBE(ORANGE, 99, 800),           // CD_STROBE
		OFF(),                              // CD_BRAKE
		OFF(),                              // CD_SEARCH
	},

	// TRANSITION_TAXI (2) -- unreachable: no NotifyState bit maps here
	// active: MAIN, TAXI, BEACON, BRAKE
	{
		SOLID(PURPLE, 5),                  // CD_MAIN
		SOLID(WHITE, 99),                  // CD_TAXI
		OFF(),                              // CD_LANDING
		OFF(),                              // CD_NAV
		SOLID(RED, 99),                    // CD_BEACON
		OFF(),                              // CD_STROBE
		SOLID(ORANGE, 99),                 // CD_BRAKE
		OFF(),                              // CD_SEARCH
	},

	// TRANSITION_TAKEOFF (3) -- active: MAIN, BEACON
	{
		SOLID(PURPLE, 5),                  // CD_MAIN
		OFF(),                              // CD_TAXI
		OFF(),                              // CD_LANDING
		OFF(),                              // CD_NAV
		SOLID(RED, 99),                    // CD_BEACON
		OFF(),                              // CD_STROBE
		OFF(),                              // CD_BRAKE
		OFF(),                              // CD_SEARCH
	},

	// TRANSITION_FLIGHT (4) -- active: MAIN, BEACON, STROBE, NAV
	{
		SOLID(PURPLE, 5),                  // CD_MAIN
		OFF(),                              // CD_TAXI
		OFF(),                              // CD_LANDING
		SOLID(BLUE, 99),                   // CD_NAV
		SOLID(RED, 99),                    // CD_BEACON
		STROBE(ORANGE, 99, 800),           // CD_STROBE
		OFF(),                              // CD_BRAKE
		OFF(),                              // CD_SEARCH
	},

	// TRANSITION_BRAKE (5) -- unreachable: no NotifyState bit maps here
	// active: MAIN, BRAKE
	{
		SOLID(PURPLE, 5),                  // CD_MAIN
		OFF(),                              // CD_TAXI
		OFF(),                              // CD_LANDING
		OFF(),                              // CD_NAV
		OFF(),                              // CD_BEACON
		OFF(),                              // CD_STROBE
		SOLID(ORANGE, 99),                 // CD_BRAKE
		OFF(),                              // CD_SEARCH
	},

	// TRANSITION_LANDING (6) -- active: MAIN, BEACON, STROBE, NAV, LANDING
	{
		SOLID(PURPLE, 5),                  // CD_MAIN
		OFF(),                              // CD_TAXI
		SOLID(WHITE, 99),                  // CD_LANDING
		SOLID(BLUE, 99),                   // CD_NAV
		SOLID(RED, 99),                    // CD_BEACON
		STROBE(ORANGE, 99, 800),           // CD_STROBE
		OFF(),                              // CD_BRAKE
		OFF(),                              // CD_SEARCH
	},

	// TRANSITION_SEARCH (7) -- unreachable: no NotifyState bit maps here
	// active: MAIN, SEARCH
	{
		SOLID(PURPLE, 5),                  // CD_MAIN
		OFF(),                              // CD_TAXI
		OFF(),                              // CD_LANDING
		OFF(),                              // CD_NAV
		OFF(),                              // CD_BEACON
		OFF(),                              // CD_STROBE
		OFF(),                              // CD_BRAKE
		SOLID(WHITE, 99),                  // CD_SEARCH
	},

	// TRANSITION_STARTUP (8) -- active: MAIN
	{
		SOLID(PURPLE, 5),                  // CD_MAIN
		OFF(),                              // CD_TAXI
		OFF(),                              // CD_LANDING
		OFF(),                              // CD_NAV
		OFF(),                              // CD_BEACON
		OFF(),                              // CD_STROBE
		OFF(),                              // CD_BRAKE
		OFF(),                              // CD_SEARCH
	},
};
