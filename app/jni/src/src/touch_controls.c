#include "touch_controls.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "overlay_menu.h"

extern Config g_config;

// Every measurement is in units normalized to the window width. So a circle
// comes out round rather than oval, the Y radius is multiplied by the aspect
// ratio.
typedef struct TouchButton {
  float cx, cy, r;
  int control;      // index for TouchInputCallback
  const char *cap;  // short caption
} TouchButton;

#define kFaceR   0.042f
#define kSmallR  0.030f

static TouchButton g_buttons[] = {
  // Face-button diamond, SNES layout: A right, B bottom, X top, Y left.
  { 0.930f, 0.660f, kFaceR,  6, "A" },
  { 0.868f, 0.820f, kFaceR,  7, "B" },
  { 0.868f, 0.500f, kFaceR,  8, "X" },
  { 0.806f, 0.660f, kFaceR,  9, "Y" },
  // Shoulder buttons
  { 0.075f, 0.130f, kSmallR, 10, "L" },
  { 0.925f, 0.130f, kSmallR, 11, "R" },
  // Select and Start
  { 0.430f, 0.900f, kSmallR,  4, "SEL" },
  { 0.545f, 0.900f, kSmallR,  5, "STA" },
  // Touch has no R3, so the menu needs a button of its own.
  { 0.488f, 0.105f, kSmallR, kTouchOpenMenu, "MENU" },
  { 0.598f, 0.105f, kSmallR, kTouchToggleTurbo, "TUR" },
};
#define kButtonCount ((int)(sizeof(g_buttons) / sizeof(g_buttons[0])))

// Copy of the factory positions, so they can be restored.
static TouchButton g_buttons_default[kButtonCount];
static bool g_defaults_saved;

// D-pad: arm length (fixed) and center (movable).
#define kPadArm 0.105f
#define kPadCxDefault 0.140f
#define kPadCyDefault 0.680f
static float g_pad_cx = kPadCxDefault, g_pad_cy = kPadCyDefault;

// With the floating stick, any touch in this left half that doesn't land on a
// button spawns a stick centered exactly where the finger went down.
#define kStickZoneRight 0.45f
#define kStickDead 0.022f
#define kStickRange 0.075f

#define kMaxFingers 8
typedef struct FingerState {
  SDL_FingerID id;
  bool active;
  uint32 mask;  // control bits this finger is holding down
} FingerState;

static FingerState g_fingers[kMaxFingers];
static uint32 g_held;  // union of every finger
static float g_aspect = 1.0f;
static bool g_turbo_on;

// Floating stick: where the finger went down, and where it is now.
static struct {
  bool active;
  SDL_FingerID id;
  float ox, oy, x, y;
} g_stick;

static bool UseStick(void) { return g_config.touch_stick != 0; }

// ---------------------------------------------------------------------------
// Layout mode: drag the buttons wherever you want them.
// ---------------------------------------------------------------------------
static bool g_edit_mode;
static int g_drag;            // dragged index; kButtonCount = the d-pad
static SDL_FingerID g_drag_id;
#define kDragNone (-1)
#define kDragPad kButtonCount

// Fixed exit button; it cannot be dragged, so you can never get stuck.
#define kDoneCx 0.500f
#define kDoneCy 0.930f
#define kDoneR  0.045f

static void LayoutPath(char *out, size_t n) {
  const char *dir = SDL_AndroidGetExternalStoragePath();
  if (dir && *dir)
    snprintf(out, n, "%s/touch_layout.txt", dir);
  else
    snprintf(out, n, "touch_layout.txt");
}

static void SaveLayout(void) {
  char path[512];
  LayoutPath(path, sizeof(path));
  FILE *f = fopen(path, "wb");
  if (!f)
    return;
  fprintf(f, "# Posiciones de los controles tactiles (0..1 de la pantalla).\n");
  fprintf(f, "PAD %.4f %.4f\n", g_pad_cx, g_pad_cy);
  for (int i = 0; i < kButtonCount; i++)
    fprintf(f, "%s %.4f %.4f\n", g_buttons[i].cap, g_buttons[i].cx, g_buttons[i].cy);
  fclose(f);
}

static void LoadLayout(void) {
  if (!g_defaults_saved) {
    memcpy(g_buttons_default, g_buttons, sizeof(g_buttons));
    g_defaults_saved = true;
  }
  char path[512];
  LayoutPath(path, sizeof(path));
  FILE *f = fopen(path, "rb");
  if (!f)
    return;
  char name[32];
  float x, y;
  while (fscanf(f, "%31s %f %f", name, &x, &y) == 3) {
    if (x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f)
      continue;
    if (strcmp(name, "PAD") == 0) {
      g_pad_cx = x; g_pad_cy = y;
      continue;
    }
    for (int i = 0; i < kButtonCount; i++) {
      if (strcmp(name, g_buttons[i].cap) == 0) {
        g_buttons[i].cx = x; g_buttons[i].cy = y;
        break;
      }
    }
  }
  fclose(f);
}

