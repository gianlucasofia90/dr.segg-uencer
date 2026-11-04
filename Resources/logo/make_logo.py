"""DR. SEGG-UENCER logo set, built the same way as the SONIC GRAIN set:
a shaded disc, a two-colour ring, the motif clipped inside and tilted -14 deg, one yellow accent,
and the wordmark in Rubik Black Italic converted to paths."""
import math, os, sys
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

REPO = sys.argv[1]
OUT = os.path.join(REPO, "Resources/logo")
os.makedirs(OUT, exist_ok=True)
FONT = TTFont(os.path.join(REPO, "Resources/fonts/Rubik-BlackItalic.ttf"))

INK, WHITE, YOLK, CHROME = "#17191d", "#fff", "#ffd21f", "#e8ecf1"
DEEP = "#3a0610"          # vignette and rim, a darker groundAlt

def lerp(a, b, t):
    a = [int(a[i:i + 2], 16) for i in (1, 3, 5)]; b = [int(b[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(x + (y - x) * t):02x}" for x, y in zip(a, b))

# ---------- the motif, in a 512 box ----------
CX = CY = 256.0

def egg_pts(a=92.0, b=124.0, cx=CX, cy=CY + 6, n=160):
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        x = a * math.sin(t) * (1 - 0.16 * math.cos(t))   # narrower at the top
        y = -b * math.cos(t)
        pts.append((cx + x, cy + y))
    return pts

def poly(pts, close=True):
    d = "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts)
    return d + ("Z" if close else "")

# crack: a gate pattern, 8 steps across the egg (levels in units of h)
LEVELS = [0, 1, 0, 2, 1, 0]
def crack(cy=CY + 8, x0=CX - 105, x1=CX + 105, h=17.0):
    w = (x1 - x0) / len(LEVELS)
    pts = []
    for i, lv in enumerate(LEVELS):
        y = cy - lv * h
        pts += [(x0 + i * w, y), (x0 + (i + 1) * w, y)]
    return pts

def motif(uid, scale, shell_top, shell_bot, shade, yolk, dot_on, dot_off, edge):
    """returns svg elements in the 512 box; caller wraps them in a transform"""
    eg = egg_pts()
    cr = crack()
    top_clip = poly([(CX - 200, CY - 220)] + [(cr[0][0] - 80, cr[0][1])] + cr + [(cr[-1][0] + 80, cr[-1][1]), (CX + 200, CY - 220)])
    bot_clip = poly([(CX - 200, CY + 220)] + [(cr[0][0] - 80, cr[0][1])] + cr + [(cr[-1][0] + 80, cr[-1][1]), (CX + 200, CY + 220)])
    yolk_pts = egg_pts(a=88, b=120, cy=CY + 6)
    s = []
    s.append(f'<clipPath id="ct{uid}"><path d="{top_clip}"/></clipPath>')
    s.append(f'<clipPath id="cb{uid}"><path d="{bot_clip}"/></clipPath>')
    if shade:
        s.append(f'<defs><linearGradient id="sh{uid}" x1="0" y1="0" x2="1" y2="1">'
                 f'<stop offset="0" stop-color="#fff"/><stop offset="0.55" stop-color="#fff"/><stop offset="1" stop-color="{CHROME}"/></linearGradient></defs>')
    fill = f"url(#sh{uid})" if shade else shell_bot
    # 16 steps around the egg, Euclidean 5 of 16
    on = {round(i * 16 / 5) for i in range(5)}
    for i in range(16):
        ang = math.radians(-90 + i * 22.5)
        x, y = CX + 178 * math.cos(ang), CY + 178 * math.sin(ang)
        c, r = (dot_on, 8.0) if i in on else (dot_off, 5.0)
        s.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{c}"/>')
    s.append(f'<g transform="rotate(-14 {CX} {CY})">')
    if yolk:
        s.append(f'<path d="{poly(yolk_pts)}" fill="{yolk}"/>')
    s.append(f'<path d="{poly(eg)}" fill="{fill}" clip-path="url(#cb{uid})"/>')
    # top shell lifted and tipped back: the sequence hatches
    s.append(f'<g transform="translate(0 -24)">'
             f'<path d="{poly(eg)}" fill="{fill if shade else shell_top}" clip-path="url(#ct{uid})"/></g>')
    s.append('</g>')
    return s

def disc(uid, cx, cy, R, kind):
    """kind: colour | ink | white. R = outer radius (240 in the 512 icon)."""
    k = R / 240.0
    s = []
    if kind == "colour":
        s.append(f'<defs><linearGradient id="g{uid}" x1="0" y1="0" x2="1" y2="1">'
                 f'<stop offset="0" stop-color="#e8344d"/><stop offset="0.5" stop-color="#c8102e"/><stop offset="1" stop-color="#860a1e"/></linearGradient>'
                 f'<radialGradient id="v{uid}" cx="0.5" cy="0.5" r="0.5">'
                 f'<stop offset="0.6" stop-color="{DEEP}" stop-opacity="0"/><stop offset="0.86" stop-color="{DEEP}" stop-opacity="0.32"/><stop offset="1" stop-color="{DEEP}" stop-opacity="0.62"/></radialGradient></defs>')
        s.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="url(#g{uid})"/>')
        s.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="url(#v{uid})"/>')
        s.append(f'<circle cx="{cx}" cy="{cy}" r="{R - 2.9 * k:.1f}" fill="none" stroke="{DEEP}" stroke-opacity="0.55" stroke-width="{5.8 * k:.1f}"/>')
        # ring: yellow at the top fading to chrome at the bottom, drawn in short arcs like SONIC GRAIN
        rr, n = 217.2 * k, 120
        for i in range(n):
            a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1.4) / n
            t = (1 - math.cos(2 * math.pi * (i + 0.5) / n)) / 2          # 0 top, 1 bottom
            col = lerp(YOLK, CHROME, t ** 1.3)
            p0 = (cx + rr * math.sin(a0), cy - rr * math.cos(a0))
            p1 = (cx + rr * math.sin(a1), cy - rr * math.cos(a1))
            s.append(f'<path d="M{p0[0]:.1f},{p0[1]:.1f} A{rr:.1f},{rr:.1f} 0 0 1 {p1[0]:.1f},{p1[1]:.1f}" fill="none" stroke="{col}" stroke-width="{8.4 * k:.1f}"/>')
    else:
        s.append(f'<circle cx="{cx}" cy="{cy}" r="{R}" fill="{INK if kind == "ink" else WHITE}"/>')
    s.append(f'<clipPath id="ci{uid}"><circle cx="{CX}" cy="{CY}" r="206.4"/></clipPath>')
    if kind == "colour":
        m = motif(uid, k, WHITE, WHITE, True, YOLK, WHITE, "#ffffff55", INK)
    elif kind == "ink":
        m = motif(uid, k, WHITE, WHITE, False, None, WHITE, "#ffffff55", None)
    else:
        m = motif(uid, k, INK, INK, False, None, INK, "#17191d55", None)
    s.append(f'<g transform="translate({cx - CX * k:.2f} {cy - CY * k:.2f}) scale({k:.5f})"><g clip-path="url(#ci{uid})">')
    s += m
    s.append('</g></g>')
    return s

# ---------- wordmark ----------
GS = FONT.getGlyphSet(); CMAP = FONT.getBestCmap(); UPM = FONT["head"].unitsPerEm
def text_path(txt, size, track=-0.01):
    sc = size / UPM
    pen = SVGPathPen(GS)
    x = 0.0
    for ch in txt:
        g = CMAP[ord(ch)]
        GS[g].draw(TransformPen(pen, (sc, 0, 0, -sc, x, 0)))
        x += GS[g].width * sc + track * size
    return pen.getCommands(), x - track * size

def bbox_w(txt, size):
    """ink extent (left, right) of the italic text, so it can be centred and clipped correctly"""
    from fontTools.pens.boundsPen import BoundsPen
    sc = size / UPM; bp = BoundsPen(GS); x = 0.0
    for ch in txt:
        g = CMAP[ord(ch)]
        GS[g].draw(TransformPen(bp, (sc, 0, 0, -sc, x, 0)))
        x += GS[g].width * sc - 0.01 * size
    return bp.bounds

NAME = "DR. SEGG-UENCER"

def svg(w, h, body, name):
    with open(os.path.join(OUT, name), "w") as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.0f} {h:.0f}" width="{w:.0f}" height="{h:.0f}">\n' + "\n".join(body) + "\n</svg>\n")

