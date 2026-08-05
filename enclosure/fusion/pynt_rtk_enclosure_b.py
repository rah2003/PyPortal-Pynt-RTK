# PyPortal Pynt RTK — enclosure REV B: the "D" shell (owner-specified,
# 2026-07-20 markup). Rev A (pynt_rtk_enclosure.py) is untouched.
#
# Plan view = a "D": the FRONT (Pynt) half is identical to rev A — flat
# front, full-width straight sides, all vertical walls stay vertical. At the
# ANTENNA CENTERLINE (z 31) the sides transition via straight TANGENT lines
# to a rear ARC about the pole axis:
#   * outer arc R26.8 = the current antenna->body rear standoff (57.8 - 31)
#   * the CASE BACK is a curved band, same 2.8 thickness as today, spanning
#     tangent point to tangent point, constant profile down the full height
#     ("mirrored profile down the entire length ... contoured to match")
#   * the old square rear corners are gone ("remove corner of housing")
#
# Case back mounting: the band stands on the shell floor, its top edge rides
# in a groove under the integral lid, its vertical end edges carry
# half-thickness TONGUES that slide into grooves rebated into the tangent
# walls (full-height lateral support), and 2x M3 come up from BELOW through
# the floor into bosses welded to the band (screw heads face down = rain-ok).
# The carrier moves 1.0 forward (mount plane z 50.5, was 51.5) so its corners
# clear the curved back; its bosses grow from the curved band to a flat
# plane ("lite further inside").
#
# Print (reference orientation): both parts NUT-SIDE DOWN. The case back is
# a constant-profile band -> prints standing with zero support. Shell keeps
# rev A's caveats (interior lid ceiling + window top edge need support).

import adsk.core, adsk.fusion, traceback, math

HEADLESS = False                  # True when driven via the Fusion MCP

# ---------- rev A8 shared numbers (fit-check validated) ----------
WALL, FRONT = 2.8, 3.0
FLOOR_T = 1.4
AXIS_Z = 31.0
INT_W = 55.0
XF_EXT = INT_W / 2 + WALL         # 30.3
XF_INT = INT_W / 2                # 27.5
EXT_H = 88.6
FUDGE = 0.15

# ---------- D-plan ----------
R_OUT = 26.8                      # axis -> case-back outer face (= rev A 57.8)
R_IN = R_OUT - WALL               # 24.0 interior at the rear arc
COVER_ANG = None                  # derived: tangent angle (from the rear axis)

# ---------- Pynt / front half (rev A8) ----------
PYNT_MIRROR = -1
PYNT_YC = 43.180 / 2
PYNT_Y0 = FLOOR_T
PYNT_HOLES_XB = [2.540, 64.262]
PYNT_HOLES_YB = [-2.540, 45.339]
FOAM_GAP, GLASS_H, PCB_T = 0.5, 3.9, 1.6
Z_PCB_FRONT = FRONT + FOAM_GAP + GLASS_H
WIN_XB = [8.49, 58.74]
WIN_YB = [2.19, 40.79]
WIN_MARGIN = 0.6
USB_YB_C, USB_SLOT_W, USB_SLOT_Z = 5.08, 13.0, (8.0, 13.0)
SD_YB_C, SD_SLOT_W, SD_SLOT_Z = 19.8, 12.0, (8.2, 11.5)
LIGHT_XB, LIGHT_YB = 3.94, 11.43

def bx(yb): return PYNT_MIRROR * (yb - PYNT_YC)
def by(xb): return PYNT_Y0 + xb

# ---------- antenna / SMA (rev A8) ----------
SMA_HOLE_D = 6.5
ANT_PAD_D, ANT_RECESS_D, ANT_PAD_H, ANT_RECESS_H = 46.0, 44.8, 1.5, 1.0
ANT_BCD = 26.6                    # per Calian HC977 mechanical drawing
                                  # (3x M2.5 deep 6, 120 deg, O-ring base)

# ---------- nut boss (rev A prism, unchanged) ----------
NUT_AF, NUT_T = 23.83, 13.9
NUT_R = (NUT_AF + 0.4) / math.sqrt(3)
BOSS_FLOOR = 3.0
BOSS_H = NUT_T + 0.3 + BOSS_FLOOR
BOSS_HX, BOSS_HZ = 16.5, 14.5
POLE_HOLE_D = 17.0

# ---------- carrier (adapter board) ----------
CARRIER_HOLE_DX, CARRIER_HOLE_DY = 20.0, 35.0
CARRIER_Y0 = 8.5                  # raised 2.5 (owner fit finding: the lower
                                  # carrier bosses hit the case-back insert
                                  # bosses at the old height)
CARRIER_FACE_Z = 48.5             # +1.0 standoff (owner assembly finding,
                                  # 2026-07-26); bosses extended inward so the
                                  # M3x4 pockets keep >=1.0 outer skin (the
                                  # cuts were showing through the back)
