/*
 * new_pattern_table.hpp
 *
 * Declares the pattern table: one row per LightingStateTransition, one
 * column per ControlDomain (see LIGHTING_MIGRATION_SPEC.md §7). The table
 * itself — the only file meant to be edited when colours, brightness, or
 * animations change — lives in new_pattern_table.cpp.
 *
 *  Author: Avi
 */

#ifndef INC_NEW_PATTERN_TABLE_HPP_
#define INC_NEW_PATTERN_TABLE_HPP_

#include <cstdint>

#include "conversions.hpp"
#include "new_pattern_types.hpp"

// Declared here (not in conversions.hpp, which is shared with Tejas's
// can_manager.hpp and must not be touched) — table size AND bounds-check
// limit for Select_Pattern.
static constexpr uint8_t STATE_COUNT = 9;

// One row per state. Index matches LightingStateTransition exactly.
extern const ZoneAppearance pattern_table[STATE_COUNT][CD_LENGTH];

#endif /* INC_NEW_PATTERN_TABLE_HPP_ */
