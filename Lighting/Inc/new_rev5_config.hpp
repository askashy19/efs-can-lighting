/*
 * new_rev5_config.hpp
 *
 * rev5 board facts (see LIGHTING_MIGRATION_SPEC.md §8) -- the only things
 * that change when a new PCB revision appears. Included exclusively via
 * new_lighting_controller.hpp's #ifdef REV5 block, after ChipType is
 * defined there.
 *
 *  Author: Avi
 */

#ifndef INC_NEW_REV5_CONFIG_HPP_
#define INC_NEW_REV5_CONFIG_HPP_

#include <cstdint>

#include "conversions.hpp"

#define BOARD_REV 5

// Positions -- three concentric rings of four:
//   SIDE_NW=0  OUTER_NW=1  INNER_NW=2  INNER_SW=3  OUTER_SW=4  SIDE_SW=5
//   SIDE_SE=6  OUTER_SE=7  INNER_SE=8  INNER_NE=9  OUTER_NE=10 SIDE_NE=11
static constexpr uint8_t NUM_LEDS = 12;

// idx 0,5,6,11 = CHIP_RGB (legacy SK6812) | idx 1,2,3,4,7,8,9,10 = CHIP_RGBW (legacy WS2812)
static constexpr ChipType led_types[NUM_LEDS] = {
	CHIP_RGB,  CHIP_RGBW, CHIP_RGBW, CHIP_RGBW,  // 0-3
	CHIP_RGBW, CHIP_RGB,  CHIP_RGB,  CHIP_RGBW,  // 4-7
	CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGB    // 8-11
};

static constexpr uint8_t  NUM_LEDS_PADDING = 10;
static constexpr uint16_t PADDING_SIZE     = NUM_LEDS_PADDING * 32;

// BANK = 4 CHIP_RGB LEDs * 24 bits + 8 CHIP_RGBW LEDs * 32 bits + padding = 672
static constexpr uint16_t BANK_OUTPUT_BUFFER_SIZE = 4 * 24 + 8 * 32 + PADDING_SIZE;
static constexpr uint16_t DMA_OUTPUT_BUFFER_SIZE  = BANK_OUTPUT_BUFFER_SIZE * 2;

// SIDE  ring = {0, 5, 6, 11}
// OUTER ring = {1, 4, 7, 10}
// INNER ring = {2, 3, 8, 9}
// Zone->LED membership is GLOBAL: one fixed set per board, not per-state
// (legacy varied CD_MAIN's membership by state -- not carried forward; see
// spec §8, "Zone -> LED membership is GLOBAL").
// bit N set == LED index N belongs to this zone
static constexpr uint16_t zone_leds[CD_LENGTH] = {
	/* CD_MAIN    */ 0x0FFF,  // all 12
	/* CD_TAXI    */ 0x0861,  // SIDE  ring {0,5,6,11}
	/* CD_LANDING */ 0x0492,  // OUTER ring {1,4,7,10}
	/* CD_NAV     */ 0x0861,  // SIDE  ring {0,5,6,11}
	/* CD_BEACON  */ 0x0492,  // OUTER ring {1,4,7,10}
	/* CD_STROBE  */ 0x030C,  // INNER ring {2,3,8,9}
	/* CD_BRAKE   */ 0x030C,  // INNER ring {2,3,8,9}
	/* CD_SEARCH  */ 0x030C,  // INNER ring {2,3,8,9}  <- legacy bug fixed: legacy listed
	                          //    LED_INNER_NW twice and omitted LED_INNER_NE, giving
	                          //    {2,3,8} instead of the intended {2,3,8,9}
};

#define LED_TIM          htim2
#define LED_TIM_CHANNEL  TIM_CHANNEL_1

#endif /* INC_NEW_REV5_CONFIG_HPP_ */
