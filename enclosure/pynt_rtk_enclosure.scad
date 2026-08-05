// PyPortal Pynt RTK — enclosure rev A
// Parametric OpenSCAD. See docs/hardware/enclosure.md for rationale.
//
// Frame: X = width (0 at center), Y = height (0 at exterior bottom face,
// +up), Z = depth (0 at exterior front face, +rearward).
// Render: set `part` below (or -D 'part="shell"' on the CLI), F6, export STL.
//
// Dimension provenance: Adafruit PyPortal Pynt .brd/.step, ArduSimple
// simpleRTK2B Lite + USB-C carrier .step, Calian HC977 datasheet 202203.
// carrier_* comes from the USB-C carrier STEP; owner confirmed 2026-07-19
// the mini-USB board on hand is identical except the connector itself.

part = "assembly"; // "shell" | "cover" | "assembly" | "base_test" | "bezel_test"

$fn = 64;
fudge = 0.15;          // printer XY compensation per side, tune to your machine

/* ---------- shell ---------- */
wall     = 2.8;
front    = 3.0;        // front (bezel) wall
cover_t  = 2.8;
floor_t  = 1.4;        // rev A7: bottom wall halved for USB plug / SD card reach
int_w    = 55.0;
int_h    = 83.0;
int_d    = 52.0;       // fits the full Ø46 antenna pad: axis_z + 23 + 1 <= shell_d

ext_w    = int_w + 2*wall;            // 60.6
ext_h    = int_h + 2*wall;            // 88.6
shell_d  = front + int_d;             // 55.0
total_d  = shell_d + cover_t;         // 57.8

/* ---------- pole / antenna axis ---------- */
// Behind the front face. Chosen so the nut boss's front face (axis_z - 14.5)
// clears both the SD card path (z<11.5) and the USB plug body (z<14.5).
axis_z   = 31.0;

/* ---------- PyPortal Pynt (#4465), exact from Adafruit CAD ---------- */
// Board frame: Xb along the 66.8 edge (0 = USB/SD edge, faces DOWN in
// rotation 2), Yb along the 43.2 edge. Ears: Yb -5.207..48.006.
pynt_mirror   = -1;    // confirmed 2026-07-19: USB lands on the viewer's
                       // LEFT (+x) looking at the screen in D2 portrait
pynt_len      = 66.802;
pynt_body_w   = 43.180;
pynt_yc       = pynt_body_w/2;        // 21.59, mirror line
pynt_y0       = floor_t;              // rev A7: ZERO standoff — board bottom edge
                                      // sits ON the floor (USB/SD reach)
pynt_holes_Xb = [2.540, 64.262];
pynt_holes_Yb = [-2.540, 45.339];
// depth stack
foam_gap      = 0.5;   // foam tape between glass perimeter and front wall
glass_h       = 3.9;   // PCB front face -> top of touch overlay
pcb_t         = 1.6;
z_pcb_front   = front + foam_gap + glass_h;   // 7.4
z_pcb_back    = z_pcb_front + pcb_t;          // 9.0
pynt_back_h   = 4.8;   // JST sockets, tallest back-side parts
// viewing window (bezel opening), board coords + margin
win_Xb = [8.49, 58.74];
win_Yb = [2.19, 40.79];
win_margin = 0.6;
// features on the bottom (USB/SD) edge, board Yb coords
usb_Yb_c   = 5.08;   usb_slot_w = 13.0;  usb_slot_z = [8.0, 13.0];
sd_Yb_c    = 19.8;   sd_slot_w  = 12.0;  sd_slot_z  = [8.2, 11.5];  // front edge aligned w/ USB
lightsensor_XbYb = [3.94, 11.43];      // pinhole through front wall

function bx(Yb) = pynt_mirror * (Yb - pynt_yc);   // board Yb -> enclosure X
function by(Xb) = pynt_y0 + Xb;                   // board Xb -> enclosure Y

