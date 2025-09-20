#!/usr/bin/env python3
"""
Upgrade ROMFS .DAT level files from 13x11 to 13x13 by appending two NB rows per level.
This version PRESERVES the original file format line-by-line:
- Keeps comments and headers like MAXLEVEL exactly as-is
- Keeps NAME and SPEED lines unchanged
- Only touches per-level brick rows: if exactly 11 rows of 13 codes are present, append 2 NB rows
- If a level already has 13 rows, it is left unchanged
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROMFS = ROOT / 'romfs'

FILES = [
    'LEVELS.DAT', 'LEVBAK.DAT', 'NASTY.DAT',
    'SPIKE1.DAT', 'SPIKE2.DAT', 'SPIKE3.DAT', 'SPIKE4.DAT'
]

BRICKS_X = 13
ROWS_OLD = 11
ROWS_NEW = 13

def is_code_row(line: str) -> bool:
    # A brick row is a line that contains 13 two-char tokens separated by spaces (and optional trailing space),
    # and typically starts with a tab in our data. We won’t require tabs to preserve varying whitespace.
    parts = [p for p in line.strip().split(' ') if p != '']
    if len(parts) != BRICKS_X:
        return False
    for p in parts:
        if len(p) != 2:
            return False
        c0, c1 = p[0], p[1]
        ok = (('A' <= c0 <= 'Z') or ('0' <= c0 <= '9')) and (('A' <= c1 <= 'Z') or ('0' <= c1 <= '9'))
        if not ok:
            return False
    return True

def make_nb_row(prefix: str, trailing_space: bool) -> str:
    row = ' '.join(['NB'] * BRICKS_X)
    if trailing_space:
        row += ' '
    return f"{prefix}{row}\n"

def upgrade_file(path: Path) -> bool:
    lines = path.read_text(errors='ignore').splitlines(keepends=True)
    out = []
    changed_any = False

    in_level = False
    in_brick_run = False
    brick_count = 0
    prefix = ''
    trailing_space = True

    def flush_brick_run():
        nonlocal in_brick_run, brick_count, changed_any
        if in_brick_run and brick_count == ROWS_OLD:
            out.append(make_nb_row(prefix, trailing_space))
            out.append(make_nb_row(prefix, trailing_space))
            changed_any = True
        in_brick_run = False
        brick_count = 0

    for line in lines:
        if line.startswith('LEVEL '):
            # Starting a new level: finalize any previous brick run
            flush_brick_run()
            in_level = True
            out.append(line)
            continue

        if in_level and is_code_row(line):
            # Inside a brick row sequence
            if not in_brick_run:
                in_brick_run = True
                # Capture formatting from this first row
                ws_len = len(line) - len(line.lstrip('\t '))
                prefix = line[:ws_len]
                trailing_space = line.rstrip('\n').endswith(' ')
            brick_count += 1
            out.append(line)
            continue

        # Non-code row or outside a level: if a brick run just ended, finalize it
        flush_brick_run()
        out.append(line)
        # Heuristic: end of level when we hit a blank line; keep in_level as-is otherwise

    # EOF: finalize any pending brick run
    flush_brick_run()

    if changed_any:
        path.write_text(''.join(out))
    return changed_any

def main():
    total = 0
    for name in FILES:
        p = ROMFS / name
        if not p.exists():
            continue
        if upgrade_file(p):
            print(f"Updated {name}")
            total += 1
        else:
            print(f"No change for {name}")
    print(f"Done. Files changed: {total}")

if __name__ == '__main__':
    main()
