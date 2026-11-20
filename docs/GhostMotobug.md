# Ghost Motobug (Sonic 1, GHZ act 1) — resolved

**Symptom:** a Motobug drawn near (3100..3480, 640..700) that Sonic walks through; the stock layout has no Motobug there.

**Cause:** the layout's Motobug at (3152, 844) is an out-of-bounds placement: no floor beneath it on either collision path (the original's FixBugs build deletes it for this reason). It falls invisibly to y≈2740, where level collision wraps (2048 px) and it drives along the floor of the wrapped row. The port's sprite builder also wrapped Y by `level_screen_y_wrap` in every level, so it was drawn 2048 px higher, on the slope, and flagged on-screen. Sonic's touch test uses the real Y (≈2740), so he passed through it. The original's BuildSprites does not wrap, so there it is never drawn.

**Fix:** `level_sprites_wrap_y` (ScrollEngine.h): sprites and rings wrap Y only when the game says the level loops. Sonic 1 sets it from the top limit (`LevelSizeLoad`: wraps only when limit_top1 < 0); the special stage keeps the old behaviour; Sonic 2 keeps the default (true). Test: `Sprites_YWrapIsTheLevelsHeight`.

**Open:** other out-of-bounds placements in Sonic 1 layouts are not listed yet (scan for objects that never find floor).
