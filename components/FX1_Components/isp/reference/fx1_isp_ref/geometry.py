"""Output channel geometry (MAS v1.07 §6.20.1-6.20.4, §8.3).

Each channel crops the post-IQ YUV444 active image (half-open, even, inside
the image; disabled = full image) and maps the crop to a Resizer preset or
passes it through (FullRes, disabled scaler). A setup is validated into a set
with a GEOMETRY_STATUS.ERROR priority code (CSR v1.07: the lowest applicable
code wins). Frames use the set captured at their accepted SOF; the IDLE-class
geometry words do not change while a frame is in flight, so the reference
reads the current registers.
"""

# RESIZER_CTRL.SCALE presets (Table 6-122); 0 is FullRes, 15 and 22..31 reserved.
MODES = {1: (3840, 2160), 2: (2560, 1440), 3: (2880, 1620), 4: (2304, 1296), 5: (1920, 1080), 6: (1280, 720),
         7: (960, 540), 8: (640, 360), 9: (2592, 1944), 10: (2048, 1536), 11: (1600, 1200), 12: (1280, 960),
         13: (800, 600), 14: (640, 480), 16: (640, 640), 17: (640, 384), 18: (640, 352), 19: (512, 512),
         20: (416, 416), 21: (320, 320)}
CH2_CODES = {0, 8, 14, 16, 17, 18, 19, 20, 21}       # CSR v1.07 CH2_RESIZER_CTRL.scale
LIMITS = [(3840, 2160), (3840, 2160), (640, 640)]     # CHc_OUTPUT_LIMIT (Table 6-115)
CTRL = ["RESIZER_CTRL", "CH1_RESIZER_CTRL", "CH2_RESIZER_CTRL"]

OK, INPUT, CROP_BOUNDS, CROP_ODD, MODE, LIMIT, EXCEEDS_CROP = range(7)


def plan(cw, ch, ow, oh):
    """Decimation stages and bilinear steps from a cw x ch crop to ow x oh
    (MAS §6.20.3.4-6.20.3.6); None when the sizes are equal (pass-through)."""
    if (cw, ch) == (ow, oh):
        return None
    k = next((t for t in range(3) if (cw >> t) < 2 * ow and (ch >> t) < 2 * oh), 2)
    while k > 0 and ((cw >> k) < ow or (ch >> k) < oh):
        k -= 1                                        # never decimate below the output
    return k, ow, oh


def derive(ch, w, h, input_ok, crop_en, cx, cy, cw, chh, en, scale):
    """The set of one channel for active w x h: dict(error, crop, out, plan)."""
    crop = (cx, cy, cw, chh) if crop_en else (0, 0, w, h)
    mode_ok = not en or scale == 0 or (scale in MODES and (ch != 2 or scale in CH2_CODES))
    out = crop[2:] if not en or scale == 0 else MODES.get(scale)
    lw, lh = LIMITS[ch]
    if not input_ok or not (32 <= w <= 3840 and 32 <= h <= 2160):
        e = INPUT
    elif crop_en and (cw == 0 or chh == 0 or cx + cw > w or cy + chh > h):
        e = CROP_BOUNDS
    elif crop_en and (cx | cy | cw | chh) & 1:
        e = CROP_ODD
    elif not mode_ok:
        e = MODE
    elif out[0] > lw or out[1] > lh:
        e = LIMIT
    elif out[0] > crop[2] or out[1] > crop[3]:
        e = EXCEEDS_CROP
    else:
        e = OK
    if e != OK:
        return {"error": e, "crop": None, "out": None, "plan": None}
    return {"error": OK, "crop": crop, "out": out, "plan": plan(crop[2], crop[3], out[0], out[1])}


def channel(regs, ch):
    """The set of channel `ch` from the registers."""
    w, h = regs.active_size()
    p = "CH%d_" % ch
    return derive(ch, w, h, regs.f("COMMON_INPUT_FORMAT", "DEPTH") < 2, regs.f(p + "CROP_CTRL", "ENABLE"),
                  regs.f(p + "CROP_ORIGIN", "X"), regs.f(p + "CROP_ORIGIN", "Y"), regs.f(p + "CROP_SIZE", "W"),
                  regs.f(p + "CROP_SIZE", "H"), regs.f(CTRL[ch], "en"), regs.f(CTRL[ch], "scale"))


def for_frame(regs, ch):
    """The crop and plan a frame uses: the channel set, or, when it is not
    valid (only reachable without admission control), the full active image
    unscaled (U4-A1)."""
    s = channel(regs, ch)
    if s["error"] != OK:
        w, h = regs.active_size()
        return {"error": s["error"], "crop": (0, 0, w, h), "out": (w, h), "plan": None}
    return s
