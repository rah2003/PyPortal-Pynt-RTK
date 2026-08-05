# PyPortal Pynt RTK — enclosure rev A, Autodesk Fusion build script.
# Mirrors enclosure/pynt_rtk_enclosure.scad; docs/hardware/enclosure.md is the
# design rationale. Edit the constants below and re-run — the script builds a
# FRESH document each run (it never modifies the active one).
#
# Run inside Fusion: UTILITIES > ADD-INS > Scripts and Add-Ins (Shift+S) >
# green "+" next to My Scripts > pick this file's FOLDER > select
# "pynt_rtk_enclosure" > Run.
#
# Frame: X = width (0 at center), Y = height (0 at exterior bottom face, +up),
# Z = depth (0 at exterior front face, + rearward). Units below are mm
# (converted to Fusion's internal cm at the API boundary).
#
# CARRIER_* comes from the ArduSimple USB-C carrier STEP; owner confirmed
# 2026-07-19 the mini-USB board on hand is identical except the connector.

import adsk.core, adsk.fusion, traceback, math, os

# ---------- part selector ----------
PART = 'assembly'   # 'assembly' | 'base_test' | 'bezel_test' — the two fit
                    # prints from docs/hardware/enclosure.md; test parts are
                    # auto-exported as STL to enclosure/prints/

# ---------- shell ----------
WALL, FRONT, COVER_T = 2.8, 3.0, 2.8
FLOOR_T = 1.4                     # rev A7: bottom wall halved so the USB plug and
                                  # SD card can reach their connectors
INT_W, INT_H, INT_D = 55.0, 83.0, 52.0   # depth fits the full antenna pad
EXT_W = INT_W + 2 * WALL          # 60.6
EXT_H = INT_H + 2 * WALL          # 88.6
SHELL_D = FRONT + INT_D           # 55.0 = AXIS_Z + ANT_PAD_D/2 + 1 margin
TOTAL_D = SHELL_D + COVER_T       # 57.8
FUDGE = 0.15                      # printer XY compensation per side

# ---------- pole / antenna axis ----------
AXIS_Z = 31.0                     # nut boss front face must clear USB plug (z<14.5)

# ---------- PyPortal Pynt (#4465), exact from Adafruit CAD ----------
PYNT_MIRROR = -1                  # confirmed 2026-07-19: USB lands on the viewer's
                                  # LEFT (+x) looking at the screen in D2 portrait
PYNT_BODY_W = 43.180
PYNT_YC = PYNT_BODY_W / 2         # 21.59 mirror line
PYNT_Y0 = FLOOR_T                 # rev A7: ZERO standoff — the board's bottom edge
                                  # sits ON the interior floor (fit finding: any
                                  # gap makes the USB plug / SD card unreachable)
PYNT_HOLES_XB = [2.540, 64.262]   # along the 66.8 edge (vertical here)
PYNT_HOLES_YB = [-2.540, 45.339]  # across, incl. the ear offsets
FOAM_GAP, GLASS_H, PCB_T = 0.5, 3.9, 1.6
Z_PCB_FRONT = FRONT + FOAM_GAP + GLASS_H       # 7.4
WIN_XB = [8.49, 58.74]            # viewing window, board coords
WIN_YB = [2.19, 40.79]
WIN_MARGIN = 0.6
USB_YB_C, USB_SLOT_W, USB_SLOT_Z = 5.08, 13.0, (8.0, 13.0)
SD_YB_C, SD_SLOT_W, SD_SLOT_Z = 19.8, 12.0, (8.2, 11.5)  # front edge aligned w/ USB
LIGHT_XB, LIGHT_YB = 3.94, 11.43

def bx(yb): return PYNT_MIRROR * (yb - PYNT_YC)   # board Yb -> enclosure X
def by(xb): return PYNT_Y0 + xb                    # board Xb -> enclosure Y

