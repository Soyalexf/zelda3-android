// On-screen touch controls.
//
// The original port had none: without a physical gamepad the app was
// unplayable. Buttons are defined in normalized window coordinates (0..1) so
// what gets drawn and what gets touched line up at any resolution or crop.
#ifndef ZELDA3_TOUCH_CONTROLS_H_
#define ZELDA3_TOUCH_CONTROLS_H_

#include "types.h"

union SDL_Event;

// Indices passed to TouchInputCallback: these match the order of
// kKeys_Controls (up, down, left, right, select, start, A, B, X, Y, L, R).
enum {
  kTouchOpenMenu = 12,
  kTouchToggleTurbo = 13,
  // Total number of indices. Any new index goes BEFORE this one.
  kTouchControlCount
};

// Implemented by main.c: maps the index to the game's input.
void TouchInputCallback(int control_index, bool pressed);

// Loads the saved layout. Called once at startup.
void TouchControls_Init(void);

// Layout mode: drag the buttons around the screen.
void TouchControls_SetEditMode(bool on);
bool TouchControls_EditMode(void);
void TouchControls_ResetLayout(void);

bool TouchControls_Active(void);
// Returns true if the event was a touch and was consumed.
bool TouchControls_HandleEvent(const union SDL_Event *e);
void TouchControls_Draw(uint8 *pixels, size_t pitch, int width, int height);

#endif  // ZELDA3_TOUCH_CONTROLS_H_