INSERT_M3 = 4.0

# ---------- case back fastening ----------
CB_SCREW_X, CB_SCREW_Z = 12.0, 48.8   # 2x M3 up through the floor (48.8 keeps
                                      # the counterbore inside the funnel wall)
CB_TOP_GROOVE = 1.0                   # band top edge rides in a lid groove
CB_TONGUE_R0, CB_TONGUE_R1 = 24.0, 25.4   # half-thickness side tongue radii
CB_TONGUE_A0, CB_TONGUE_A1 = 61.9, 65.5   # tongue angular span (deg from rear)

# ============ Fusion plumbing ============
CM = 0.1

def P3(x, y, z):
    return adsk.core.Point3D.create(x * CM, y * CM, z * CM)

def VR(v_mm):
    return adsk.core.ValueInput.createByReal(v_mm * CM)

class Builder:
    def __init__(self, comp):
        self.comp = comp
        self.body = None

    def plane_at_z(self, z_mm):
        planes = self.comp.constructionPlanes
        inp = planes.createInput()
        inp.setByOffset(self.comp.xYConstructionPlane, VR(z_mm))
        return planes.add(inp)

    def sketch_at_y(self, y_mm):
        """Sketch on a plane parallel to xz at world y (offset sign probed)."""
        for trial in (y_mm, -y_mm):
            planes = self.comp.constructionPlanes
            inp = planes.createInput()
            inp.setByOffset(self.comp.xZConstructionPlane, VR(trial))
            pl = planes.add(inp)
            sk = self.comp.sketches.add(pl)
            oy = sk.origin.asVector().dotProduct(
                adsk.core.Vector3D.create(0, 1, 0)) / CM
            if abs(oy - y_mm) < 0.01:
                return sk
            sk.deleteMe()
            pl.deleteMe()
        raise RuntimeError('plane offset sign probe failed for y=%s' % y_mm)

    def sketch(self, plane):
        return self.comp.sketches.add(plane)

    def sk_pt(self, sk, x, y, z):
        return sk.modelToSketchSpace(P3(x, y, z))

    def rect(self, sk, p1, p2):
        sk.sketchCurves.sketchLines.addTwoPointRectangle(
            self.sk_pt(sk, *p1), self.sk_pt(sk, *p2))

    def line(self, sk, p1, p2):
        return sk.sketchCurves.sketchLines.addByTwoPoints(
            self.sk_pt(sk, *p1), self.sk_pt(sk, *p2))

    def arc3(self, sk, p1, pm, p2):
        return sk.sketchCurves.sketchArcs.addByThreePoints(
            self.sk_pt(sk, *p1), self.sk_pt(sk, *pm), self.sk_pt(sk, *p2))

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
        ax = {'y': adsk.core.Vector3D.create(0, 1, 0),
              'z': adsk.core.Vector3D.create(0, 0, 1)}[axis]
        n = self._normal(sk)
        s = n.dotProduct(ax)
        o = sk.origin.asVector().dotProduct(ax) / CM
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
                pass
        return lofts.add(inp)


def teardrop_xy(b, sk, cx, cy, d, z=0.0):
    """z-axis holes print horizontally nut-side down: 45° roof, apex +y."""
    r = d / 2.0
    b.circle(sk, (cx, cy, z), d)
    k = r / math.sqrt(2.0)
    ln = sk.sketchCurves.sketchLines
    apex = b.sk_pt(sk, cx, cy + r * math.sqrt(2.0), z)
    ln.addByTwoPoints(b.sk_pt(sk, cx - k, cy + k, z), apex)
    ln.addByTwoPoints(b.sk_pt(sk, cx + k, cy + k, z), apex)


def tangent_angle(r, xf):
    """Angle (from +x, at the antenna centerline) of the tangent point for a
    tangent line from (xf, AXIS_Z) to the circle R=r about the axis."""
    return math.acos(r / xf)


def d_profile(b, sk, r, xf, zf):
    """The owner's D: front line at z=zf (half-width xf), straight sides to
    the antenna centerline, tangent lines to the rear arc R=r on the axis."""
    a = tangent_angle(r, xf)
    tx, tz = r * math.cos(a), AXIS_Z + r * math.sin(a)
    b.line(sk, (-xf, 0, zf), (xf, 0, zf))
    b.line(sk, (xf, 0, zf), (xf, 0, AXIS_Z))
    b.line(sk, (xf, 0, AXIS_Z), (tx, 0, tz))
    b.arc3(sk, (tx, 0, tz), (0, 0, AXIS_Z + r), (-tx, 0, tz))
    b.line(sk, (-tx, 0, tz), (-xf, 0, AXIS_Z))
    b.line(sk, (-xf, 0, AXIS_Z), (-xf, 0, zf))


def band_pts(r, ang_deg):
    a = math.radians(ang_deg)
    return (r * math.sin(a), AXIS_Z + r * math.cos(a))