void TouchControls_ResetLayout(void) {
  if (g_defaults_saved)
    memcpy(g_buttons, g_buttons_default, sizeof(g_buttons));
  g_pad_cx = kPadCxDefault;
  g_pad_cy = kPadCyDefault;
  SaveLayout();
}

void TouchControls_Init(void) { LoadLayout(); }

void TouchControls_SetEditMode(bool on) {
  if (on)
    LoadLayout();
  else
    SaveLayout();
  g_edit_mode = on;
  g_drag = kDragNone;
}

bool TouchControls_EditMode(void) { return g_edit_mode; }

bool TouchControls_Active(void) {
  if (g_config.touch_controls == 1)
    return true;
  if (g_config.touch_controls == 2)
    return false;
  // Default (0): automatic, only when no gamepad is connected. That keeps a
  // handheld with physical controls from being cluttered with buttons.
  return SDL_NumJoysticks() == 0;
}

static uint32 HitTest(float x, float y) {
  uint32 mask = 0;
  // With the floating stick there is no d-pad, but every other button is still
  // there: skip only this block, never the button search further down.
  if (!UseStick()) {
    // D-pad: split into 3x3 so diagonals fall out naturally.
    float dx = (x - g_pad_cx) / kPadArm;
    float dy = (y - g_pad_cy) / (kPadArm * g_aspect);
    if (dx > -1.5f && dx < 1.5f && dy > -1.5f && dy < 1.5f) {
      if (dy < -0.33f) mask |= 1u << 0;   // up
      if (dy >  0.33f) mask |= 1u << 1;   // down
      if (dx < -0.33f) mask |= 1u << 2;   // left
      if (dx >  0.33f) mask |= 1u << 3;   // right
      if (mask)
        return mask;
    }
  }
  for (int i = 0; i < kButtonCount; i++) {
    const TouchButton *b = &g_buttons[i];
    float ex = (x - b->cx) / b->r;
    float ey = (y - b->cy) / (b->r * g_aspect);
    // Hit area slightly larger than the drawn circle: fewer missed taps.
    if (ex * ex + ey * ey <= 1.6f)
      return 1u << b->control;
  }
  return 0;
}

// Turns the finger's offset from the stick origin into the same 4 directions a
// d-pad would produce, diagonals included.
static uint32 StickMask(void) {
  float dx = g_stick.x - g_stick.ox;
  float dy = (g_stick.y - g_stick.oy) / g_aspect;
  float mag = sqrtf(dx * dx + dy * dy);
  if (mag < kStickDead)
    return 0;
  float nx = dx / mag, ny = dy / mag;
  uint32 m = 0;
  // 0.38 ~= sin(22.5 grados): reparte el circulo en ocho sectores iguales.
  if (nx < -0.38f) m |= 1u << 2;
  if (nx >  0.38f) m |= 1u << 3;
  if (ny < -0.38f) m |= 1u << 0;
  if (ny >  0.38f) m |= 1u << 1;
  return m;
}

static void ApplyMask(uint32 before, uint32 after) {
  uint32 changed = before ^ after;
  for (int i = 0; i < kTouchControlCount && changed; i++, changed >>= 1) {
    if (!(changed & 1))
      continue;
    bool pressed = (after >> i) & 1;
    if (i == kTouchOpenMenu) {
      if (pressed)
        OverlayMenu_Toggle();
      continue;
    }
    if (i == kTouchToggleTurbo) {
      // Unlike the rest, turbo latches: on touch there is no point forcing the
      // player to keep a finger held down.
      if (pressed) {
        g_turbo_on = !g_turbo_on;
        TouchInputCallback(kTouchToggleTurbo, g_turbo_on);
      }
      continue;
    }
    TouchInputCallback(i, pressed);
  }
}

static void Recompute(void) {
  uint32 now = 0;
  for (int i = 0; i < kMaxFingers; i++)
    if (g_fingers[i].active)
      now |= g_fingers[i].mask;
  if (now != g_held) {
    ApplyMask(g_held, now);
    g_held = now;
  }
}

static FingerState *FindFinger(SDL_FingerID id, bool create) {
  for (int i = 0; i < kMaxFingers; i++)
    if (g_fingers[i].active && g_fingers[i].id == id)
      return &g_fingers[i];
  if (!create)
    return NULL;
  for (int i = 0; i < kMaxFingers; i++) {
    if (!g_fingers[i].active) {
      g_fingers[i].active = true;
      g_fingers[i].id = id;
      g_fingers[i].mask = 0;
      return &g_fingers[i];
    }
  }
  return NULL;
}