/* ---------- carrier + Lite stack (mounted on the back cover) ---------- */
carrier_l       = 41.0;   // along Y, USB end DOWN
carrier_w       = 26.0;   // along X, centered
carrier_hole_dy = 35.0;   // hole grid (3 mm in from each edge)
carrier_hole_dx = 20.0;
carrier_y0      = 6.0;    // bottom edge of carrier above interior floor
carrier_boss_h  = 3.5;    // clears 1.35 mm solder tails
stack_h         = 15.3;   // carrier PCB bottom -> Lite JST-GH top (from CAD)
lite_top_off    = 9.6;    // carrier PCB bottom -> Lite PCB top face
radio_void      = 12.0;   // reserved fwd of Lite top for future XBee radio

/* ---------- antenna / SMA bulkhead (lid) ---------- */
sma_hole_d   = 6.5;      // round bulkhead — owner's adapter has no D-flat
ant_pad_d    = 46.0;     // HC977 base is 44.2
ant_recess_d = 44.8;
ant_pad_h    = 1.5;
ant_recess_h = 1.0;
ant_bcd      = 26.6;     // HC977 3x M2.5 insert bolt circle, per the Calian
                         // mechanical drawing (3x M2.5 deep 6, 120 deg)

/* ---------- 5/8"-11 pole nut boss ---------- */
// Hex pocket axis along Y; hexagon corners point along ±X, flats face ±Z
// (that combination clears the USB plug in Z and keeps 2.4 mm walls).
nut_af       = 23.83;    // across flats
nut_t        = 13.9;
nut_pocket_af= nut_af + 0.4;
nut_R        = nut_pocket_af / sqrt(3);   // circumradius ≈ 13.99 (±X extent)
boss_floor   = 3.0;
boss_h       = nut_t + 0.3 + boss_floor;  // 17.2 below the base
boss_hx      = 16.5;     // half-width  (pocket corners 13.99 + wall)
boss_hz      = 14.5;     // half-depth  (pocket flats 12.11 + wall)
pole_hole_d  = 17.0;     // clearance for the 5/8-11 stud

/* ---------- cover ---------- */
lip_h       = 3.0;
lip_t       = 2.0;
post_d      = 9.0;
post_h      = 12.0;
post_x      = int_w/2 - 3.25;             // corner posts, buried into the walls
post_dy     = int_h/2 - 3.25;
insert_d_m3  = 4.0;    // ALL printed inserts are M3 (owner stock: 4/6/8 mm
                       // lengths; 4 mm in Pynt/carrier bosses, 6-8 in posts)

/* ---------- derived / sanity checks ---------- */
z_lite_top = shell_d - carrier_boss_h - lite_top_off;    // Lite top face
echo("EXTERIOR W x H x D", ext_w, ext_h, total_d);
echo("pole shoulder -> antenna pad top (mm)", boss_h + ext_h + ant_pad_h);
echo("Lite top at z", z_lite_top, "; radio void reaches z", z_lite_top - radio_void);
assert(z_lite_top - radio_void > z_pcb_back + pynt_back_h + 1,
       "radio void collides with the back of the Pynt");
assert(axis_z - boss_hz > usb_slot_z[1] + 1.5, "nut boss too close to the USB plug");
assert(axis_z - boss_hz > sd_slot_z[1] + 1.5,  "nut boss blocks the SD card path");
assert(axis_z + boss_hz <= total_d + 0.01,     "nut boss past the cover's back face");
assert(axis_z + ant_pad_d/2 + 1 <= shell_d + 0.01,
       "antenna pad hangs past the shell's back face");
assert(ant_bcd/2 + 2 < ant_recess_d/2, "antenna screw holes outside the recess");

/* ================= modules ================= */

module rrect(w, h, r) { offset(r) offset(-r) square([w, h], center=true); }
module post_positions() {
  for (sx=[-1,1], sy=[-1,1])
    translate([sx*post_x, ext_h/2 + sy*post_dy, 0]) children();
}

