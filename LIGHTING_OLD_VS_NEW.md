# Lighting Controller: Legacy vs. New (PineCAN) — Comparison

Companion doc to [`LIGHTING_MIGRATION_SPEC.md`](LIGHTING_MIGRATION_SPEC.md), which is the
authoritative design spec. This doc is a quick-reference map between the legacy
`lighting_controller.*` stack and the new `new_lighting_controller.*` stack for anyone
diffing the two.

---

## TL;DR

The legacy stack is **object-oriented and runtime-mutable**: each physical LED is a C++
object (`SK6812`/`WS2812`), constructed via `placement new`, and a `LightingController`
class exposes ~18 methods to reconfigure colour/brightness/domain-membership at runtime.

The new stack is **data-driven and static**: there are no LED objects at all. "What LED is
what chip type" and "what does each zone look like in each flight state" are both
`constexpr` lookup tables baked into the binary at compile time. The only runtime API is
4 functions, and the only thing that changes at runtime is *which table row is selected*
plus animation math (breathe/strobe) within that row.

---

## File Manifest

| Concern | Legacy | New |
|---|---|---|
| Board facts (LED count, chip type, zone membership) | Hardcoded inline via `#ifdef REV5`/`#elifdef REV4` in `lighting_controller.cpp` | [`new_rev5_config.hpp`](Lighting/Inc/new_rev5_config.hpp) / [`new_rev4_config.hpp`](Lighting/Inc/new_rev4_config.hpp) |
| Per-LED objects | `sk6812.hpp/cpp`, `ws2812.hpp/cpp`, `led.hpp/cpp` (class hierarchy) | **Deleted** — no LED classes in the new stack |
| Controller / public API | `lighting_controller.hpp/cpp` (`LightingController` class) | [`new_lighting_controller.hpp/cpp`](Lighting/Inc/new_lighting_controller.hpp) (4 free functions) |
| Per-state lighting patterns | `rev4_/rev5_lighting_control_state_classes.*` (state object hierarchy) | [`new_pattern_table.cpp`](Lighting/Src/new_pattern_table.cpp) (static table — **the only file to edit for colour/brightness/animation changes**) |
| Pattern data types | N/A (baked into state classes) | [`new_pattern_types.hpp`](Lighting/Inc/new_pattern_types.hpp) (`ZoneAppearance`, `AnimType`, `AnimParams`) |
| Shared/reused | — | [`conversions.hpp`](Lighting/Inc/conversions.hpp) (zones, states, colour palette) — reused as-is, not touched |

---

## LED Definition: Storage vs. Table

**Legacy** — each LED is a real C++ object, requiring hand-reserved, aligned storage and a
`placement new` call per index, duplicated per board rev:

```cpp
// lighting_controller.cpp
#ifdef REV5
alignas(SK6812) uint8_t LED0Storage[sizeof(SK6812)];
...
#endif

void initialize_leds(LED **leds) {
    #ifdef REV5
    leds[0] = new (&LED0Storage) SK6812();
    leds[1] = new (&LED1Storage) WS2812();
    ...
    #endif
}
```

**New** — no storage, no objects, no `initialize_leds()`-equivalent function. "What chip
type is LED `i`" is answered by a `constexpr` array baked directly into read-only memory
at compile time — nothing executes to "set it up":

```cpp
// new_rev5_config.hpp
static constexpr uint8_t NUM_LEDS = 12;
static constexpr ChipType led_types[NUM_LEDS] = {
    CHIP_RGB,  CHIP_RGBW, CHIP_RGBW, CHIP_RGBW,
    CHIP_RGBW, CHIP_RGB,  CHIP_RGB,  CHIP_RGBW,
    CHIP_RGBW, CHIP_RGBW, CHIP_RGBW, CHIP_RGB
};
static constexpr uint16_t zone_leds[CD_LENGTH] = {
    /* CD_MAIN    */ 0x0FFF,  // all 12
    /* CD_TAXI    */ 0x0861,  // SIDE  ring {0,5,6,11}
    ...
};
```

`Push_Leds()` reads `led_types[i]` inline at encode time to decide 3-channel vs.
4-channel bit packing — the same job the old `SK6812`/`WS2812` subclasses did via virtual
dispatch, now done with a single `if`.

---

## Runtime API — Function-by-Function

The legacy `LightingController` class's public methods were **not ported 1:1**. They were
architecturally replaced by two static tables (`led_types`/`zone_leds` per-board config,
`pattern_table` per-state data) plus 4 functions.

