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
# an egg drawn as sequencer columns, cracked open along a zigzag; the lid is lifted and a robot peeks out
N_BARS, TILT, RING = 7, -8, False
LID_ANGLE, LID_LIFT = -14.0, (8.0, -80.0)

def egg_span(x, pts):
    """top and bottom of the egg outline at x"""
    ys = []
    for (x0, y0), (x1, y1) in zip(pts, pts[1:] + pts[:1]):
        if (x0 - x) * (x1 - x) <= 0 and x0 != x1:
            ys.append(y0 + (y1 - y0) * (x - x0) / (x1 - x0))
    return (min(ys), max(ys)) if len(ys) >= 2 else None

def xform(pts, ang, cx, cy, tx=0.0, ty=0.0):
    c, s_ = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    return [(cx + (x - cx) * c - (y - cy) * s_ + tx, cy + (x - cx) * s_ + (y - cy) * c + ty) for x, y in pts]

def motif(uid, pal):
    eg = egg_pts(a=94, b=126, cy=CY + 40, n=720)
    xs = [p[0] for p in eg]
    x0, x1 = min(xs), max(xs)
    pitch = (x1 - x0) / N_BARS
    w, r, gap = pitch * 0.66, pitch * 0.33, 18.0
    base = CY + 40
    crack = [base + (-20 if i % 2 else 20) for i in range(N_BARS)]   # one triangular tooth every two columns
    hinge = (x0 + 4, crack[0])
    # bounding box of the whole drawing (bottom shell + lifted lid), tilted, so it can sit on the disc centre
    bottom = [p for p in eg if p[1] > base - 20]
    lid = xform([p for p in eg if p[1] < base + 20], LID_ANGLE, *hinge, *LID_LIFT)
    allp = xform(bottom + lid, TILT, CX, CY)
    dx = CX - (min(p[0] for p in allp) + max(p[0] for p in allp)) / 2
    dy = CY - (min(p[1] for p in allp) + max(p[1] for p in allp)) / 2
    s = [f'<clipPath id="ce{uid}"><path d="{poly(eg)}"/></clipPath>']
    # the robot is cut by the top edge of the bottom shell, so it rises out of it
    rim = [(x0 - 40, crack[0] + gap / 2)]
    for i, cy in enumerate(crack):
        rim += [(x0 + pitch * i, cy + gap / 2), (x0 + pitch * (i + 1), cy + gap / 2)]
    rim += [(x1 + 40, crack[-1] + gap / 2), (x1 + 40, 0), (x0 - 40, 0)]
    s.append(f'<clipPath id="cr{uid}"><path d="{poly(rim)}"/></clipPath>')
    s.append(f'<g transform="translate({dx:.2f} {dy:.2f}) rotate({TILT} {CX} {CY})">')
    # robot head
    hx, hw, hh = CX + 6, 108.0, 96.0
    htop = base - 86
    B, V, E, H = pal["bar"], pal["visor"], pal["accent"], pal["head"]
    # bottom shell
    s.append(f'<g clip-path="url(#ce{uid})">')
    for i, cy in enumerate(crack):
        cx = x0 + pitch * (i + 0.5)
        span = egg_span(cx, eg)
        if span:
            s.append(f'<rect x="{cx - w / 2:.1f}" y="{cy + gap / 2:.1f}" width="{w:.1f}" height="{span[1] - cy - gap / 2 + 60:.1f}" rx="{r:.1f}" fill="{B}"/>')
    s.append('</g>')
    if pal.get("halo"):   # mono: a cut in the disc colour keeps the head apart from the shells
        h = pal["halo"]
        s.append(f'<g clip-path="url(#cr{uid})"><rect x="{hx - hw / 2 - 9}" y="{htop - 9}" width="{hw + 18}" height="{hh + 58}" rx="32" fill="{h}"/>'
                 f'<rect x="{hx - 12.5}" y="{htop - 45}" width="25" height="50" rx="12.5" fill="{h}"/>'
                 f'<rect x="{hx - hw / 2 - 19}" y="{htop + 15}" width="{hw + 38}" height="46" rx="12" fill="{h}"/></g>')
    s.append(f'<g clip-path="url(#cr{uid})">')
    # the head is a halftone: tiny dots at the antenna that grow and merge into the solid silhouette below
    sil = (f'<rect x="{hx - 3.5}" y="{htop - 26}" width="7" height="30" rx="3.5"/>'
           f'<circle cx="{hx}" cy="{htop - 28}" r="8.5"/>'
           + "".join(f'<rect x="{hx + side * (hw / 2 + 3) - 7:.1f}" y="{htop + 24}" width="14" height="28" rx="5"/>' for side in (-1, 1))
           + f'<rect x="{hx - hw / 2}" y="{htop}" width="{hw}" height="{hh + 40}" rx="24"/>')
    s.append(f'<clipPath id="ch{uid}">{sil}</clipPath><g clip-path="url(#ch{uid})">')
    P = 11.0
    y_from, y_full = htop - 38, htop + 82
    y = y_from
    row = 0
    while y < htop + hh + 40:
        f = min(1.0, max(0.0, (y - y_from) / (y_full - y_from)))
        rad = P * (0.15 + 0.45 * f ** 1.5)                       # 0.74 P overlaps the neighbours: solid
        x = hx - hw / 2 - 20 + (P / 2 if row % 2 else 0)
        while x < hx + hw / 2 + 20:
            s.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{rad:.2f}" fill="{H}"/>')
            x += P
        y += P * 0.866
        row += 1
    s.append('</g>')
    s.append(f'<rect x="{hx - 34}" y="{htop + 24}" width="68" height="24" rx="12" fill="{V}"/>')
    s.append(f'<circle cx="{hx + 12}" cy="{htop + 36}" r="7" fill="{E}"/>')
    s.append('</g>')
    # lid, lifted and tipped back on its left hinge
    s.append(f'<g transform="translate({LID_LIFT[0]} {LID_LIFT[1]}) rotate({LID_ANGLE} {hinge[0]:.1f} {hinge[1]:.1f})"><g clip-path="url(#ce{uid})">')
    for i, cy in enumerate(crack):
        cx = x0 + pitch * (i + 0.5)
        span = egg_span(cx, eg)
        if span:
            s.append(f'<rect x="{cx - w / 2:.1f}" y="{span[0] - 60:.1f}" width="{w:.1f}" height="{cy - gap / 2 - span[0] + 60:.1f}" rx="{r:.1f}" fill="{B}"/>')
    s.append('</g></g></g>')
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
        pal = dict(bar=WHITE, head=INK, visor=WHITE, accent=INK)
    else:
        s.append(f'<circle cx="{CX}" cy="{CY}" r="240" fill="{INK if kind == "ink" else WHITE}"/>')
        pal = dict(bar=WHITE if kind == "ink" else INK, head=WHITE if kind == "ink" else INK, accent=WHITE if kind == "ink" else INK, visor=INK if kind == "ink" else WHITE, halo=INK if kind == "ink" else WHITE)
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