bool TouchControls_HandleEvent(const union SDL_Event *ev) {
  const SDL_Event *e = (const SDL_Event *)ev;
  if (e->type != SDL_FINGERDOWN && e->type != SDL_FINGERUP &&
      e->type != SDL_FINGERMOTION)
    return false;
  // In layout mode they are handled even when the controls are off; otherwise
  // they could not be repositioned to turn them back on.
  if (!TouchControls_Active() && !g_edit_mode)
    return true;  // consumido igualmente: no debe llegar al juego como raton

  SDL_FingerID id = e->tfinger.fingerId;
  float tx = e->tfinger.x, ty = e->tfinger.y;

  if (g_edit_mode) {
    if (e->type == SDL_FINGERDOWN) {
      // The exit button wins: touching it leaves the mode and saves.
      float ex = (tx - kDoneCx) / kDoneR, ey = (ty - kDoneCy) / (kDoneR * g_aspect);
      if (ex * ex + ey * ey <= 1.4f) {
        TouchControls_SetEditMode(false);
        return true;
      }
      // Grab the nearest control within a generous radius.
      float best = 0.10f;
      g_drag = kDragNone;
      for (int i = 0; i < kButtonCount; i++) {
        float dx = tx - g_buttons[i].cx, dy = (ty - g_buttons[i].cy) / g_aspect;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < best) { best = d; g_drag = i; }
      }
      if (!UseStick()) {
        float dx = tx - g_pad_cx, dy = (ty - g_pad_cy) / g_aspect;
        float d = sqrtf(dx * dx + dy * dy);
        if (d < best) { best = d; g_drag = kDragPad; }
      }
      g_drag_id = id;
    } else if (e->type == SDL_FINGERMOTION && g_drag != kDragNone &&
               g_drag_id == id) {
      // Clamped inside the screen so a button can't be lost off-edge.
      float x = tx < 0.03f ? 0.03f : tx > 0.97f ? 0.97f : tx;
      float y = ty < 0.04f ? 0.04f : ty > 0.96f ? 0.96f : ty;
      if (g_drag == kDragPad) { g_pad_cx = x; g_pad_cy = y; }
      else { g_buttons[g_drag].cx = x; g_buttons[g_drag].cy = y; }
    } else if (e->type == SDL_FINGERUP && g_drag_id == id) {
      g_drag = kDragNone;
      SaveLayout();
    }
    return true;
  }

  if (e->type == SDL_FINGERUP) {
    FingerState *f = FindFinger(id, false);
    if (f)
      f->active = false;
    if (g_stick.active && g_stick.id == id)
      g_stick.active = false;
    Recompute();
    return true;
  }

  if (OverlayMenu_IsOpen()) {
    // A tap inside the panel drives the menu itself. Outside it only the menu
    // button responds, so the menu can always be closed again.
    if (e->type == SDL_FINGERDOWN && OverlayMenu_HandleTouch(tx, ty))
      return true;
    FingerState *f = FindFinger(id, true);
    if (f)
      f->mask = HitTest(tx, ty) & (1u << kTouchOpenMenu);
    Recompute();
    return true;
  }

  FingerState *f = FindFinger(id, true);
  if (!f)
    return true;

  if (g_stick.active && g_stick.id == id) {
    g_stick.x = tx;
    g_stick.y = ty;
    f->mask = StickMask();
  } else {
    uint32 hit = HitTest(tx, ty);
    if (hit == 0 && UseStick() && !g_stick.active && tx < kStickZoneRight &&
        e->type == SDL_FINGERDOWN) {
      // The stick is born right under the finger.
      g_stick.active = true;
      g_stick.id = id;
      g_stick.ox = g_stick.x = tx;
      g_stick.oy = g_stick.y = ty;
      f->mask = 0;
    } else {
      f->mask = hit;
    }
  }
  Recompute();
  return true;
}

// ---------------------------------------------------------------------------
// Dibujado
// ---------------------------------------------------------------------------
static void DrawDisc(uint8 *pixels, size_t pitch, int w, int h,
                     float cx, float cy, float r, uint32 color, int alpha) {
  int px = (int)(cx * w), py = (int)(cy * h);
  int rx = (int)(r * w), ry = (int)(r * w);  // circulo: mismo radio en pixeles
  for (int y = -ry; y <= ry; y++) {
    int yy = py + y;
    if (yy < 0 || yy >= h)
      continue;
    // Half-width of this row, so the edge comes out curved and not square.
    int half = (int)(sqrtf((float)(rx * rx - (y * y))) + 0.5f);
    OverlayDraw_Rect(pixels, pitch, w, h, px - half, yy, px + half, yy + 1,
                     color, alpha);
  }
}

