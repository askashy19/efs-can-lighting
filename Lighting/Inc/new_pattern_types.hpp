/*
 * new_pattern_types.hpp
 *
 * Keystone data types for the PineCAN-driven lighting pipeline (see
 * LIGHTING_MIGRATION_SPEC.md §6). A ZoneAppearance is the full description of
 * what one zone (ControlDomain) looks like: whether it's lit, its colour, its
 * brightness, and which animation (if any) drives it over time.
 *
 * "Off" is expressed by `active = false`, never by `brightness = 0` — one
 * representation only.
 *
 *  Author: Avi
 */

#ifndef INC_NEW_PATTERN_TYPES_HPP_
#define INC_NEW_PATTERN_TYPES_HPP_

#include <cstdint>

#include "conversions.hpp"

enum AnimType {
	ANIM_SOLID   = 0,   // static colour, no time dependence
	ANIM_BREATHE = 1,   // brightness ramps up and down
	ANIM_STROBE  = 2    // on/off flash sequence
};

struct AnimParams {
	uint16_t period_ms;   // full cycle length
	uint8_t  min_pct;     // BREATHE floor  (0-100)
	uint8_t  max_pct;     // BREATHE ceiling (0-100)
};

struct ZoneAppearance {
	bool          active;      // is this zone lit in this state?
	RGB_colour_t  colour;      // from the conversions.hpp palette
	uint8_t       brightness;  // 0-100 PERCENT — not 0-255
	AnimType      animation;
	AnimParams    params;
};

#endif /* INC_NEW_PATTERN_TYPES_HPP_ */
