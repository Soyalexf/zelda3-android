// Math for the locked camera used by widescreen mode.
//
// The extended view asks for more world than the current area has loaded.
// Instead of painting black (or worse, the tilemap wrapping around) once the
// edge is reached, the visual camera is locked inside the area bounds and Link
// keeps moving within the frame. This is a render-only shift: the game's own
// logical camera is never touched.
#ifndef ZELDA3_WIDE_CAMERA_H_
#define ZELDA3_WIDE_CAMERA_H_

int WideCamera_IsMapMenu(int module, int submodule);

// Rebuilds a 16-bit value as the integer nearest to 'reference', so wrapping
// coordinates can be compared without special cases.
int WideCamera_Unwrap16(int value, int reference);

int WideCamera_ClampToBounds(int logical_x, int left_bound,
                             int right_bound, int margin);
int WideCamera_FindDungeonTransitionEnd(int start_x, int direction,
                                        int target);
// Spreads the gap between the logical and visual camera across the transition,
// so catching back up is gradual instead of a jump.
int WideCamera_InterpolateTransition(int logical_x, int start_logical_x,
                                     int start_visual_x, int end_offset,
                                     int direction, int distance);

#endif  // ZELDA3_WIDE_CAMERA_H_
