#!/usr/bin/env python3
"""Records term3d spinning a model and saves it as a looping GIF.

    python3 tools/make_demo_gif.py build/term3d models/spot.obj docs/demo.gif

It runs term3d in a pseudo-terminal (a fake terminal window that a program
can't tell from a real one), decodes the frames term3d draws the same way a
terminal would, and writes them out as a GIF. Only Python's standard library
is used, so there's nothing to install. Works on Linux and macOS.

Each half-block character is two pixels, one above the other, so the GIF
shows exactly the pixels term3d drew, scaled up. The two text lines are left
out.
"""

import argparse
import fcntl
import math
import os
import pty
import re
import signal
import struct
import subprocess
import termios
import threading
import time

BACKGROUND = (13, 17, 23)  # what a dark terminal shows where nothing is drawn


def record(binary, model, cols, rows, fps, spin, seconds, extra_args):
    """Runs term3d for `seconds` and returns everything it wrote, as text."""
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
    args = [binary, model, "--fps", str(fps), "--spin", str(spin)] + extra_args
    proc = subprocess.Popen(args, stdin=slave, stdout=slave, stderr=slave)
    os.close(slave)

    # Read continuously in the background. If nobody reads, the pty's buffer
    # fills up and term3d blocks on write, which would drop frames.
    chunks = []

    def reader():
        while True:
            try:
                data = os.read(master, 1 << 16)
            except OSError:
                return
            if not data:
                return
            chunks.append(data)

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()
    time.sleep(seconds)
    proc.send_signal(signal.SIGINT)  # term3d's normal way out, like Ctrl+C
    proc.wait()
    thread.join(timeout=1.0)
    os.close(master)
    return b"".join(chunks).decode("utf-8", "replace")


def decode_frame(ansi, cols, rows):
    """Turns one frame of term3d output into rows of (r, g, b) pixels."""
    pixels = [[BACKGROUND] * cols for _ in range(rows * 2)]
    fg = bg = None
    row = col = 0
    for m in re.finditer(r"\x1b\[([0-9;]*)([A-Za-z])|(.)", ansi, re.S):
        if m.group(2) == "H":  # move the cursor to row;col (counting from 1)
            r, c = m.group(1).split(";")
            row, col = int(r) - 1, int(c) - 1
        elif m.group(2) == "m":  # change colors
            codes = [int(x) for x in m.group(1).split(";") if x] or [0]
            if codes[0] == 0:
                fg = bg = None
            elif codes[0] == 39:
                fg = None
            elif codes[0] == 49:
                bg = None
            elif codes[:2] == [38, 2]:
                fg = tuple(codes[2:5])
            elif codes[:2] == [48, 2]:
                bg = tuple(codes[2:5])
        elif m.group(3) is not None and 0 <= row < rows and 0 <= col < cols:
            ch = m.group(3)
            top, bottom = {"▀": (fg, bg), "▄": (bg, fg)}.get(ch, (bg, bg))
            pixels[row * 2][col] = top or BACKGROUND
            pixels[row * 2 + 1][col] = bottom or BACKGROUND
            col += 1
    # Drop the first and last rows of cells, which hold the text lines.
    return pixels[2:-2]


def difference(a, b):
    """How different two frames are: the number of pixels that differ."""
    return sum(pa != pb for ra, rb in zip(a, b) for pa, pb in zip(ra, rb))


def build_palette(frames):
    """Maps every color to an index in a palette of at most 256 colors.

    Shading one base color gives a few hundred shades at most. If there are
    more than a GIF can hold, the least used ones are replaced with the
    closest kept color, which no one can see in a small animation.
    """
    counts = {}
    for frame in frames:
        for row in frame:
            for p in row:
                counts[p] = counts.get(p, 0) + 1
    palette = sorted(counts, key=counts.get, reverse=True)[:256]
    index = {color: i for i, color in enumerate(palette)}
    for color in counts:
        if color not in index:
            nearest = min(palette, key=lambda p: sum((x - y) ** 2 for x, y in zip(p, color)))
            index[color] = index[nearest]
    return palette, index


def lzw_compress(data):
    """GIF's LZW compression of a list of 8-bit color indices, as bytes.

    LZW builds a dictionary of byte sequences as it goes and writes the code
    for the longest sequence it has seen before. Codes start 9 bits wide and
    grow to at most 12; when all 4096 codes are used, a "clear" code starts
    the dictionary over. The decoder rebuilds the same dictionary on its own.
    """
    clear, end = 256, 257
    out = bytearray()
    bits = nbits = 0

    def emit(code, width):
        nonlocal bits, nbits
        bits |= code << nbits  # GIF packs codes starting from the low bits
        nbits += width
        while nbits >= 8:
            out.append(bits & 0xFF)
            bits >>= 8
            nbits -= 8

    width = 9
    next_code = 258
    table = {}  # (prefix code << 8 | next byte) -> code
    emit(clear, width)
    prefix = data[0]
    for byte in data[1:]:
        key = (prefix << 8) | byte
        code = table.get(key)
        if code is not None:
            prefix = code
            continue
        emit(prefix, width)
        if next_code < 4096:
            table[key] = next_code
            # The decoder is always one code behind the encoder, so the code
            # gets wider once the code just added no longer fits.
            if next_code == (1 << width):
                width += 1
            next_code += 1
        else:
            emit(clear, width)
            table.clear()
            width, next_code = 9, 258
        prefix = byte
    emit(prefix, width)
    emit(end, width)
    if nbits:
        out.append(bits & 0xFF)
    return bytes(out)