# ---------- carrier + Lite stack (on the back cover) ----------
CARRIER_HOLE_DX, CARRIER_HOLE_DY = 20.0, 35.0
CARRIER_Y0 = 6.0                  # carrier bottom edge above interior floor
CARRIER_BOSS_H = 3.5
STACK_H, LITE_TOP_OFF, RADIO_VOID = 15.3, 9.6, 12.0

# ---------- antenna / SMA bulkhead ----------
SMA_HOLE_D = 6.5                  # round bulkhead — owner's adapter has no D-flat
ANT_PAD_D, ANT_RECESS_D, ANT_PAD_H, ANT_RECESS_H = 46.0, 44.8, 1.5, 1.0
ANT_BCD = 26.6                    # HC977 3x M2.5 insert bolt circle, per the Calian
                                  # mechanical drawing (3x M2.5 deep 6, 120 deg)

# ---------- 5/8"-11 pole nut boss ----------
NUT_AF, NUT_T = 23.83, 13.9
NUT_R = (NUT_AF + 0.4) / math.sqrt(3)   # hex pocket circumradius ≈ 13.99
BOSS_FLOOR = 3.0
BOSS_H = NUT_T + 0.3 + BOSS_FLOOR       # 17.2 below the base
BOSS_HX, BOSS_HZ = 16.5, 14.5           # pocket corners ±X, flats ±Z
POLE_HOLE_D = 17.0

# ---------- cover ----------
LIP_H, LIP_T = 3.0, 2.0
POST_D, POST_H = 9.0, 12.0
POST_X = INT_W / 2 - 3.25
POST_DY = INT_H / 2 - 3.25
INSERT_M3 = 4.0                   # ALL printed inserts are M3 (owner stock: 4/6/8 mm
                                  # lengths; 4 mm in Pynt/carrier bosses, 6-8 in posts)

def post_xy():
    return [(sx * POST_X, EXT_H / 2 + sy * POST_DY)
            for sx in (-1, 1) for sy in (-1, 1)]

# ============ Fusion plumbing ============
CM = 0.1  # mm -> cm

def P3(x, y, z):
    return adsk.core.Point3D.create(x * CM, y * CM, z * CM)

def VR(v_mm):
    return adsk.core.ValueInput.createByReal(v_mm * CM)

class Builder:
    """Wraps a component; all coordinates are WORLD mm — sketch-plane
    orientation quirks are neutralized via modelToSketchSpace and the
    sketch normal, so no feature depends on Fusion's plane conventions."""

    def __init__(self, comp):
        self.comp = comp
        self.body = None            # main body, set by the first new_body

    def plane_at_z(self, z_mm):
        planes = self.comp.constructionPlanes
        inp = planes.createInput()
        inp.setByOffset(self.comp.xYConstructionPlane, VR(z_mm))
        return planes.add(inp)

    def sketch(self, plane):
        return self.comp.sketches.add(plane)

    def sk_pt(self, sk, x, y, z):
        return sk.modelToSketchSpace(P3(x, y, z))

    def rect(self, sk, p1, p2):
        sk.sketchCurves.sketchLines.addTwoPointRectangle(
            self.sk_pt(sk, *p1), self.sk_pt(sk, *p2))

    def circle(self, sk, c, d_mm):
        sk.sketchCurves.sketchCircles.addByCenterRadius(
            self.sk_pt(sk, *c), d_mm / 2 * CM)

    def polygon(self, sk, pts):
        lines = sk.sketchCurves.sketchLines
        sps = [self.sk_pt(sk, *p) for p in pts]
        for i in range(len(sps)):
            lines.addByTwoPoints(sps[i], sps[(i + 1) % len(sps)])

    def _normal(self, sk):
        n = sk.xDirection.crossProduct(sk.yDirection)
        n.normalize()
        return n

    def _profiles(self, sk, pick=None):
        profs = [sk.profiles.item(i) for i in range(sk.profiles.count)]
        if pick:
            profs = [p for p in profs if pick(p)]
        coll = adsk.core.ObjectCollection.create()
        for p in profs:
            coll.add(p)
        return coll

    def extrude(self, sk, w_from, w_to, axis, op, pick=None, participants=None):
        """Extrude the sketch's profiles from world coordinate w_from to w_to
        (mm) measured along `axis` = 'y' or 'z'. Sign-safe by construction."""
        ax = {'y': adsk.core.Vector3D.create(0, 1, 0),
              'z': adsk.core.Vector3D.create(0, 0, 1)}[axis]
        n = self._normal(sk)
        s = n.dotProduct(ax)                 # +1 or -1
        o = sk.origin.asVector().dotProduct(ax) / CM   # plane position, mm
        exts = self.comp.features.extrudeFeatures
        inp = exts.createInput(self._profiles(sk, pick), op)
        inp.startExtent = adsk.fusion.OffsetStartDefinition.create(
            VR((w_from - o) * s))
        inp.setDistanceExtent(False, VR((w_to - w_from) * s))
        if participants:
            inp.participantBodies = participants
        feat = exts.add(inp)
        if self.body is None and feat.bodies.count > 0:
            self.body = feat.bodies.item(0)
        return feat

    def loft(self, sk1, sk2, op, participants=None):
        lofts = self.comp.features.loftFeatures
        inp = lofts.createInput(op)
        inp.loftSections.add(sk1.profiles.item(0))
        inp.loftSections.add(sk2.profiles.item(0))
        if participants:
            try:
                inp.participantBodies = participants
            except Exception:
                pass                          # older API: default participants
        return lofts.add(inp)