def build_shell(b):
    NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
    JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation
    xy0 = b.comp.xYConstructionPlane
    xz0 = b.comp.xZConstructionPlane
    z_front = b.plane_at_z(FRONT)

    # D-plan body, integral floor and lid
    sk = b.sketch(xz0)
    d_profile(b, sk, R_OUT, XF_EXT, 0.0)
    b.extrude(sk, 0, EXT_H, 'y', NEW).name = 'shell body'
    shell = [b.body]
    sk = b.sketch(xz0)
    d_profile(b, sk, R_IN, XF_INT, FRONT)
    b.extrude(sk, FLOOR_T, EXT_H - WALL, 'y', CUT,
              participants=shell).name = 'cavity'

    # rear aperture: the arc sector between the tangent radials is open —
    # the curved case back lands here (flush with the outer surface)
    ang = math.degrees(math.pi / 2 - tangent_angle(R_OUT, XF_EXT))  # 62.2
    sk = b.sketch(xz0)
    o1 = band_pts(R_OUT + 2, ang)
    i1 = band_pts(R_IN - FUDGE, ang)
    b.arc3(sk, (o1[0], 0, o1[1]), (0, 0, AXIS_Z + R_OUT + 2),
           (-o1[0], 0, o1[1]))
    b.arc3(sk, (i1[0], 0, i1[1]), (0, 0, AXIS_Z + R_IN - FUDGE),
           (-i1[0], 0, i1[1]))
    b.line(sk, (o1[0], 0, o1[1]), (i1[0], 0, i1[1]))
    b.line(sk, (-o1[0], 0, o1[1]), (-i1[0], 0, i1[1]))
    b.extrude(sk, FLOOR_T, EXT_H - WALL, 'y', CUT,
              participants=shell).name = 'case-back aperture'
    # groove under the lid so the band's top edge is captured
    sk = b.sketch(xz0)
    o2 = band_pts(R_OUT + 2, ang)
    i2 = band_pts(R_IN - FUDGE, ang)
    b.arc3(sk, (o2[0], 0, o2[1]), (0, 0, AXIS_Z + R_OUT + 2),
           (-o2[0], 0, o2[1]))
    b.arc3(sk, (i2[0], 0, i2[1]), (0, 0, AXIS_Z + R_IN - FUDGE),
           (-i2[0], 0, i2[1]))
    b.line(sk, (o2[0], 0, o2[1]), (i2[0], 0, i2[1]))
    b.line(sk, (-o2[0], 0, o2[1]), (-i2[0], 0, i2[1]))
    b.extrude(sk, EXT_H - WALL, EXT_H - WALL + CB_TOP_GROOVE, 'y', CUT,
              participants=shell).name = 'case-back top groove'
    # (case-back screw holes + counterbores are cut AFTER the funnel exists —
    # see the end of the nut-boss section)
    # vertical side grooves in the tangent walls: the band's half-thickness
    # tongues slide in for full-height lateral support (both edges)
    for side in (-1, 1):
        sk = b.sketch(xz0)
        # clearances HALVED after the first band test read loose (owner):
        # radial 0.075 per side, angular padding 0.2/0.35 deg
        g0, g1 = CB_TONGUE_A0 - 0.2, CB_TONGUE_A1 + 0.35
        ro, ri = CB_TONGUE_R1 + 0.075, CB_TONGUE_R0 - 0.075
        p_o0 = (side * ro * math.sin(math.radians(g0)), 0,
                AXIS_Z + ro * math.cos(math.radians(g0)))
        p_om = (side * ro * math.sin(math.radians((g0 + g1) / 2)), 0,
                AXIS_Z + ro * math.cos(math.radians((g0 + g1) / 2)))
        p_o1 = (side * ro * math.sin(math.radians(g1)), 0,
                AXIS_Z + ro * math.cos(math.radians(g1)))
        p_i0 = (side * ri * math.sin(math.radians(g0)), 0,
                AXIS_Z + ri * math.cos(math.radians(g0)))
        p_im = (side * ri * math.sin(math.radians((g0 + g1) / 2)), 0,
                AXIS_Z + ri * math.cos(math.radians((g0 + g1) / 2)))
        p_i1 = (side * ri * math.sin(math.radians(g1)), 0,
                AXIS_Z + ri * math.cos(math.radians(g1)))
        b.arc3(sk, p_o0, p_om, p_o1)
        b.arc3(sk, p_i0, p_im, p_i1)
        b.line(sk, p_o0, p_i0)
        b.line(sk, p_o1, p_i1)
        b.extrude(sk, FLOOR_T, EXT_H - WALL + CB_TOP_GROOVE, 'y', CUT,
                  participants=shell).name = 'case-back side groove'

    # anti-flex ledges (owner request): a 1.0 rib proud of the interior face
    # behind each band end, so the band bears on it instead of deflecting
    # inward; the rib hooks into the tangent wall past the groove end so it
    # is braced along its full height (a free-standing blade would flex too)
    for side in (-1, 1):
        def lp(r, a):
            return (side * r * math.sin(math.radians(a)), 0,
                    AXIS_Z + r * math.cos(math.radians(a)))
        ro_l = CB_TONGUE_R0 - 0.075          # stop face: same 0.075 gap as the
        ri_l = ro_l - 1.0                    # confirmed band clearances
        a0, a1, a2 = 57.0, 66.3, 69.5        # aperture edge 62.2; groove ends
        ra = 24.8                            # 65.85; anchor bites ~0.6 of wall
        sk = b.sketch(xz0)
        b.arc3(sk, lp(ro_l, a0), lp(ro_l, (a0 + a1) / 2), lp(ro_l, a1))
        b.line(sk, lp(ro_l, a1), lp(ra, a1))
        b.arc3(sk, lp(ra, a1), lp(ra, (a1 + a2) / 2), lp(ra, a2))
        b.line(sk, lp(ra, a2), lp(ri_l, a2))
        b.arc3(sk, lp(ri_l, a2), lp(ri_l, (a0 + a2) / 2), lp(ri_l, a0))
        b.line(sk, lp(ri_l, a0), lp(ro_l, a0))
        b.extrude(sk, FLOOR_T - 0.3, EXT_H - WALL, 'y', JOIN,
                  participants=shell).name = 'band anti-flex ledge'

    # ---- front half: identical to rev A8 ----
    wx = sorted([bx(WIN_YB[0]), bx(WIN_YB[1])])
    wy = (by(WIN_XB[0]), by(WIN_XB[1]))
    skA = b.sketch(xy0)
    b.rect(skA, (wx[0] - WIN_MARGIN - FRONT, wy[0] - WIN_MARGIN - FRONT, 0),
                (wx[1] + WIN_MARGIN + FRONT, wy[1] + WIN_MARGIN + FRONT, 0))
    skB = b.sketch(z_front)
    b.rect(skB, (wx[0] - WIN_MARGIN, wy[0] - WIN_MARGIN, FRONT),
                (wx[1] + WIN_MARGIN, wy[1] + WIN_MARGIN, FRONT))
    b.loft(skA, skB, CUT, participants=shell).name = 'window'

    eb0 = wy[1] + WIN_MARGIN + FRONT
    skA = b.sketch(xy0)
    b.rect(skA, (-(XF_EXT - 4), eb0, 0), (XF_EXT - 4, eb0 + 5.0, 0))
    z_eb = b.plane_at_z(-4.0)
    skB = b.sketch(z_eb)
    b.rect(skB, (-(XF_EXT - 4), eb0 + 4.0, -4), (XF_EXT - 4, eb0 + 5.0, -4))
    b.loft(skA, skB, JOIN).name = 'eyebrow'

    sk = b.sketch(xy0)
    teardrop_xy(b, sk, bx(LIGHT_YB), by(LIGHT_XB), 2.5)
    b.extrude(sk, -1, FRONT + 1, 'z', CUT, participants=shell).name = 'light sensor'

    z_pcb_pl = b.plane_at_z(Z_PCB_FRONT)
    cy = by(PYNT_HOLES_XB[1])
    for yb_ in PYNT_HOLES_YB:
        cx = bx(yb_)
        sgn = 1.0 if cx > 0 else -1.0
        x_in, x_out = cx - sgn * 2.2, sgn * (XF_INT + 0.1)
        skA = b.sketch(z_front)
        b.rect(skA, (x_in, cy - 3.5 - (Z_PCB_FRONT - FRONT), FRONT),
                    (x_out, cy + 3.5, FRONT))
        skB = b.sketch(z_pcb_pl)
        b.rect(skB, (x_in, cy - 3.5, Z_PCB_FRONT), (x_out, cy + 3.5, Z_PCB_FRONT))
        b.loft(skA, skB, JOIN).name = 'pynt boss'
    sk = b.sketch(z_front)
    for yb_ in PYNT_HOLES_YB:
        teardrop_xy(b, sk, bx(yb_), cy, 2.5, FRONT)
    b.extrude(sk, Z_PCB_FRONT - 5.0, Z_PCB_FRONT + 6, 'z', CUT,
              participants=shell).name = 'pynt pilots'

    for yb_ in PYNT_HOLES_YB:
        cx = bx(yb_)
        sgn = 1.0 if cx > 0 else -1.0
        x_in, x_out = cx - sgn * 2.0, sgn * (XF_INT + 0.1)
        x_in_rear = x_in
        if sgn * bx(USB_YB_C) > 0:
            x_in_rear = sgn * (abs(bx(USB_YB_C)) + USB_SLOT_W / 2 + 0.25)
        sk = b.sketch(xy0)
        b.rect(sk, (x_in, FLOOR_T, 0), (x_out, PYNT_Y0 + 4.7, 0))
        b.extrude(sk, FRONT, Z_PCB_FRONT - 0.2, 'z', JOIN).name = 'ear slot front'
        # end fill relieved 26.8 -> 27.3 (owner finding: symmetric end stops
        # fought the asymmetric pilot positions — two datums, screws bound;
        # the board now centers on the screws themselves, slots only capture)
        sk = b.sketch(xy0)
        b.rect(sk, (sgn * 27.3, FLOOR_T, 0), (x_out, PYNT_Y0 + 4.7, 0))
        b.extrude(sk, Z_PCB_FRONT - 0.2, Z_PCB_FRONT + PCB_T + 0.2, 'z',
                  JOIN).name = 'ear slot end fill'
        skA = b.sketch(b.plane_at_z(Z_PCB_FRONT + PCB_T + 0.2))
        b.rect(skA, (x_in_rear, FLOOR_T, Z_PCB_FRONT + PCB_T + 0.2),
                    (x_out, PYNT_Y0 + 1.4, Z_PCB_FRONT + PCB_T + 0.2))
        skB = b.sketch(b.plane_at_z(Z_PCB_FRONT + PCB_T + 1.8))
        b.rect(skB, (x_in_rear, FLOOR_T, Z_PCB_FRONT + PCB_T + 1.8),
                    (x_out, PYNT_Y0 + 3.0, Z_PCB_FRONT + PCB_T + 1.8))
        b.loft(skA, skB, JOIN).name = 'ear slot wedge'

    # ---- bottom penetrations (rev A8) ----
    sk = b.sketch(xz0)
    b.rect(sk, (bx(USB_YB_C) - USB_SLOT_W / 2, 0, USB_SLOT_Z[0]),
               (bx(USB_YB_C) + USB_SLOT_W / 2, 0, USB_SLOT_Z[1]))
    sd_lo = bx(SD_YB_C) - SD_SLOT_W / 2 - (2.0 if bx(SD_YB_C) > 0 else 0.0)
    sd_hi = bx(SD_YB_C) + SD_SLOT_W / 2 + (2.0 if bx(SD_YB_C) < 0 else 0.0)
    b.rect(sk, (sd_lo, 0, SD_SLOT_Z[0]), (sd_hi, 0, SD_SLOT_Z[1]))
    for sx in (-1, 1):
        for k in (-1, 0, 1):
            b.rect(sk, (sx * 14.5 + k * 4.8 - 1.4, 0, 4.5),
                       (sx * 14.5 + k * 4.8 + 1.4, 0, 7.0))
    b.extrude(sk, -1, FLOOR_T + 1, 'y', CUT, participants=shell).name = 'USB/SD/vents'

    # ---- nut boss: mini-D mimicking the shell (owner request) — flat front
    # at z 16.5 (USB plug clearance preserved), straight sides to the pole
    # axis, rounded back R16.5. Same wall thicknesses around the hex pocket
    # as the old prism (2.4-2.5).
    sk = b.sketch(xz0)
    b.line(sk, (-BOSS_HX, 0, AXIS_Z - BOSS_HZ), (BOSS_HX, 0, AXIS_Z - BOSS_HZ))
    b.line(sk, (BOSS_HX, 0, AXIS_Z - BOSS_HZ), (BOSS_HX, 0, AXIS_Z))
    b.arc3(sk, (BOSS_HX, 0, AXIS_Z), (0, 0, AXIS_Z + BOSS_HX),
           (-BOSS_HX, 0, AXIS_Z))
    b.line(sk, (-BOSS_HX, 0, AXIS_Z), (-BOSS_HX, 0, AXIS_Z - BOSS_HZ))
    b.extrude(sk, 0, -BOSS_H, 'y', JOIN).name = 'nut boss (D)'

    # D-shaped FUNNEL skirt (owner request): planar tapers from the boss's
    # flat sides straight out to the housing side walls, and a lofted taper
    # from the boss's rounded back out to the housing arc — one loft does
    # both because the sections share the flat front plane at z 16.5 (the
    # USB/SD/vent strip stays clear). Side tapers ~51 deg, rear ~59 deg from
    # horizontal: the underside is self-supporting in the bottom-up print.
    yA = -BOSS_H
    skA = b.sketch_at_y(yA)
    b.line(skA, (-BOSS_HX, yA, AXIS_Z - BOSS_HZ), (BOSS_HX, yA, AXIS_Z - BOSS_HZ))
    b.line(skA, (BOSS_HX, yA, AXIS_Z - BOSS_HZ), (BOSS_HX, yA, AXIS_Z))
    b.arc3(skA, (BOSS_HX, yA, AXIS_Z), (0, yA, AXIS_Z + BOSS_HX),
           (-BOSS_HX, yA, AXIS_Z))
    b.line(skA, (-BOSS_HX, yA, AXIS_Z), (-BOSS_HX, yA, AXIS_Z - BOSS_HZ))
    skB = b.sketch_at_y(0.0)
    a = tangent_angle(R_OUT, XF_EXT)
    tx, tz = R_OUT * math.cos(a), AXIS_Z + R_OUT * math.sin(a)
    b.line(skB, (-XF_EXT, 0, AXIS_Z - BOSS_HZ), (XF_EXT, 0, AXIS_Z - BOSS_HZ))
    b.line(skB, (XF_EXT, 0, AXIS_Z - BOSS_HZ), (XF_EXT, 0, AXIS_Z))
    b.line(skB, (XF_EXT, 0, AXIS_Z), (tx, 0, tz))
    b.arc3(skB, (tx, 0, tz), (0, 0, AXIS_Z + R_OUT), (-tx, 0, tz))
    b.line(skB, (-tx, 0, tz), (-XF_EXT, 0, AXIS_Z))
    b.line(skB, (-XF_EXT, 0, AXIS_Z), (-XF_EXT, 0, AXIS_Z - BOSS_HZ))
    tf = b.loft(skA, skB, NEW)                # NewBody + combine-by-reference
    tf.name = 'D funnel'                      # (loft joins are unreliable)
    tools = adsk.core.ObjectCollection.create()
    tools.add(tf.bodies.item(0))
    ci = b.comp.features.combineFeatures.createInput(shell[0], tools)
    ci.operation = adsk.fusion.FeatureOperations.JoinFeatureOperation
    b.comp.features.combineFeatures.add(ci)
    sk = b.sketch(xz0)
    b.polygon(sk, [(NUT_R * math.cos(math.radians(60 * k)),
                    0,
                    AXIS_Z + NUT_R * math.sin(math.radians(60 * k)))
                   for k in range(6)])
    b.extrude(sk, -BOSS_H + BOSS_FLOOR, FLOOR_T + 0.1, 'y', CUT,
              participants=shell).name = 'nut pocket'
    sk = b.sketch(xz0)
    b.circle(sk, (0, 0, AXIS_Z), POLE_HOLE_D)
    b.extrude(sk, -BOSS_H - 1, -BOSS_H + BOSS_FLOOR + 1, 'y', CUT,
              participants=shell).name = 'pole hole'
    # case-back screw CLEARANCE holes + counterbores straight through the
    # funnel — owner: no extra bosses. At z 48.8 the Ø6.6 head seat at y -2
    # is a complete ring. CRITICAL: the cuts start at -18.5, BELOW the
    # deepest funnel-surface point inside the bore footprint (-14.4) — a
    # shallower cut leaves an uncut crescent shelf inside the bore that
    # blocks the screw head (owner caught it as a non-round mouth).
    sk = b.sketch(xz0)
    for sx in (-1, 1):
        b.circle(sk, (sx * CB_SCREW_X, 0, CB_SCREW_Z), 3.4)
    b.extrude(sk, -18.5, FLOOR_T + 0.1, 'y', CUT,
              participants=shell).name = 'case-back screw holes'
    sk = b.sketch(xz0)
    for sx in (-1, 1):
        b.circle(sk, (sx * CB_SCREW_X, 0, CB_SCREW_Z), 6.6)
    b.extrude(sk, -18.5, -2.0, 'y', CUT,
              participants=shell).name = 'screw counterbores'

    # chamfer the sharp exterior edges (owner request): the two front
    # vertical corners, the top outer perimeter, and the base-plane crease
    def plan_hyp(x_mm, z_mm):
        return math.hypot(x_mm, z_mm - AXIS_Z)

    def edge_info(e):
        bb = e.boundingBox
        mid = e.pointOnEdge
        return (bb.minPoint.y / CM, bb.maxPoint.y / CM,
                mid.x / CM, mid.z / CM)

    def pick_front(e):
        ymin, ymax, mx, mz = edge_info(e)
        return (abs(abs(mx) - XF_EXT) < 0.15 and abs(mz) < 0.15
                and (ymax - ymin) > 10)

    def pick_top(e):
        ymin, ymax, mx, mz = edge_info(e)
        return (abs(ymin - EXT_H) < 0.05 and abs(ymax - EXT_H) < 0.05
                and (plan_hyp(mx, mz) > 24.5 or mz < 0.2))

    def pick_base(e):
        # only the 90° edges around the flat front strip (owner markup):
        # the front-bottom line, the strip's side edges, and the crease
        # where the strip meets the funnel's flat front face. The shallow
        # wall-to-funnel creases at the sides/rear refuse to chamfer and
        # are excluded.
        ymin, ymax, mx, mz = edge_info(e)
        if not (abs(ymin) < 0.05 and abs(ymax) < 0.05):
            return False
        return (mz < 0.2
                or abs(mz - (AXIS_Z - BOSS_HZ)) < 0.35
                or (abs(mx) > XF_EXT - 0.15 and mz < AXIS_Z - BOSS_HZ + 0.1))

    # NOTE: each chamfer rebuilds the body's edges, so re-scan fresh before
    # every feature (stale edge proxies raise InternalValidationError)
    for gname, pred, size in (('front corners', pick_front, 1.2),
                              ('top rim', pick_top, 1.2),
                              ('base rim', pick_base, 0.6)):
        coll = adsk.core.ObjectCollection.create()
        for e in shell[0].edges:
            try:
                if pred(e):
                    coll.add(e)
            except Exception:
                continue
        if coll.count == 0:
            print('chamfer %s: no edges found' % gname)
            continue
        try:
            ch = b.comp.features.chamferFeatures.createInput(coll, True)
            ch.setToEqualDistance(VR(size))
            b.comp.features.chamferFeatures.add(ch)
        except Exception as ex:
            print('chamfer %s skipped: %s' % (gname, ex))

    # ---- lid: antenna pad / recess / SMA / BCD holes (all vertical) ----
    sk = b.sketch(xz0)
    b.circle(sk, (0, 0, AXIS_Z), ANT_PAD_D)
    b.extrude(sk, EXT_H, EXT_H + ANT_PAD_H, 'y', JOIN).name = 'antenna pad'
    sk = b.sketch(xz0)
    b.circle(sk, (0, 0, AXIS_Z), ANT_RECESS_D)
    b.extrude(sk, EXT_H + ANT_PAD_H + 0.1, EXT_H + ANT_PAD_H - ANT_RECESS_H,
              'y', CUT, participants=shell).name = 'antenna recess'
    sk = b.sketch(xz0)
    b.circle(sk, (0, 0, AXIS_Z), SMA_HOLE_D + 2 * FUDGE)
    b.extrude(sk, EXT_H - WALL - CB_TOP_GROOVE - 1, EXT_H + ANT_PAD_H + 1,
              'y', CUT, participants=shell).name = 'SMA bulkhead hole'
    sk = b.sketch(xz0)
    for a in (0.0, 120.0, 240.0):
        b.circle(sk, (ANT_BCD / 2 * math.sin(math.radians(a)), 0,
                      AXIS_Z + ANT_BCD / 2 * math.cos(math.radians(a))),
                 2.8 + 2 * FUDGE)
    b.extrude(sk, EXT_H - WALL - CB_TOP_GROOVE - 1, EXT_H + ANT_PAD_H + 1,
              'y', CUT, participants=shell).name = 'antenna mount holes'