| Legacy method | New equivalent |
|---|---|
| `LightingController(...)` ctor, `start_lighting_control()` | `led_init()` |
| `recolour_all(colour[, brightness])` | **Gone as a runtime call.** Colour/brightness are fixed per `(state, zone)` cell in `pattern_table` — changed by editing the table, not by calling a function |
| `recolour_by_index(index, colour[, brightness])` | **Gone entirely.** New code never addresses an individual LED by index for colour — only zones, expanded via `zone_leds` |
| `add_led_to_cd(index, domain)` / `remove_led_from_cd(...)` | **Gone.** Zone→LED membership is `global` and fixed per board (spec §12, decision #1) — no runtime reassignment |
| `activate_domain(domain)` / `deactivate_domain(domain)` / `activate_domains(bitfield)` | **Gone.** Whether a zone is lit is the `active` bool baked into each `pattern_table` cell; the whole row applies at once via `Select_Pattern(state)` |
| `allow_domain` / `disallow_domain` / `configure_allowed_domains` / `configure_active_domains` | **Gone.** No allowed-vs-active bitfield gating layer exists anymore |
| `set_domain_colour_and_brightness` / `set_domain_colour` / `set_domain_brightness` | **Gone.** Same as `recolour_all` — values live in the static table |
| `set_lighting_control_state(state*)`, `exit_current_state()`, `execute_state()`, `get_lighting_control_state()`, plus the entire `rev4_/rev5_lighting_control_state_classes.*` hierarchy | Collapsed into one call: `Select_Pattern(flight_state)` |

**New public API (all 4 functions, in [`new_lighting_controller.hpp`](Lighting/Inc/new_lighting_controller.hpp)):**

| Function | Called from | Does |
|---|---|---|
| `led_init()` | Once, at setup | Primes output buffers to "off", starts PWM/DMA, selects boot default (`TRANSITION_GROUND`) |
| `Select_Pattern(uint8_t flight_state)` | On flight-state change only | Copies one row of `pattern_table` into the live per-zone appearance state; bounds-checks `flight_state`, holding the previous appearance on anything ≥ `STATE_COUNT` (incl. `0xFF`) |
| `Generate_Leds(uint32_t tick)` | Every rate-gated tick (~50 Hz) | Advances animation math (breathe ramp / strobe stage), expands each active zone into its member LEDs via `zone_leds`, writes into the colour buffer |
| `Push_Leds()` | Every tick, right after `Generate_Leds` | Encodes the colour buffer into the DMA bank buffer as bit-timed PWM, using `led_types[i]` to choose RGB vs. RGBW packing |

---

## Data Flow

**Legacy:** CAN command → state object swap → state object's own logic mutates
`LightingController`'s domain colour/brightness/active fields imperatively → next DMA
cycle picks up the change.

**New:**
```
flight_state ──> Select_Pattern() ──> current_appearance[zone]   (from static pattern_table)
                                              │
                          Generate_Leds()  ───┤  animation math (breathe/strobe) + zone→LED expansion
                                              ▼
                                      colour_buffer[LED index]
                                              │
                              Push_Leds() ────┘  bit-encode using led_types[]
                                              ▼
                                   bank_output_buffer → DMA
```

---

## Known Caveats / Deliberate Deviations

These are called out explicitly in `LIGHTING_MIGRATION_SPEC.md` §11–§12 — flagging here
so they aren't mistaken for silent bugs:

- **rev4 buffer size changed.** Legacy computed `BANK_OUTPUT_BUFFER_SIZE = 10*24 + ...`
  (24 bits/LED), but every rev4 LED is actually the 4-channel/32-bit chip. New code uses
  `NUM_LEDS * 32 + PADDING_SIZE`. Believed to fix a legacy under-sized-buffer bug —
  **unverified on hardware.**
- **rev5 `CD_SEARCH` fixed.** Legacy listed `LED_INNER_NW` twice and omitted
  `LED_INNER_NE`, giving `{2,3,8}` instead of the intended `{2,3,8,9}`. New table has the
  corrected set.
- **rev4 `CD_STROBE`/`CD_BRAKE` conflict resolved.** Legacy defined these zones
  differently per-state (e.g. STROBE was `{1,4}` in FLIGHT but `{6,7,8,9}` in STANDBY).
  The new global-membership model can only hold one set, so `{1,4}` was chosen for both
  as the closest structural match to rev5's INNER ring — a judgment call, not a
  rediscovered fact.
- **Zone→LED membership is now global**, not per-state as some legacy domains were.
- **No dynamic/ad-hoc recolour hook.** If a future requirement needs to change a zone's
  colour independent of a flight-state transition (e.g. a one-off CAN command), there's
  no API for that today — it would require either a new pattern-table row or a new
  function. Not a gap in this port; simply out of scope for it.
- **Only 6 of 9 pattern-table rows are reachable today** (`TAXI`, `BRAKE`, `SEARCH` have
  no corresponding `ardupilot.indication.NotifyState` bit) — defined anyway so
  `pattern_table[state]` stays a direct index with no remapping layer.