def teardrop_xy(b, sk, cx, cy, d, z=0.0):
    """Teardrop hole profile in an xy sketch: circle plus a 45° roof, apex
    toward +y — 'up' in the REFERENCE print orientation (rev A8: BOTH
    enclosures print nut-side down). z-axis holes print horizontally, so
    their tops compress unless teardropped (rev A7 fit finding)."""
    r = d / 2.0
    b.circle(sk, (cx, cy, z), d)
    k = r / math.sqrt(2.0)
    ln = sk.sketchCurves.sketchLines
    apex = b.sk_pt(sk, cx, cy + r * math.sqrt(2.0), z)
    ln.addByTwoPoints(b.sk_pt(sk, cx - k, cy + k, z), apex)
    ln.addByTwoPoints(b.sk_pt(sk, cx + k, cy + k, z), apex)


def area_between(lo_mm2, hi_mm2):
    def pick(prof):
        a = prof.areaProperties(
            adsk.fusion.CalculationAccuracy.MediumCalculationAccuracy).area
        return lo_mm2 * CM * CM <= a <= hi_mm2 * CM * CM
    return pick


def build_shell(b):
    NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
    JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation
    xy0 = b.comp.xYConstructionPlane          # z = 0, the exterior front face
    xz0 = b.comp.xZConstructionPlane          # y = 0, the exterior bottom face
    z_front = b.plane_at_z(FRONT)
    z_back = b.plane_at_z(SHELL_D)

    # body
    sk = b.sketch(xy0)
    b.rect(sk, (-EXT_W / 2, 0, 0), (EXT_W / 2, EXT_H, 0))
    b.extrude(sk, 0, SHELL_D, 'z', NEW).name = 'shell body'
    shell = [b.body]

    # interior cavity
    sk = b.sketch(xy0)
    b.rect(sk, (-INT_W / 2, FLOOR_T, 0), (INT_W / 2, WALL + INT_H, 0))
    b.extrude(sk, FRONT, SHELL_D + 5, 'z', CUT, participants=shell).name = 'cavity'

    # screen window: loft cut flaring 45° to the front face
    wx = sorted([bx(WIN_YB[0]), bx(WIN_YB[1])])
    wy = (by(WIN_XB[0]), by(WIN_XB[1]))
    skA = b.sketch(xy0)                       # big outline at the front face
    b.rect(skA, (wx[0] - WIN_MARGIN - FRONT, wy[0] - WIN_MARGIN - FRONT, 0),
                (wx[1] + WIN_MARGIN + FRONT, wy[1] + WIN_MARGIN + FRONT, 0))
    skB = b.sketch(z_front)                   # true window at the glass
    b.rect(skB, (wx[0] - WIN_MARGIN, wy[0] - WIN_MARGIN, FRONT),
                (wx[1] + WIN_MARGIN, wy[1] + WIN_MARGIN, FRONT))
    b.loft(skA, skB, CUT, participants=shell).name = 'window'

    # rain eyebrow: its 45° underside is COLINEAR with the window's top
    # chamfer — one continuous plane from the window edge to the eyebrow
    # tip (rev A5: the old offset shape left a step / double edge)
    eb0 = wy[1] + WIN_MARGIN + FRONT      # the flare's outer top edge at z=0
    skA = b.sketch(xy0)
    b.rect(skA, (-(EXT_W / 2 - 4), eb0, 0), (EXT_W / 2 - 4, eb0 + 5.0, 0))
    z_eb = b.plane_at_z(-4.0)
    skB = b.sketch(z_eb)
    b.rect(skB, (-(EXT_W / 2 - 4), eb0 + 4.0, -4), (EXT_W / 2 - 4, eb0 + 5.0, -4))
    b.loft(skA, skB, JOIN).name = 'eyebrow'

    # light-sensor pinhole
    sk = b.sketch(xy0)                        # z-axis hole: teardrop (rev A8)
    teardrop_xy(b, sk, bx(LIGHT_YB), by(LIGHT_XB), 2.5)
    b.extrude(sk, -1, FRONT + 1, 'z', CUT, participants=shell).name = 'light sensor'

    # Pynt bosses + heat-set pilots (M2.5)
    # Pynt TOP-ear bosses (the only bosses, rev A4): rectangular, trimmed to
    # 2.2 mm inboard of the hole center — the glass side edge is ~2.5 mm from
    # the hole center (owner-measured; a round Ø8 boss hits the screen) —
    # merged into the side wall outboard, 45° taper on the bottom face
    # (support-free in any orientation), direct-thread M3 pilots (Ø2.5, no
    # inserts: no room for one beside the glass).
    z_pcb_pl = b.plane_at_z(Z_PCB_FRONT)
    cy = by(PYNT_HOLES_XB[1])
    for yb_ in PYNT_HOLES_YB:
        cx = bx(yb_)
        sgn = 1.0 if cx > 0 else -1.0
        x_in, x_out = cx - sgn * 2.2, sgn * (INT_W / 2 + 0.1)
        skA = b.sketch(z_front)
        b.rect(skA, (x_in, cy - 3.5 - (Z_PCB_FRONT - FRONT), FRONT),
                    (x_out, cy + 3.5, FRONT))
        skB = b.sketch(z_pcb_pl)
        b.rect(skB, (x_in, cy - 3.5, Z_PCB_FRONT), (x_out, cy + 3.5, Z_PCB_FRONT))
        b.loft(skA, skB, JOIN).name = 'pynt boss'
    sk = b.sketch(z_front)                    # z-axis pilots: teardrop (rev A8)
    for yb_ in PYNT_HOLES_YB:
        teardrop_xy(b, sk, bx(yb_), cy, 2.5, FRONT)
    b.extrude(sk, Z_PCB_FRONT - 5.0, Z_PCB_FRONT + 6, 'z', CUT,
              participants=shell).name = 'pynt pilots (direct thread)'

    # bottom-EAR capture slots (rev A6): the board slides straight DOWN, its
    # bare ear tabs (PCB only) entering 2.0 mm slots — 0.2 clearance each
    # side of the 1.6 PCB. Front block trimmed 2.0 mm inboard of the hole
    # center (clears the glass edge, same rule as the top bosses); the rear
    # wedge's 45° face is both the slide-in lead and the print-safe
    # underside. Top screws then clamp the board; PCB plane unchanged.
    for yb_ in PYNT_HOLES_YB:
        cx = bx(yb_)
        sgn = 1.0 if cx > 0 else -1.0
        x_in, x_out = cx - sgn * 2.0, sgn * (INT_W / 2 + 0.1)
        # rear pieces (z >= 7.2) must clear the USB plug path on that side
        # (USB slot spans z 8-13 — the front block at z <= 7.2 is fine)
        x_in_rear = x_in
        if sgn * bx(USB_YB_C) > 0:
            x_in_rear = sgn * (abs(bx(USB_YB_C)) + USB_SLOT_W / 2 + 0.25)
        sk = b.sketch(xy0)
        b.rect(sk, (x_in, FLOOR_T, 0), (x_out, PYNT_Y0 + 4.7, 0))
        b.extrude(sk, FRONT, Z_PCB_FRONT - 0.2, 'z', JOIN).name = 'ear slot front'
        # (rev A7: no base fill — the board edge now sits ON the floor)
        # outboard end fill: the ear stops ~0.9 short of the side wall, so
        # the gap is closed there — anchors the rear wall's printed underside
        sk = b.sketch(xy0)
        b.rect(sk, (sgn * 26.8, FLOOR_T, 0), (x_out, PYNT_Y0 + 4.7, 0))
        b.extrude(sk, Z_PCB_FRONT - 0.2, Z_PCB_FRONT + PCB_T + 0.2, 'z',
                  JOIN).name = 'ear slot end fill'
        # rear wall: VERTICAL capture face at z 9.2 backing the PCB's lower
        # 1.4 mm, then a halved 45° lead-in
        skA = b.sketch(b.plane_at_z(Z_PCB_FRONT + PCB_T + 0.2))
        b.rect(skA, (x_in_rear, FLOOR_T, Z_PCB_FRONT + PCB_T + 0.2),
                    (x_out, PYNT_Y0 + 1.4, Z_PCB_FRONT + PCB_T + 0.2))
        skB = b.sketch(b.plane_at_z(Z_PCB_FRONT + PCB_T + 1.8))
        b.rect(skB, (x_in_rear, FLOOR_T, Z_PCB_FRONT + PCB_T + 1.8),
                    (x_out, PYNT_Y0 + 3.0, Z_PCB_FRONT + PCB_T + 1.8))
        b.loft(skA, skB, JOIN).name = 'ear slot wedge'

    # cover corner posts + heat-set pilots (M3)
    sk = b.sketch(z_back)
    for (px, py) in post_xy():
        b.circle(sk, (px, py, SHELL_D), POST_D)
    b.extrude(sk, SHELL_D, SHELL_D - POST_H, 'z', JOIN).name = 'cover posts'
    sk = b.sketch(z_back)                     # z-axis pilots: teardrop (rev A8)
    for (px, py) in post_xy():
        teardrop_xy(b, sk, px, py, INSERT_M3, SHELL_D)
    b.extrude(sk, SHELL_D + 0.1, SHELL_D - 9, 'z', CUT,
              participants=shell).name = 'post pilots'

    # bottom-wall penetrations: USB, SD, vents (all cut y -1 .. WALL+1)
    sk = b.sketch(xz0)
    b.rect(sk, (bx(USB_YB_C) - USB_SLOT_W / 2, 0, USB_SLOT_Z[0]),
               (bx(USB_YB_C) + USB_SLOT_W / 2, 0, USB_SLOT_Z[1]))
    # SD slot: rev A7 fit check read ~2 mm off — widened 2 mm toward the
    # enclosure center only (outboard edge stays put)
    sd_lo = bx(SD_YB_C) - SD_SLOT_W / 2 - (2.0 if bx(SD_YB_C) > 0 else 0.0)
    sd_hi = bx(SD_YB_C) + SD_SLOT_W / 2 + (2.0 if bx(SD_YB_C) < 0 else 0.0)
    b.rect(sk, (sd_lo, 0, SD_SLOT_Z[0]), (sd_hi, 0, SD_SLOT_Z[1]))
    for sx in (-1, 1):                # 3-finger vent grilles (read as vents, not
        for k in (-1, 0, 1):          # ports); centers +-14.5 keeps the outer
            b.rect(sk, (sx * 14.5 + k * 4.8 - 1.4, 0, 4.5),   # finger 1+ mm
                       (sx * 14.5 + k * 4.8 + 1.4, 0, 7.0))   # clear of the
                                                              # ear-slot blocks
    b.extrude(sk, -1, FLOOR_T + 1, 'y', CUT, participants=shell).name = 'USB/SD/vents'

    # 5/8-11 nut boss: plain prism below the base (front face vertical —
    # any forward blend would block the USB plug / SD card)
    sk = b.sketch(xz0)
    b.rect(sk, (-BOSS_HX, 0, AXIS_Z - BOSS_HZ), (BOSS_HX, 0, AXIS_Z + BOSS_HZ))
    b.extrude(sk, 0, -BOSS_H, 'y', JOIN).name = 'nut boss'
    sk = b.sketch(xz0)                        # hex pocket, corners toward ±X
    b.polygon(sk, [(NUT_R * math.cos(math.radians(60 * k)),
                    0,
                    AXIS_Z + NUT_R * math.sin(math.radians(60 * k)))
                   for k in range(6)])
    b.extrude(sk, -BOSS_H + BOSS_FLOOR, FLOOR_T + 0.1, 'y', CUT,
              participants=shell).name = 'nut pocket'
    sk = b.sketch(xz0)                        # vertical bottom-up: plain round
    b.circle(sk, (0, 0, AXIS_Z), POLE_HOLE_D)
    b.extrude(sk, -BOSS_H - 1, -BOSS_H + BOSS_FLOOR + 1, 'y', CUT,
              participants=shell).name = 'pole hole'

    # antenna pad on the lid (full circle — the shell is deep enough that
    # the whole HC977 base lands on it), locating recess, SMA bulkhead hole
    sk = b.sketch(xz0)
    b.circle(sk, (0, 0, AXIS_Z), ANT_PAD_D)
    b.extrude(sk, EXT_H, EXT_H + ANT_PAD_H, 'y', JOIN).name = 'antenna pad'
    sk = b.sketch(xz0)
    b.circle(sk, (0, 0, AXIS_Z), ANT_RECESS_D)
    b.extrude(sk, EXT_H + ANT_PAD_H + 0.1, EXT_H + ANT_PAD_H - ANT_RECESS_H,
              'y', CUT, participants=shell).name = 'antenna recess'
    sk = b.sketch(xz0)                        # vertical bottom-up: plain round
    b.circle(sk, (0, 0, AXIS_Z), SMA_HOLE_D + 2 * FUDGE)
    b.extrude(sk, EXT_H - WALL - 1, EXT_H + ANT_PAD_H + 1, 'y', CUT,
              participants=shell).name = 'SMA bulkhead hole'
    # 3x M2.5 clearance holes on the HC977's insert bolt circle; screws come
    # from inside the lid into the antenna's own inserts. The a=0 hole aims
    # dead REARWARD (+z): antenna-north reference faces away from the screen.
    sk = b.sketch(xz0)                        # vertical bottom-up: plain round
    for a in (0.0, 120.0, 240.0):
        b.circle(sk, (ANT_BCD / 2 * math.sin(math.radians(a)), 0,
                      AXIS_Z + ANT_BCD / 2 * math.cos(math.radians(a))),
                 2.8 + 2 * FUDGE)
    b.extrude(sk, EXT_H - WALL - 1, EXT_H + ANT_PAD_H + 1, 'y', CUT,
              participants=shell).name = 'antenna mount holes'


