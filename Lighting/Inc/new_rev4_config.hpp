/*
 * new_rev4_config.hpp
 *
 * rev4 board facts (see LIGHTING_MIGRATION_SPEC.md §8) -- the only things
 * that change when a new PCB revision appears. Included exclusively via
 * new_lighting_controller.hpp's #elifdef REV4 block, after ChipType is
 * defined there.
 *
 *  Author: Avi
 */

#ifndef INC_NEW_REV4_CONFIG_HPP_
#define INC_NEW_REV4_CONFIG_HPP_

#include <cstdint>

#include "conversions.hpp"

#define BOARD_REV 4

// Positions:
//   NW=0 W=1 SW=2 SE=3 E=4 NE=5 SIDE_NE=6 SIDE_SE=7 SIDE_SW=8 SIDE_NW=9
static constexpr uint8_t NUM_LEDS = 10;

// ALL 10 = CHIP_RGBW (legacy WS2812)
static constexpr ChipType led_types[NUM_LEDS] = {
	CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGBW,
	CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGBW
};

static constexpr uint8_t  NUM_LEDS_PADDING = 10;
static constexpr uint16_t PADDING_SIZE     = NUM_LEDS_PADDING * 32;

// RECOMPUTED -- legacy said `10*24`, which appears to be a bug. Every rev4
// LED instantiates as the 4-channel (CHIP_RGBW / legacy WS2812) class, i.e.
// 32 bits each, not 24. An undersized DMA buffer would truncate the chain.
// Verify on hardware before trusting.
static constexpr uint16_t BANK_OUTPUT_BUFFER_SIZE = NUM_LEDS * 32 + PADDING_SIZE;
static constexpr uint16_t DMA_OUTPUT_BUFFER_SIZE  = BANK_OUTPUT_BUFFER_SIZE * 2;

// Corners = {0,2,3,5}, sides = {6,7,8,9}, E/W = {1,4}.
// Zone->LED membership is GLOBAL: one fixed set per board, not per-state
// (legacy varied CD_MAIN's membership by state -- not carried forward; see
// spec §8, "Zone -> LED membership is GLOBAL").
// bit N set == LED index N belongs to this zone
static constexpr uint16_t zone_leds[CD_LENGTH] = {
	/* CD_MAIN    */ 0x03FF,  // all 10
	/* CD_TAXI    */ 0x03C0,  // sides   {6,7,8,9}
	/* CD_LANDING */ 0x002D,  // corners {0,2,3,5}
	/* CD_NAV     */ 0x03C0,  // sides   {6,7,8,9}
	/* CD_BEACON  */ 0x002D,  // corners {0,2,3,5}
	/* CD_STROBE  */ 0x0012,  // E/W     {1,4}   <- conflict resolved, see note below
	/* CD_BRAKE   */ 0x0012,  // E/W     {1,4}   <- conflict resolved, see note below
	/* CD_SEARCH  */ 0x003F,  // {0,1,2,3,4,5}
};

// CONFLICT RESOLUTION: legacy defined these zones differently in different states --
//   CD_STROBE: {1,4} in FLIGHT, but {6,7,8,9} in STANDBY
//   CD_BRAKE:  {1,4} in TAXI,   but {0,2,3,5} in the BRAKE state
// Under the global-membership model (see spec §8) a single set must be chosen.
// {1,4} was selected for both, because on rev5 STROBE and BRAKE both occupy the
// INNER ring, and {1,4} (W/E -- the non-corner, non-side LEDs) is rev4's closest
// structural equivalent. This keeps rev4 and rev5 semantically parallel.
// Chosen deliberately; revisit if the original intent is confirmed to differ.

#define LED_TIM          htim2
#define LED_TIM_CHANNEL  TIM_CHANNEL_1

#endif /* INC_NEW_REV4_CONFIG_HPP_ */
