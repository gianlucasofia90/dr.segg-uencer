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

# ---------- the abstract mark ----------
# an egg drawn as sequencer columns; the crack is a zigzag of gaps across them; one column is the playhead
N_BARS, RING, PLAYHEAD, TILT = 7, False, 4, -8

def egg_span(x, pts):
    """top and bottom of the egg outline at x"""
    ys = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
        if (x0 - x) * (x1 - x) <= 0 and x0 != x1:
            ys.append(y0 + (y1 - y0) * (x - x0) / (x1 - x0))
    return (min(ys), max(ys)) if len(ys) >= 2 else None

def motif(uid, pal):
    eg = egg_pts(a=118, b=158, cy=CY + 10, n=720)
    xs = [p[0] for p in eg]
    x0, x1 = min(xs), max(xs)
    pitch = (x1 - x0) / N_BARS
    w = pitch * 0.66
    ca, sa = math.cos(math.radians(TILT)), math.sin(math.radians(TILT))
    rot = [(CX + (x - CX) * ca - (y - CY) * sa, CY + (x - CX) * sa + (y - CY) * ca) for x, y in eg]
    dx = CX - (min(p[0] for p in rot) + max(p[0] for p in rot)) / 2
    dy = CY - (min(p[1] for p in rot) + max(p[1] for p in rot)) / 2
    s = [f'<clipPath id="ce{uid}"><path d="{poly(eg)}"/></clipPath>',
         f'<g transform="translate({dx:.2f} {dy:.2f}) rotate({TILT} {CX} {CY})"><g clip-path="url(#ce{uid})">']
    for i in range(N_BARS):
        cx = x0 + pitch * (i + 0.5)
        span = egg_span(cx, eg)
        if not span:
            continue
        top, bot = span
        cy = CY + 6 + (-24 if i % 2 else 24)   # crack: one triangular tooth every two columns
        gap = 20.0
        col = pal["accent"] if i == PLAYHEAD else pal["bar"]
        r = w / 2
        # columns run past the outline and are cut by it, so the egg reads; only the crack ends are rounded
        s.append(f'<rect x="{cx - w / 2:.1f}" y="{top - 60:.1f}" width="{w:.1f}" height="{cy - gap / 2 - top + 60:.1f}" rx="{r:.1f}" fill="{col}"/>')
        s.append(f'<rect x="{cx - w / 2:.1f}" y="{cy + gap / 2:.1f}" width="{w:.1f}" height="{bot - cy - gap / 2 + 60:.1f}" rx="{r:.1f}" fill="{col}"/>')
    s.append('</g></g>')
    return s

R_IN, R_OUT, STRIPE = 206.0, 236.0, 16.0

def disc(uid, cx, cy, R, kind):
    """kind: colour | ink | white. R = outer radius (240 in the 512 icon)."""
    k = R / 240.0
    s = [f'<g transform="translate({cx - CX * k:.2f} {cy - CY * k:.2f}) scale({k:.5f})">']
    if kind == "colour":
        s.append(f'<defs><linearGradient id="g{uid}" x1="0.15" y1="0" x2="0.85" y2="1">'
                 f'<stop offset="0" stop-color="#ff5a3c"/><stop offset="0.45" stop-color="#c8102e"/><stop offset="1" stop-color="#5e0718"/></linearGradient></defs>')
        s.append(f'<circle cx="{CX}" cy="{CY}" r="240" fill="url(#g{uid})"/>')
        s.append(f'<circle cx="{CX}" cy="{CY}" r="237.1" fill="none" stroke="{DEEP}" stroke-opacity="0.55" stroke-width="5.8"/>')
        # ring like SONIC GRAIN: short arcs, yellow at the top fading to black at the bottom, symmetric
        rr, n = 217.2, 120
        for i in range(n):
            a0, a1 = 2 * math.pi * i / n, 2 * math.pi * (i + 1.4) / n
            t = (1 - math.cos(2 * math.pi * (i + 0.5) / n)) / 2          # 0 top, 1 bottom
            # bottom half solid black; the top half fades from black at the sides to yellow at the top (through amber, not olive)
            u = min(1.0, t / 0.5)
            col = INK if u >= 1 else (lerp(YOLK, "#d27a0c", u / 0.5) if u < 0.5 else lerp("#d27a0c", INK, (u - 0.5) / 0.5))
            p0 = (CX + rr * math.sin(a0), CY - rr * math.cos(a0))
            p1 = (CX + rr * math.sin(a1), CY - rr * math.cos(a1))
            s.append(f'<path d="M{p0[0]:.1f},{p0[1]:.1f} A{rr},{rr} 0 0 1 {p1[0]:.1f},{p1[1]:.1f}" fill="none" stroke="{col}" stroke-width="8.4"/>')
        pal = dict(bar=WHITE, accent=YOLK)
    else:
        s.append(f'<circle cx="{CX}" cy="{CY}" r="240" fill="{INK if kind == "ink" else WHITE}"/>')
        pal = dict(bar=WHITE if kind == "ink" else INK, accent=WHITE if kind == "ink" else INK)
    if RING:
        ring = (f"M{CX - R_OUT},{CY} a{R_OUT},{R_OUT} 0 1 0 {2 * R_OUT},0 a{R_OUT},{R_OUT} 0 1 0 {-2 * R_OUT},0Z "
                f"M{CX - R_IN},{CY} a{R_IN},{R_IN} 0 1 0 {2 * R_IN},0 a{R_IN},{R_IN} 0 1 0 {-2 * R_IN},0Z")
        stripe = {"colour": INK, "ink": WHITE, "white": INK}[kind]
        s.append(f'<clipPath id="cr{uid}"><path d="{ring}" clip-rule="evenodd"/></clipPath>')
        s.append(f'<defs><pattern id="hz{uid}" patternUnits="userSpaceOnUse" width="{2 * STRIPE}" height="{2 * STRIPE}" patternTransform="rotate(45 {CX} {CY})">'
                 f'<rect width="{STRIPE}" height="{2 * STRIPE}" fill="{stripe}"/></pattern></defs>')
        if kind == "colour":
            s.append(f'<path d="{ring}" fill-rule="evenodd" fill="{YOLK}"/>')
        s.append(f'<rect width="512" height="512" fill="url(#hz{uid})" clip-path="url(#cr{uid})"/>')
    s += motif(uid, pal)
    s.append('</g>')
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