def build_case_back(b):
    NEW = adsk.fusion.FeatureOperations.NewBodyFeatureOperation
    JOIN = adsk.fusion.FeatureOperations.JoinFeatureOperation
    CUT = adsk.fusion.FeatureOperations.CutFeatureOperation
    xz0 = b.comp.xZConstructionPlane

    # band with half-thickness side tongues: full 2.8 section to the aperture
    # edge, then a R24..25.4 tongue continuing into the shell's side grooves
    def bp(r, a_deg, side=1):
        a = math.radians(a_deg)
        return (side * r * math.sin(a), 0, AXIS_Z + r * math.cos(a))
    sk = b.sketch(xz0)
    o_r, o_l = bp(R_OUT, CB_TONGUE_A0), bp(R_OUT, CB_TONGUE_A0, -1)
    t1_r, t1_l = bp(CB_TONGUE_R1, CB_TONGUE_A0), bp(CB_TONGUE_R1, CB_TONGUE_A0, -1)
    t2_r, t2_l = bp(CB_TONGUE_R1, CB_TONGUE_A1), bp(CB_TONGUE_R1, CB_TONGUE_A1, -1)
    t3_r, t3_l = bp(CB_TONGUE_R0, CB_TONGUE_A1), bp(CB_TONGUE_R0, CB_TONGUE_A1, -1)
    am = (CB_TONGUE_A0 + CB_TONGUE_A1) / 2
    b.arc3(sk, o_l, (0, 0, AXIS_Z + R_OUT), o_r)
    b.line(sk, o_r, t1_r)
    b.arc3(sk, t1_r, bp(CB_TONGUE_R1, am), t2_r)
    b.line(sk, t2_r, t3_r)
    b.arc3(sk, t3_r, (0, 0, AXIS_Z + CB_TONGUE_R0), t3_l)
    b.line(sk, t3_l, t2_l)
    b.arc3(sk, t2_l, bp(CB_TONGUE_R1, am, -1), t1_l)
    b.line(sk, t1_l, o_l)
    b.extrude(sk, FLOOR_T + 0.15, EXT_H - WALL + CB_TOP_GROOVE - 0.2, 'y',
              NEW).name = 'case back band'
    band = [b.body]

    # screw bosses welded to the band's inner face at the bottom; M3x4
    # HEAT-SET inserts (owner request), set from the boss undersides before
    # the band goes in — the M3x8 screws come up through the floor
    sk = b.sketch(xz0)
    for sx in (-1, 1):
        b.circle(sk, (sx * CB_SCREW_X, 0, CB_SCREW_Z), 8.0)
    b.extrude(sk, FLOOR_T + 0.15, FLOOR_T + 8.0, 'y', JOIN,
              participants=band).name = 'screw bosses'
    sk = b.sketch(xz0)
    for sx in (-1, 1):
        b.circle(sk, (sx * CB_SCREW_X, 0, CB_SCREW_Z), INSERT_M3)
    b.extrude(sk, FLOOR_T, FLOOR_T + 4.75, 'y', CUT,
              participants=band).name = 'insert pilots (M3x4 heat-set)'

    # carrier bosses grow from the curved band to a flat mounting plane
    # (face plane z 48.5: rev A 51.5 -> 50.5 -> 49.5 skin fix -> 48.5
    # +1.0 standoff owner finding; gusset underside now ~51 deg, still
    # self-supporting)
    sk = b.sketch(b.comp.xYConstructionPlane)
    for sx in (-1, 1):
        for iy in (0, 1):
            b.circle(sk, (sx * CARRIER_HOLE_DX / 2,
                          FLOOR_T + CARRIER_Y0 + 3.0 + iy * CARRIER_HOLE_DY, 0),
                     8.0)
    # depth capped at 53.6: the band's OUTER face dips to z 53.85 at the boss
    # circles' outermost point (x 14), so anything deeper pokes through the
    # curved back; the outer third of each boss (x > ~11) welds into the band
    b.extrude(sk, CARRIER_FACE_Z, 53.6, 'z', JOIN,
              participants=band).name = 'carrier bosses (contoured)'
    sk = b.sketch(b.comp.xYConstructionPlane)
    for sx in (-1, 1):
        for iy in (0, 1):
            teardrop_xy(b, sk, sx * CARRIER_HOLE_DX / 2,
                        FLOOR_T + CARRIER_Y0 + 3.0 + iy * CARRIER_HOLE_DY,
                        INSERT_M3)
    b.extrude(sk, CARRIER_FACE_Z - 0.1, CARRIER_FACE_Z + 4.4, 'z', CUT,
              participants=band).name = 'carrier pockets (M3x4)'

    # 45° gusset fins under every carrier boss, tapering back to the band's
    # inner face — the bosses are horizontal cantilevers in the bottom-up
    # print and would otherwise need supports (owner request)
    for sx in (-1, 1):
        for iy in (0, 1):
            hx = sx * CARRIER_HOLE_DX / 2
            hy = FLOOR_T + CARRIER_Y0 + 3.0 + iy * CARRIER_HOLE_DY
            skA = b.sketch(b.plane_at_z(CARRIER_FACE_Z))
            b.rect(skA, (hx - 2.0, hy - 3.2, CARRIER_FACE_Z),
                        (hx + 2.0, hy - 3.0, CARRIER_FACE_Z))
            skB = b.sketch(b.plane_at_z(53.5))
            b.rect(skB, (hx - 2.0, hy - 7.0, 53.5),
                        (hx + 2.0, hy - 3.0, 53.5))
            b.loft(skA, skB, JOIN, participants=band).name = 'boss gusset'