def build_cover(b):
    NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
    JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation
    z_back = b.plane_at_z(SHELL_D)

    # plate
    sk = b.sketch(z_back)
    b.rect(sk, (-EXT_W / 2, 0, SHELL_D), (EXT_W / 2, EXT_H, SHELL_D))
    b.extrude(sk, SHELL_D, TOTAL_D, 'z', NEW).name = 'cover plate'
    cov = [b.body]

    # labyrinth lip (annular ring between two rectangles, picked by area)
    cy = WALL + INT_H / 2
    ow, oh = INT_W - 2 * FUDGE, INT_H - 2 * FUDGE
    iw, ih = INT_W - 2 * LIP_T, INT_H - 2 * LIP_T
    sk = b.sketch(z_back)
    b.rect(sk, (-ow / 2, cy - oh / 2, SHELL_D), (ow / 2, cy + oh / 2, SHELL_D))
    b.rect(sk, (-iw / 2, cy - ih / 2, SHELL_D), (iw / 2, cy + ih / 2, SHELL_D))
    ring = ow * oh - iw * ih
    b.extrude(sk, SHELL_D, SHELL_D - LIP_H, 'z', JOIN,
              pick=area_between(ring * 0.9, ring * 1.1)).name = 'lip'

    # lip clearance around the shell's corner posts
    sk = b.sketch(z_back)
    for (px, py) in post_xy():
        b.circle(sk, (px, py, SHELL_D), POST_D + 0.6)
    b.extrude(sk, SHELL_D - LIP_H - 1, SHELL_D, 'z', CUT,
              participants=cov).name = 'lip post clearance'

    # carrier bosses + blind M3x4 heat-set pockets (boss + 1.0 into the
    # plate, 1.8 mm back skin)
    sk = b.sketch(z_back)
    for sx in (-1, 1):
        for iy in (0, 1):
            b.circle(sk, (sx * CARRIER_HOLE_DX / 2,
                          WALL + CARRIER_Y0 + 3.0 + iy * CARRIER_HOLE_DY,
                          SHELL_D), 8.0)
    b.extrude(sk, SHELL_D, SHELL_D - CARRIER_BOSS_H, 'z', JOIN).name = 'carrier bosses'
    sk = b.sketch(z_back)
    for sx in (-1, 1):
        for iy in (0, 1):
            b.circle(sk, (sx * CARRIER_HOLE_DX / 2,
                          WALL + CARRIER_Y0 + 3.0 + iy * CARRIER_HOLE_DY,
                          SHELL_D), INSERT_M3)
    b.extrude(sk, SHELL_D - CARRIER_BOSS_H - 0.1, SHELL_D - CARRIER_BOSS_H + 4.5,
              'z', CUT, participants=cov).name = 'carrier pilots'

    # cover screw through-holes (M3 pan head into the shell posts)
    sk = b.sketch(z_back)
    for (px, py) in post_xy():
        b.circle(sk, (px, py, SHELL_D), 3.4)
    b.extrude(sk, SHELL_D - 0.1, TOTAL_D + 0.1, 'z', CUT,
              participants=cov).name = 'cover screw holes'