module shell() {
  difference() {
    union() {
      // body
      translate([-ext_w/2, 0, 0]) cube([ext_w, ext_h, shell_d]);
      // rain eyebrow: 45° underside COLINEAR with the window's top chamfer —
      // one continuous plane from window edge to eyebrow tip (no step)
      eb0 = by(win_Xb[1]) + win_margin + front;
      translate([0, eb0, 0]) rotate([0,-90,0]) linear_extrude(ext_w - 8, center=true)
        polygon([[0,0],[0,5],[-4,5],[-4,4]]);    // (Z,Y): tip forward at z=-4
      // antenna pad, clipped to the shell's top-face footprint
      translate([0, ext_h, axis_z]) intersection() {
        rotate([-90,0,0]) cylinder(d=ant_pad_d, h=ant_pad_h);
        translate([-ext_w/2, 0, -axis_z]) cube([ext_w, ant_pad_h, shell_d]);
      }
      nut_boss_body();
      // cover corner posts (rest of their support is the wall they bury into)
      post_positions() translate([0, 0, shell_d - post_h]) cylinder(d=post_d, h=post_h);
      // Pynt TOP-ear bosses (the only bosses, rev A4): rectangular, trimmed
      // to 2.2 mm inboard of the hole center (glass side edge ~2.5 mm from
      // the hole center, owner-measured — a round Ø8 boss hits the screen),
      // merged into the side wall outboard, 45° taper on the bottom face.
      for (Yb=pynt_holes_Yb)
        let (cx = bx(Yb), sgn = bx(Yb) > 0 ? 1 : -1,
             cy = by(pynt_holes_Xb[1]),
             xa = min(cx - sgn*2.2, sgn*(int_w/2 + 0.1)),
             xb2 = max(cx - sgn*2.2, sgn*(int_w/2 + 0.1)))
          hull() {
            translate([xa, cy - 3.5 - (z_pcb_front - front), front])
              cube([xb2 - xa, 7 + (z_pcb_front - front), 0.01]);
            translate([xa, cy - 3.5, z_pcb_front - 0.01])
              cube([xb2 - xa, 7, 0.01]);
          }
      // bottom-EAR capture slots (rev A6): the board slides straight DOWN,
      // bare ear tabs (PCB only) entering 2.0 mm slots (0.2 clearance each
      // side of the 1.6 PCB). Front block trimmed 2.0 mm inboard of the
      // hole center (clears the glass, same rule as the top bosses); the
      // rear wedge's 45° face is the slide-in lead AND the print-safe
      // underside. Top screws then clamp; PCB plane unchanged.
      for (Yb=pynt_holes_Yb)
        let (cx = bx(Yb), sgn = bx(Yb) > 0 ? 1 : -1,
             xa = min(cx - sgn*2.0, sgn*(int_w/2 + 0.1)),
             xb2 = max(cx - sgn*2.0, sgn*(int_w/2 + 0.1)),
             // rear pieces (z >= 7.2) stop clear of the USB plug path
             xr = (sgn * bx(usb_Yb_c) > 0)
                  ? sgn * (abs(bx(usb_Yb_c)) + usb_slot_w/2 + 0.25)
                  : cx - sgn*2.0,
             xra = min(xr, sgn*(int_w/2 + 0.1)),
             xrb = max(xr, sgn*(int_w/2 + 0.1)))
        {
          translate([xa, floor_t, front])                     // front block -> z 7.2
            cube([xb2 - xa, pynt_y0 + 4.7 - floor_t, z_pcb_front - 0.2 - front]);
          // (rev A7: no base fill — the board edge sits ON the floor)
          // outboard end fill: ear stops ~0.9 short of the side wall — closing
          // the gap there anchors the rear wall's printed underside
          translate([min(sgn*26.8, sgn*(int_w/2 + 0.1)), floor_t, z_pcb_front - 0.2])
            cube([abs(sgn*(int_w/2 + 0.1) - sgn*26.8),
                  pynt_y0 + 4.7 - floor_t, pcb_t + 0.4]);
          hull() {   // rear wall: vertical capture face backing the PCB's lower
                     // 1.4, then a HALVED 45° lead-in
            translate([xra, floor_t, z_pcb_back + 0.2])
              cube([xrb - xra, pynt_y0 + 1.4 - floor_t, 0.01]);
            translate([xra, floor_t, z_pcb_back + 1.8 - 0.01])
              cube([xrb - xra, pynt_y0 + 3.0 - floor_t, 0.01]);
          }
        }
    }
    // interior cavity (floor at floor_t, rev A7)
    translate([-int_w/2, floor_t, front])
      cube([int_w, int_h + wall - floor_t, int_d + cover_t + 1]);
    // screen window, flaring 45° outward toward the front face
    wx0 = min(bx(win_Yb[0]), bx(win_Yb[1])) - win_margin;
    wx1 = max(bx(win_Yb[0]), bx(win_Yb[1])) + win_margin;
    wy0 = by(win_Xb[0]) - win_margin;  wy1 = by(win_Xb[1]) + win_margin;
    hull() {
      translate([(wx0+wx1)/2, (wy0+wy1)/2, front/2 + 0.01])
        cube([wx1-wx0, wy1-wy0, front], center=true);
      translate([(wx0+wx1)/2, (wy0+wy1)/2, 0])
        linear_extrude(0.02) rrect(wx1-wx0 + 2*front, wy1-wy0 + 2*front, 1.5);
    }
    // light-sensor pinhole (z-axis: teardrop, rev A8)
    translate([bx(lightsensor_XbYb[1]), by(lightsensor_XbYb[0]), -1])
      linear_extrude(front + 2) teardrop2d(2.5);
    // bottom-wall penetrations
    usb_slot(); sd_slot(); vents();
    // Pynt pilots — TOP ears only, Ø2.5: M3 threads directly into the boss
    // (no inserts — no room beside the glass); teardrop (z-axis, rev A8)
    for (Yb=pynt_holes_Yb)
      translate([bx(Yb), by(pynt_holes_Xb[1]), z_pcb_front - 5.0])
        linear_extrude(20) teardrop2d(2.5);
    // cover post pilots (heat-set M3); teardrop (z-axis, rev A8)
    post_positions() translate([0, 0, shell_d - 9])
      linear_extrude(10) teardrop2d(insert_d_m3);
    nut_boss_voids();
    // SMA bulkhead hole through top wall + pad (vertical bottom-up: plain
    // round — a teardrop notch here would spoil the gasket seat)
    translate([0, ext_h - wall - 1, axis_z]) rotate([-90,0,0])
      cylinder(d=sma_hole_d + 2*fudge, h=wall + ant_pad_h + 2);
    // 3x M2.5 clearance holes on the HC977 insert bolt circle; screws come
    // from inside the lid into the antenna's own inserts. The a=0 hole aims
    // dead REARWARD (+z): antenna-north reference faces away from the screen.
    for (a=[0,120,240])
      translate([ant_bcd/2*sin(a), ext_h - wall - 1, axis_z + ant_bcd/2*cos(a)])
        rotate([-90,0,0]) cylinder(d=2.8 + 2*fudge, h=wall + ant_pad_h + 2);
    // antenna base locating recess in the pad
    translate([0, ext_h + ant_pad_h - ant_recess_h, axis_z])
      rotate([-90,0,0]) cylinder(d=ant_recess_d, h=ant_recess_h + 1);
  }
}

