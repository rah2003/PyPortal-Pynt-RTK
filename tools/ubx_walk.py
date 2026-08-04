"""Walk UBX frames in .ubx logs: validate checksums, find real data end,
extract first/last RAWX (0x02 0x15) rcvTow/week for duration."""
import struct
import sys
from datetime import datetime, timedelta, timezone

GPS_EPOCH = datetime(1980, 1, 6, tzinfo=timezone.utc)
LEAP_S = 18


def gps_to_utc(week, tow):
    return GPS_EPOCH + timedelta(weeks=week, seconds=tow - LEAP_S)


def walk(path):
    with open(path, "rb") as f:
        data = f.read()
    n = len(data)
    off = 0
    frames = 0
    bad_ck = 0
    resyncs = 0
    last_valid_end = 0
    first_epoch = None
    last_epoch = None
    classes = {}
    while off + 8 <= n:
        if data[off] != 0xB5 or data[off + 1] != 0x62:
            # end of real data? scan ahead for next sync
            nxt = data.find(b"\xb5\x62", off + 1)
            if nxt == -1:
                break
            # count a resync only if we skipped non-zero garbage
            if any(data[off:nxt]):
                resyncs += 1
            off = nxt
            continue
        cls, mid, ln = data[off + 2], data[off + 3], struct.unpack_from(
            "<H", data, off + 4)[0]
        end = off + 6 + ln + 2
        if end > n:
            break
        ck_a = ck_b = 0
        for b in data[off + 2:off + 6 + ln]:
            ck_a = (ck_a + b) & 0xFF
            ck_b = (ck_b + ck_a) & 0xFF
        if ck_a != data[end - 2] or ck_b != data[end - 1]:
            bad_ck += 1
            off += 2
            continue
        frames += 1
        classes[(cls, mid)] = classes.get((cls, mid), 0) + 1
        last_valid_end = end
        if cls == 0x02 and mid == 0x15 and ln >= 16:  # RXM-RAWX
            tow = struct.unpack_from("<d", data, off + 6)[0]
            week = struct.unpack_from("<H", data, off + 6 + 8)[0]
            ep = gps_to_utc(week, tow)
            if first_epoch is None:
                first_epoch = ep
            last_epoch = ep
        off = end
    return {
        "file": path, "size": n, "frames": frames, "bad_ck": bad_ck,
        "resyncs": resyncs, "real_end": last_valid_end,
        "first": first_epoch, "last": last_epoch,
        "classes": classes,
    }


for p in sys.argv[1:]:
    r = walk(p)
    dur = (r["last"] - r["first"]).total_seconds() if r["first"] else 0
    print(f"\n{r['file']}")
    print(f"  size {r['size']:,} B | real data ends {r['real_end']:,} B "
          f"({100 * r['real_end'] / max(r['size'], 1):.1f}%)")
    print(f"  frames {r['frames']:,} | bad checksums {r['bad_ck']} | "
          f"resyncs {r['resyncs']}")
    if r["first"]:
        print(f"  RAWX epochs {r['first']:%Y-%m-%d %H:%M:%S}Z .. "
              f"{r['last']:%H:%M:%S}Z  ({dur / 3600:.2f} h)")
    top = sorted(r["classes"].items(), key=lambda kv: -kv[1])[:4]
    print("  top msgs: " + ", ".join(
        f"{c:02X}-{i:02X}x{k}" for (c, i), k in top))