void TouchControls_Draw(uint8 *pixels, size_t pitch, int width, int height) {
  // In layout mode they are always drawn, even when hidden by automatic mode:
  // otherwise there would be nothing to reposition.
  if (!TouchControls_Active() && !g_edit_mode)
    return;
  g_aspect = (float)width / (float)height;

  int scale = height / 220;
  if (scale < 1) scale = 1;
  const uint32 kFill = 0xe8e8f0, kEdge = 0x202030;
  const int kAlpha = g_edit_mode ? 150 : 70, kAlphaOn = 150;

  // Floating stick: drawn only while a finger is down, so it doesn't cover the
  // game the rest of the time.
  if (UseStick() && !g_edit_mode) {
    if (g_stick.active) {
      DrawDisc(pixels, pitch, width, height, g_stick.ox, g_stick.oy,
               kStickRange, kEdge, 70);
      // The knob is capped to the stick radius even if the finger goes far.
      float dx = g_stick.x - g_stick.ox;
      float dy = (g_stick.y - g_stick.oy) / g_aspect;
      float mag = sqrtf(dx * dx + dy * dy);
      if (mag > kStickRange) {
        dx *= kStickRange / mag;
        dy *= kStickRange / mag;
      }
      DrawDisc(pixels, pitch, width, height, g_stick.ox + dx,
               g_stick.oy + dy * g_aspect, kStickRange * 0.45f, kFill, kAlphaOn);
    }
  } else
  // D-pad: two crossed bars.
  {
    int cx = (int)(g_pad_cx * width), cy = (int)(g_pad_cy * height);
    int arm = (int)(kPadArm * width), thick = arm / 3;
    int aUp = (g_held & 1u << 0) ? kAlphaOn : kAlpha;
    int aDn = (g_held & 1u << 1) ? kAlphaOn : kAlpha;
    int aLf = (g_held & 1u << 2) ? kAlphaOn : kAlpha;
    int aRt = (g_held & 1u << 3) ? kAlphaOn : kAlpha;
    OverlayDraw_Rect(pixels, pitch, width, height, cx - thick, cy - arm,
                     cx + thick, cy - thick, kFill, aUp);
    OverlayDraw_Rect(pixels, pitch, width, height, cx - thick, cy + thick,
                     cx + thick, cy + arm, kFill, aDn);
    OverlayDraw_Rect(pixels, pitch, width, height, cx - arm, cy - thick,
                     cx - thick, cy + thick, kFill, aLf);
    OverlayDraw_Rect(pixels, pitch, width, height, cx + thick, cy - thick,
                     cx + arm, cy + thick, kFill, aRt);
    OverlayDraw_Rect(pixels, pitch, width, height, cx - thick, cy - thick,
                     cx + thick, cy + thick, kFill, kAlpha);
  }

  for (int i = 0; i < kButtonCount; i++) {
    const TouchButton *b = &g_buttons[i];
    // Turbo latches, so it is highlighted by its state, not by the finger.
    bool on = (b->control == kTouchToggleTurbo)
                  ? g_turbo_on
                  : ((g_held >> b->control) & 1) != 0;
    DrawDisc(pixels, pitch, width, height, b->cx, b->cy, b->r, kEdge,
             on ? 170 : 90);
    DrawDisc(pixels, pitch, width, height, b->cx, b->cy, b->r * 0.82f, kFill,
             on ? kAlphaOn : kAlpha);
    int tw = (int)strlen(b->cap) * 6 * scale;
    OverlayDraw_Text(pixels, pitch, width, height,
                     (int)(b->cx * width) - tw / 2,
                     (int)(b->cy * height) - 4 * scale, b->cap, 0x303040, scale);
  }

  if (!g_edit_mode)
    return;

  // Banner and exit button for layout mode.
  const char *hint = OverlayMenu_Text("DRAG THE BUTTONS",
                                      "ARRASTRA LOS BOTONES");
  int tw = (int)strlen(hint) * 6 * scale;
  OverlayDraw_Rect(pixels, pitch, width, height, 0, 0, width, 13 * scale,
                   0x101018, 200);
  OverlayDraw_Text(pixels, pitch, width, height, (width - tw) / 2, 3 * scale,
                   hint, 0xf0d060, scale);

  DrawDisc(pixels, pitch, width, height, kDoneCx, kDoneCy, kDoneR, 0x206020, 220);
  int dw = 4 * 6 * scale;
  OverlayDraw_Text(pixels, pitch, width, height,
                   (int)(kDoneCx * width) - dw / 2,
                   (int)(kDoneCy * height) - 4 * scale, "OK", 0xffffff, scale);
}