def trim_to_base_test(b, body):
    """Bottom 25 mm of the shell (matches the .scad base_test region):
    nut pocket fit, USB plug vs boss clearance, SD reach, grilles."""
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation
    sk = b.sketch(b.comp.xZConstructionPlane)
    b.rect(sk, (-EXT_W / 2 - 2, 0, -8), (EXT_W / 2 + 2, 0, SHELL_D + 2))
    b.extrude(sk, 25.0, EXT_H + ANT_PAD_H + 2, 'y', CUT,
              participants=[body]).name = 'base_test trim'


def trim_to_bezel_test(b, body):
    """One top bezel corner (matches the .scad bezel_test region): window
    chamfer vs touch reach, eyebrow overhang, one top-ear insert pocket."""
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation
    xz0 = b.comp.xZConstructionPlane
    xy0 = b.comp.xYConstructionPlane
    y0 = by(WIN_XB[1]) - 12.0
    y1 = y0 + 30.0
    z_hi = FRONT + 8.0
    sk = b.sketch(xz0)
    b.rect(sk, (-EXT_W / 2 - 2, 0, -8), (EXT_W / 2 + 2, 0, SHELL_D + 2))
    b.extrude(sk, -BOSS_H - 2, y0, 'y', CUT, participants=[body]).name = 'trim below'
    sk = b.sketch(xz0)
    b.rect(sk, (-EXT_W / 2 - 2, 0, -8), (EXT_W / 2 + 2, 0, SHELL_D + 2))
    b.extrude(sk, y1, EXT_H + ANT_PAD_H + 2, 'y', CUT,
              participants=[body]).name = 'trim above'
    sk = b.sketch(xy0)
    b.rect(sk, (0, y0 - 2, 0), (EXT_W / 2 + 2, y1 + 2, 0))
    b.extrude(sk, -8, SHELL_D + 2, 'z', CUT, participants=[body]).name = 'trim +x half'
    sk = b.sketch(xy0)
    b.rect(sk, (-EXT_W / 2 - 2, y0 - 2, 0), (2, y1 + 2, 0))
    b.extrude(sk, z_hi, SHELL_D + 2, 'z', CUT,
              participants=[body]).name = 'trim depth'


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        design = adsk.fusion.Design.cast(app.activeProduct)
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        root = design.rootComponent
        # Fusion forbids renaming the root component of an unsaved document
        # (its name tracks the document name), so name it on a best-effort
        # basis — the components below always get their own names.
        try:
            root.name = 'Pynt RTK enclosure rev A'
        except RuntimeError:
            pass

        occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        shell_comp = adsk.fusion.Component.cast(occ.component)
        shell_comp.name = 'Shell' if PART == 'assembly' else PART
        build_shell(Builder(shell_comp))

        if PART != 'assembly':
            trim = {'base_test': trim_to_base_test,
                    'bezel_test': trim_to_bezel_test}[PART]
            trim(Builder(shell_comp), shell_comp.bRepBodies.item(0))
            out = os.path.normpath(os.path.join(
                os.path.dirname(os.path.abspath(__file__)),
                '..', 'prints', PART + '.stl'))
            os.makedirs(os.path.dirname(out), exist_ok=True)
            em = design.exportManager
            opts = em.createSTLExportOptions(shell_comp.bRepBodies.item(0), out)
            opts.meshRefinement = adsk.fusion.MeshRefinementSettings.MeshRefinementHigh
            em.execute(opts)
            app.activeViewport.fit()
            ui.messageBox(PART + ' built and exported to:\n' + out)
            return

        occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        cover_comp = adsk.fusion.Component.cast(occ.component)
        cover_comp.name = 'Cover'
        build_cover(Builder(cover_comp))

        app.activeViewport.fit()
        z_lite_top = SHELL_D - CARRIER_BOSS_H - LITE_TOP_OFF
        ui.messageBox(
            'Enclosure rev A built.\n\n'
            f'Exterior: {EXT_W:.1f} x {EXT_H:.1f} x {TOTAL_D:.1f} mm '
            f'(+{BOSS_H:.1f} nut boss, +{ANT_PAD_H:.1f} antenna pad)\n'
            f'Pole shoulder -> antenna pad top: {BOSS_H + EXT_H + ANT_PAD_H:.1f} mm\n'
            f'Lite top face at z={z_lite_top:.1f}; radio void to z={z_lite_top - RADIO_VOID:.1f}\n\n'
            'Fit-check refs: Insert > Insert CAD from the STEP files in '
            'enclosure/vendor (Pynt, Lite, USB-C carrier).\n'
            'Reminders: pan-head M3s on the cover; measure the HC977 '
            'insert bolt circle -> ANT_BCD before printing the lid. '
            'Carrier dims confirmed (mini-USB = USB-C board minus '
            'connector); PYNT_MIRROR = -1 confirmed.')
    except Exception:
        if ui:
            ui.messageBox('Failed:\n{}'.format(traceback.format_exc()))