module nut_boss_body() {
  // plain prism: a 45° front blend would ramp forward under the base and
  // block the USB plug / SD card, so the front face stays vertical
  translate([-boss_hx, -boss_h, axis_z - boss_hz])
    cube([2*boss_hx, boss_h, 2*boss_hz]);
}
// teardrop 2D profile for z-axis holes (rev A8: BOTH enclosures now print
// NUT-SIDE DOWN, so z-axis holes are the horizontal ones): apex toward +y
module teardrop2d(d) {
  r = d/2;
  union() {
    circle(d=d);
    polygon([[-r/1.414, r/1.414], [0, r*1.414], [r/1.414, r/1.414]]);
  }
}

module nut_boss_voids() {
  // hex pocket, axis along Y; $fn=6 puts corners at ±X, flats at ±Z
  translate([0, -boss_h + boss_floor, axis_z]) rotate([-90,0,0])
    cylinder(r=nut_R + fudge, h=boss_h - boss_floor + floor_t + 1, $fn=6);
  // pole stud clearance through the boss floor (vertical bottom-up: plain)
  translate([0, -boss_h - 1, axis_z]) rotate([-90,0,0])
    cylinder(d=pole_hole_d, h=boss_floor + 2);
}

module usb_slot() {
  x0 = bx(usb_Yb_c) - usb_slot_w/2;
  translate([x0, -1, usb_slot_z[0]])
    cube([usb_slot_w, wall + 2, usb_slot_z[1]-usb_slot_z[0]]);
}
module sd_slot() {
  // rev A7: widened 2 mm toward the enclosure center only (fit check)
  x0 = bx(sd_Yb_c) - sd_slot_w/2 - (bx(sd_Yb_c) > 0 ? 2 : 0);
  translate([x0, -1, sd_slot_z[0]])
    cube([sd_slot_w + 2, wall + 2, sd_slot_z[1]-sd_slot_z[0]]);
}
module vents() {  // 3-finger grilles: read as vents, not ports; double as drains
  // centers +-14.5: outer finger stays 1+ mm clear of the ear-slot blocks
  for (sx=[-1,1], k=[-1,0,1])
    translate([sx*14.5 + k*4.8 - 1.4, -1, 4.5]) cube([2.8, wall + 2, 2.5]);
}