# icon + mono
svg(512, 512, disc("i", 256, 256, 240, "colour"), "drsegg-mark.svg")
svg(512, 512, disc("k", 256, 256, 240, "ink"), "drsegg-icon-mono-ink.svg")
svg(512, 512, disc("w", 256, 256, 240, "white"), "drsegg-icon-mono-white.svg")

# wordmark alone (caps 84 at size 120, as SONIC GRAIN)
d, adv = text_path(NAME, 120)
b = bbox_w(NAME, 120)
ww = b[2] - b[0] + 16
for col, nm in ((INK, "ink"), (WHITE, "white")):
    svg(ww, 100, [f'<path transform="translate({8 - b[0]:.1f},92.0)" d="{d}" fill="{col}"/>'], f"drsegg-wordmark-{nm}.svg")

# horizontal lockup: disc r 92 at 100, text size 112 from x 228
d, adv = text_path(NAME, 112)
b = bbox_w(NAME, 112)
lw = 228 - b[0] + b[2] + 10
for theme, col in (("dark", WHITE), ("light", INK)):
    svg(lw, 200, disc("l" + theme, 100, 100.0, 92, "colour") +
        [f'<path transform="translate({228 - b[0]:.1f},139.2)" d="{d}" fill="{col}"/>'], f"drsegg-lockup-{theme}.svg")

# stacked: disc r 150, then DR. / SEGG-UENCER centred
l1, l2 = "DR.", "SEGG-UENCER"
d1, _ = text_path(l1, 120); b1 = bbox_w(l1, 120)
d2, _ = text_path(l2, 120); b2 = bbox_w(l2, 120)
sw = max(b1[2] - b1[0], b2[2] - b2[0]) + 60
for theme, col in (("dark", WHITE), ("light", INK)):
    svg(sw, 558, disc("s" + theme, sw / 2, 160, 150, "colour") +
        [f'<path transform="translate({sw / 2 - (b1[0] + b1[2]) / 2:.1f},434.0)" d="{d1}" fill="{col}"/>',
         f'<path transform="translate({sw / 2 - (b2[0] + b2[2]) / 2:.1f},542.0)" d="{d2}" fill="{col}"/>'], f"drsegg-stacked-{theme}.svg")
print("ok", ww, lw, sw)