def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        doc = app.documents.add(adsk.core.DocumentTypes.FusionDesignDocumentType)
        design = adsk.fusion.Design.cast(app.activeProduct)
        design.designType = adsk.fusion.DesignTypes.ParametricDesignType
        root = design.rootComponent

        occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        shell_comp = adsk.fusion.Component.cast(occ.component)
        shell_comp.name = 'Shell D'
        build_shell(Builder(shell_comp))

        occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
        cb_comp = adsk.fusion.Component.cast(occ.component)
        cb_comp.name = 'Case back D'
        build_case_back(Builder(cb_comp))

        app.activeViewport.fit()
        ang = math.degrees(math.pi / 2 - tangent_angle(R_OUT, XF_EXT))
        msg = ('Enclosure REV B (D shell) built.\n'
               'Front half identical to rev A; rear arc R%.1f about the pole '
               'axis, tangent lines from the antenna centerline (+-%.1f deg '
               'aperture).\n'
               'Case back: curved band, %.1f thick, full height; 2x M3 from '
               'below + top-groove capture.\n'
               'Carrier mount plane z %.1f (1.0 further inside).\n'
               'Print: both parts nut-side down; the band needs no support.' % (
                   R_OUT, ang, WALL, CARRIER_FACE_Z))
        if HEADLESS:
            print('shell bodies=%d, case back bodies=%d' % (
                shell_comp.bRepBodies.count, cb_comp.bRepBodies.count))
            print(msg)
        else:
            ui.messageBox(msg)
    except Exception:
        if HEADLESS:
            raise
        if ui:
            ui.messageBox('Failed:\n{}'.format(traceback.format_exc()))
