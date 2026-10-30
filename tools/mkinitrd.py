#!/usr/bin/env python3
"""Собрать initrd в формате RIRD из списка файлов.
Usage: mkinitrd.py out.img file1 file2 ...
Имя внутри архива = basename файла.
"""
import sys, os, struct

def main():
    if len(sys.argv) < 3:
        print("usage: mkinitrd.py out.img files...", file=sys.stderr)
        sys.exit(1)
    out = sys.argv[1]
    files = sys.argv[2:]
    entries = []
    for path in files:
        if not os.path.isfile(path):
            print("skip missing", path, file=sys.stderr)
            continue
        name = os.path.basename(path).encode("ascii")
        with open(path, "rb") as f:
            data = f.read()
        entries.append((name, data))
    blob = bytearray(b"RIRD")
    blob += struct.pack("<I", len(entries))
    for name, data in entries:
        blob += struct.pack("<H", len(name))
        blob += name
        blob += struct.pack("<I", len(data))
        blob += data
    with open(out, "wb") as f:
        f.write(blob)
    print(f"initrd: {len(entries)} files, {len(blob)} bytes -> {out}")

if __name__ == "__main__":
    main()