module cover() {
  difference() {
    union() {
      translate([-ext_w/2, 0, shell_d]) cube([ext_w, ext_h, cover_t]);
      // labyrinth lip entering the interior
      translate([0, wall + int_h/2, shell_d]) rotate([0,180,0]) linear_extrude(lip_h)
        difference() { rrect(int_w - 2*fudge, int_h - 2*fudge, 1);
                       rrect(int_w - 2*lip_t, int_h - 2*lip_t, 1); }
      // carrier bosses (grid centered on X, USB end down)
      for (sx=[-1,1], iy=[0,1])
        translate([sx*carrier_hole_dx/2,
                   wall + carrier_y0 + 3.0 + iy*carrier_hole_dy,
                   shell_d - carrier_boss_h]) cylinder(d=8, h=carrier_boss_h);
    }
    // lip clearance around the shell's corner posts
    post_positions() translate([0, 0, shell_d - lip_h - 1])
      cylinder(d=post_d + 0.6, h=lip_h + 1);
    // carrier boss pilots — blind M3x4 heat-set pockets (boss + 1.0 into
    // the plate, 1.8 mm back skin)
    for (sx=[-1,1], iy=[0,1])
      translate([sx*carrier_hole_dx/2,
                 wall + carrier_y0 + 3.0 + iy*carrier_hole_dy,
                 shell_d - carrier_boss_h - 0.1]) cylinder(d=insert_d_m3, h=4.6);
    // countersunk M3 cover screws into the shell posts
    post_positions() translate([0, 0, shell_d - 1]) {
      cylinder(d=3.4, h=cover_t + 2);
      translate([0, 0, 1 + cover_t - 1.7]) cylinder(d1=3.4, d2=6.6, h=1.71);
    }
  }
}

/* ================= part selector ================= */
if (part == "shell") shell();
if (part == "cover") cover();
if (part == "assembly") { shell(); color("steelblue", 0.6) cover(); }
if (part == "base_test")   // bottom-front region: nut pocket, USB, SD, vents
  intersection() { shell(); translate([-ext_w/2 - 1, -boss_h - 1, -6])
                     cube([ext_w + 2, 26 + boss_h, total_d + 8]); }
if (part == "bezel_test")  // window top corner: chamfer, eyebrow, touch reach
  intersection() { shell(); translate([-ext_w/2 - 1, by(win_Xb[1]) - 12, -6])
                     cube([ext_w/2 + 1, 30, front + 14]); }
