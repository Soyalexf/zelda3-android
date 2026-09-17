// On-screen settings menu, driven with the gamepad.
//
// It exists because on Android changing any option meant editing the .ini
// inside Android/data/, which has been restricted since Android 11 and needs a
// file manager with special permissions. This way settings are changed from
// inside the game and saved automatically.
#ifndef ZELDA3_OVERLAY_MENU_H_
#define ZELDA3_OVERLAY_MENU_H_

#include "types.h"

// Actions the menu cannot carry out on its own; main.c resolves them.
enum {
  kMenuActionSaveState = 1,
  kMenuActionLoadState,
  kMenuActionMoveButtons,
  kMenuActionResetButtons
};
void OverlayMenu_RunAction(int action);

// Picks between the English and Spanish version of a string.
const char *OverlayMenu_Text(const char *en, const char *es);

bool OverlayMenu_IsOpen(void);
void OverlayMenu_Toggle(void);

// Handles a tap in normalized window coordinates. Returns true if it landed on
// the menu. Without this the menu could be opened by touch but not operated,
// which made it useless on a device with no gamepad.
bool OverlayMenu_HandleTouch(float x, float y);

// Returns true if the menu consumed the command (and it must therefore not
// reach the game). It only consumes while open.
bool OverlayMenu_HandleCommand(uint32 cmd, bool pressed);

// Draws on top of the already rendered frame. 'width' and 'height' are the
// buffer's real pixel size, i.e. already multiplied by render_scale.
void OverlayMenu_Draw(uint8 *pixels, size_t pitch, int width, int height);

// Exposed so the touch controls can reuse the font and blending instead of
// duplicating them.
void OverlayDraw_Text(uint8 *pixels, size_t pitch, int w, int h,
                      int x, int y, const char *s, uint32 color, int scale);
void OverlayDraw_Rect(uint8 *pixels, size_t pitch, int w, int h,
                      int x0, int y0, int x1, int y1, uint32 color, int alpha);

#endif  // ZELDA3_OVERLAY_MENU_H_
