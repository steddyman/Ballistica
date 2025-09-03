#!/usr/bin/env python3
"""
build_t3x_from_pcx.py BASE_NAME [--maxsize WxH] [--format rgba8|rgb565|rgba5551] [--trim yes|no] [--rotate yes|no]

Given BASE_NAME (e.g. DESIGNER), reads BASE_NAME.PCX and optional BASE_NAME.INF,
slices sprites according to INF (or whole image if no INF), writes:
  - out_slices/BASE_NAME/*.png
  - BASE_NAME.t3s (tex3ds manifest, stable order)
  - BASE_NAME_indices.h (C header with #define SPR_<BASE>_<NAME> INDEX)

Requirements: Pillow (pip install pillow)

INF format expected between STARTDATA/ENDDATA:
  name x y w h
Lines starting with // or blank are ignored. Case-insensitive markers.
"""

import sys
import re
from pathlib import Path
from typing import List, Tuple, Optional

try:
    from PIL import Image
except ImportError:
    sys.exit("ERROR: Pillow not installed. Install with: pip install pillow")

def find_case_insensitive(path: Path) -> Optional[Path]:
    """Return existing file path regardless of case (e.g., NAME.PCX vs name.pcx)."""
    if path.exists():
        return path
    parent = path.parent
    stem = path.stem.lower()
    suffix = path.suffix.lower()
    for p in parent.glob(f"*{suffix}"):
        if p.stem.lower() == stem:
            return p
    return None

def parse_inf(inf_path: Path) -> List[Tuple[str,int,int,int,int]]:
    lines = inf_path.read_text(encoding="utf-8", errors="ignore").splitlines()
    data_mode = False
    out: List[Tuple[str,int,int,int,int]] = []
    for ln in lines:
        s = ln.strip()
        if not s or s.startswith("//"):
            continue
        u = s.upper()
        if "COMMENT" == u:
            continue
        if u.startswith("STARTDATA"):
            data_mode = True
            continue
        if u.startswith("ENDDATA"):
            break
        if not data_mode:
            continue
        parts = s.split()
        if len(parts) != 5:
            # ignore malformed lines silently (keeps robust to comments)
            continue
        name = parts[0]
        try:
            x, y, w, h = map(int, parts[1:])
        except ValueError:
            continue
        out.append((name, x, y, w, h))
    return out

def sanitize_macro(base: str, name: str) -> str:
    return f"SPR_{re.sub(r'[^A-Za-z0-9]+', '_', base).upper()}_{re.sub(r'[^A-Za-z0-9]+', '_', name).upper()}"

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(2)

    base = sys.argv[1]
    # defaults
    maxsize = "1024x1024"
    tex_format = "rgba8"
    trim = "no"
    rotate = "no"

    # simple arg parse
    args = sys.argv[2:]
    def next_val(flag, default):
        if flag in args:
            i = args.index(flag)
            if i + 1 < len(args):
                return args[i+1]
        return default
    maxsize = next_val("--maxsize", maxsize)
    tex_format = next_val("--format", tex_format)
    trim = next_val("--trim", trim)
    rotate = next_val("--rotate", rotate)

    base_path = Path(base)
    pcx_path = find_case_insensitive(base_path.with_suffix(".pcx"))
    if not pcx_path:
        sys.exit(f"ERROR: Could not find PCX for base '{base}' (looked for {base}.PCX / .pcx etc.)")
    inf_path = find_case_insensitive(base_path.with_suffix(".inf"))

    # Load PCX and convert to RGBA
    try:
        img = Image.open(pcx_path).convert("RGBA")
    except Exception as e:
        sys.exit(f"ERROR: failed to open {pcx_path}: {e}")

    W, H = img.size

    # Parse INF entries if present
    entries: List[Tuple[str,int,int,int,int]]
    if inf_path and inf_path.exists():
        entries = parse_inf(inf_path)
    else:
        # Fallback: one full-image sprite
        entries = [(base, 0, 0, W, H)]

    # Ensure output dir exists
    out_dir = Path("out_slices") / base
    out_dir.mkdir(parents=True, exist_ok=True)

    # Crop slices and gather file list (in INF order)
    files: List[str] = []
    header_lines: List[str] = []
    for idx, (name, x, y, w, h) in enumerate(entries):
        # clamp & validate
        x2 = max(0, min(x + w, W))
        y2 = max(0, min(y + h, H))
        x1 = max(0, min(x, W))
        y1 = max(0, min(y, H))
        if x2 <= x1 or y2 <= y1:
            # skip degenerate rectangles
            continue
        crop = img.crop((x1, y1, x2, y2))
        out_png = out_dir / f"{name}.png"
        # ensure parent exists in case of nested names (rare)
        out_png.parent.mkdir(parents=True, exist_ok=True)
        crop.save(out_png)
        files.append(out_png.as_posix())
        header_lines.append(f"#define {sanitize_macro(base, name)} {idx}")

    if not files:
        # safety fallback: write one full tile if everything was invalid
        out_png = out_dir / f"{base}.png"
        img.save(out_png)
        files.append(out_png.as_posix())
        header_lines = [f"#define {sanitize_macro(base, base)} 0"]

    # Write .t3s manifest
    t3s_path = Path(f"{base}.t3s")
    t3s_lines = [
        f"output: {base}.t3x",
        "atlas: true",
        f"maxsize: {maxsize}",
        "miplevels: 1",
        f"format: {tex_format}",
        "wrapS: clamp",
        "wrapT: clamp",
        "filterMin: linear",
        "filterMag: linear",
        f"allowRotate: {'true' if rotate.lower() in ('1','true','yes','y') else 'false'}",
        f"trim: {'true' if trim.lower() in ('1','true','yes','y') else 'false'}",
        "files:"
    ] + [f"  - {f}" for f in files]

    t3s_path.write_text("\n".join(t3s_lines) + "\n", encoding="utf-8")

    # Write header with indices
    hdr_path = Path(f"{base}_indices.h")
    hdr = (
        "// Auto-generated by build_t3x_from_pcx.py\n"
        "// Maps sprite names to atlas indices (tex3ds order)\n"
        f"// Base: {base}\n"
        "#pragma once\n\n" +
        "\n".join(header_lines) + "\n"
    )
    hdr_path.write_text(hdr, encoding="utf-8")

    print(f"[OK] Sliced {len(files)} sprites → out_slices/{base}/")
    print(f"[OK] Wrote {t3s_path} and {hdr_path}")
    print("Hint: build the atlas with: tex3ds", t3s_path.name)

if __name__ == "__main__":
    main()
