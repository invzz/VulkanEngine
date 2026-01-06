import struct
import sys
from pathlib import Path

# Matches VTexHeader in IBLSystem.cpp
# uint32_t magic, version, vkFormat, width, height, mipLevels, layers, bytesPerPx
HEADER_FMT = "<8I"
HEADER_SIZE = struct.calcsize(HEADER_FMT)
VTEX_MAGIC = 0x58455456  # 'VTEX'


def read_header(f):
    data = f.read(HEADER_SIZE)
    if len(data) != HEADER_SIZE:
        raise RuntimeError("File too small for VTEX header")
    magic, version, vk_format, width, height, mip_levels, layers, bpp = struct.unpack(HEADER_FMT, data)
    if magic != VTEX_MAGIC or version != 1:
        raise RuntimeError(f"Not a VTEX v1 file (magic={magic:08x}, version={version})")
    return {
        "vkFormat": vk_format,
        "width": width,
        "height": height,
        "mipLevels": mip_levels,
        "layers": layers,
        "bytesPerPx": bpp,
    }


def mip_extent(base_w, base_h, mip):
    w = max(1, base_w >> mip)
    h = max(1, base_h >> mip)
    return w, h


def iter_faces(header, payload):
    """Yields (mip, layer, data_bytes) in the same order used by loadFromDisk(): mip-major, layer-minor."""
    off = 0
    for mip in range(header["mipLevels"]):
        w, h = mip_extent(header["width"], header["height"], mip)
        face_bytes = w * h * header["bytesPerPx"]
        for layer in range(header["layers"]):
            chunk = payload[off : off + face_bytes]
            if len(chunk) != face_bytes:
                raise RuntimeError("Payload shorter than expected")
            yield mip, layer, w, h, chunk
            off += face_bytes
    if off != len(payload):
        # Some tools may pad; just report.
        pass


def stats_f32(data_bytes):
    # Interpret as float32 stream
    if len(data_bytes) % 4 != 0:
        raise RuntimeError("Data size not divisible by 4 for float32 stats")
    count = len(data_bytes) // 4
    vals = struct.unpack("<" + "f" * count, data_bytes)
    mn = min(vals)
    mx = max(vals)
    # average can overflow if NaNs; keep simple and robust
    finite = [v for v in vals if v == v and abs(v) != float("inf")]
    avg = sum(finite) / max(1, len(finite))
    nan_count = count - len([v for v in vals if v == v])
    return mn, mx, avg, nan_count


def half_to_float(h: int) -> float:
    # IEEE 754 half-precision to python float
    s = (h >> 15) & 0x1
    e = (h >> 10) & 0x1F
    f = h & 0x3FF
    if e == 0:
        if f == 0:
            return -0.0 if s else 0.0
        # subnormal
        return ((-1.0) ** s) * (2.0 ** (-14)) * (f / 1024.0)
    if e == 31:
        if f == 0:
            return float("-inf") if s else float("inf")
        return float("nan")
    return ((-1.0) ** s) * (2.0 ** (e - 15)) * (1.0 + f / 1024.0)


def stats_fp16xN(data_bytes, channels: int):
    if len(data_bytes) % 2 != 0:
        raise RuntimeError("Data size not divisible by 2 for fp16")
    half_count = len(data_bytes) // 2
    halves = struct.unpack("<" + "H" * half_count, data_bytes)
    floats = [half_to_float(h) for h in halves]
    # channel stats
    out = []
    for c in range(channels):
        vals = floats[c::channels]
        finite = [v for v in vals if v == v and abs(v) != float("inf")]
        mn = min(finite) if finite else float("nan")
        mx = max(finite) if finite else float("nan")
        avg = sum(finite) / max(1, len(finite))
        nans = len(vals) - len([v for v in vals if v == v])
        out.append((mn, mx, avg, nans))
    return out


def main():
    if len(sys.argv) < 2:
        print("Usage: python scripts/vtex_inspect.py <file.vtex> [mip] [layer]", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    mip_req = int(sys.argv[2]) if len(sys.argv) >= 3 else 0
    layer_req = int(sys.argv[3]) if len(sys.argv) >= 4 else 0

    with path.open("rb") as f:
        header = read_header(f)
        payload = f.read()

    print(f"VTEX: {path}")
    print(f"  width={header['width']} height={header['height']} mips={header['mipLevels']} layers={header['layers']} bpp={header['bytesPerPx']} vkFormat={header['vkFormat']}")
    print(f"  payloadBytes={len(payload)}")

    # Show a few samples for requested mip/layer
    found = False
    for mip, layer, w, h, chunk in iter_faces(header, payload):
        if mip == mip_req and layer == layer_req:
            found = True
            print(f"  sample mip={mip} layer={layer} extent={w}x{h} bytes={len(chunk)}")
            bpp = header["bytesPerPx"]
            if bpp == 16:
                mn, mx, avg, nans = stats_f32(chunk)
                print(f"  float32 stats: min={mn:.6g} max={mx:.6g} avg={avg:.6g} nans={nans}")
                floats = struct.unpack("<" + "f" * min(8, len(chunk) // 4), chunk[: min(8 * 4, len(chunk))])
                print("  firstF32:", ", ".join(f"{v:.6g}" for v in floats))
            elif bpp == 8:
                stats = stats_fp16xN(chunk, 4)
                for i, (mn, mx, avg, nans) in enumerate(stats):
                    print(f"  fp16 ch{i}: min={mn:.6g} max={mx:.6g} avg={avg:.6g} nans={nans}")
                halves = struct.unpack("<" + "H" * min(16, len(chunk) // 2), chunk[: min(16 * 2, len(chunk))])
                first = [half_to_float(h) for h in halves]
                print("  firstFP16(as f32):", ", ".join(f"{v:.6g}" for v in first))
            elif bpp == 4:
                stats = stats_fp16xN(chunk, 2)
                for i, (mn, mx, avg, nans) in enumerate(stats):
                    print(f"  fp16 ch{i}: min={mn:.6g} max={mx:.6g} avg={avg:.6g} nans={nans}")
                halves = struct.unpack("<" + "H" * min(16, len(chunk) // 2), chunk[: min(16 * 2, len(chunk))])
                first = [half_to_float(h) for h in halves]
                print("  firstFP16(as f32):", ", ".join(f"{v:.6g}" for v in first))
            else:
                print("  (unknown bytesPerPx for stats)")
            break

    if not found:
        print(f"  Requested mip={mip_req} layer={layer_req} not found", file=sys.stderr)
        return 3

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