def write_gif(path, frames, palette, index, scale, delay_cs):
    """Writes an endlessly looping GIF. delay_cs is per frame, in 1/100 s."""
    height = len(frames[0]) * scale
    width = len(frames[0][0]) * scale
    gif = bytearray(b"GIF89a")
    # Screen size, then: a 256-entry global palette follows (0xF7).
    gif += struct.pack("<HHBBB", width, height, 0xF7, 0, 0)
    for i in range(256):
        gif += bytes(palette[i] if i < len(palette) else (0, 0, 0))
    # The "NETSCAPE2.0" block makes it loop forever (0 = no limit).
    gif += b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00"

    for frame in frames:
        # Graphic control block: the delay before the next frame.
        gif += b"\x21\xF9\x04\x04" + struct.pack("<H", delay_cs) + b"\x00\x00"
        # Image block covering the whole screen, using the global palette.
        gif += b"\x2C" + struct.pack("<HHHHB", 0, 0, width, height, 0)
        data = bytearray()
        for row in frame:
            line = bytearray()
            for p in row:
                line += bytes([index[p]]) * scale
            data += bytes(line) * scale
        compressed = lzw_compress(data)
        gif += b"\x08"  # LZW starts from 8-bit color indices
        for i in range(0, len(compressed), 255):  # stored in blocks of <= 255 bytes
            block = compressed[i:i + 255]
            gif += bytes([len(block)]) + block
        gif += b"\x00"
    gif += b"\x3B"  # end of file
    with open(path, "wb") as f:
        f.write(gif)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("binary", help="path to the term3d executable")
    parser.add_argument("model", help="the .obj file to show")
    parser.add_argument("output", help="where to write the GIF")
    parser.add_argument("--cols", type=int, default=150, help="terminal width in cells")
    parser.add_argument("--rows", type=int, default=64, help="terminal height in cells")
    parser.add_argument("--scale", type=int, default=3, help="GIF pixels per terminal pixel")
    parser.add_argument("--turn-seconds", type=float, default=5.0,
                        help="time for one full turn, which is the GIF's length")
    parser.add_argument("extra", nargs="*", help="more term3d options, after --")
    args = parser.parse_args()

    fps = 25  # GIF delays are in 1/100 s, so 25 fps is exactly 4 per frame
    spin = 2 * math.pi / args.turn_seconds
    warmup = 0.5
    output = record(args.binary, args.model, args.cols, args.rows, fps, spin,
                    warmup + args.turn_seconds * 1.25, args.extra)

    raw = re.findall(r"\x1b\[\?2026h(.*?)\x1b\[\?2026l", output, re.S)
    frames = [decode_frame(f, args.cols, args.rows) for f in raw]
    frames = frames[int(warmup * fps):]  # skip startup
    if len(frames) < fps * args.turn_seconds:
        raise SystemExit(f"only captured {len(frames)} frames; is the machine busy?")

    # The capture runs in real time, so a full turn doesn't land exactly on a
    # frame. Search near where one turn should end for the frame that looks
    # most like the first one, and stop just before it. Then the loop is
    # seamless: the last frame leads straight back into the first.
    expected = round(fps * args.turn_seconds)
    window = range(expected - 5, min(expected + 6, len(frames)))
    loop_end = min(window, key=lambda i: difference(frames[0], frames[i]))
    frames = frames[:loop_end]

    # Crop to the area the model covers in any frame, plus a small margin.
    covered_rows = [y for f in frames for y, row in enumerate(f) if any(p != BACKGROUND for p in row)]
    covered_cols = [x for f in frames for row in f for x, p in enumerate(row) if p != BACKGROUND]
    margin = 3
    top = max(min(covered_rows) - margin, 0)
    bottom = min(max(covered_rows) + margin + 1, len(frames[0]))
    left = max(min(covered_cols) - margin, 0)
    right = min(max(covered_cols) + margin + 1, len(frames[0][0]))
    frames = [[row[left:right] for row in f[top:bottom]] for f in frames]

    palette, index = build_palette(frames)
    write_gif(args.output, frames, palette, index, args.scale, 100 // fps)
    size_kb = os.path.getsize(args.output) / 1024
    print(f"{args.output}: {len(frames)} frames, {(right - left) * args.scale}x"
          f"{(bottom - top) * args.scale} pixels, {len(set(index.values()))} colors, {size_kb:.0f} KB")


if __name__ == "__main__":
    main()
