#!/usr/bin/env python3
"""Create and apply BPS patches.

The game can build its asset file from the ROM by applying a BPS patch, which is
how the English data ships without distributing any game content. This lets us
do the same for other languages: the patch is a few KB, the ROM stays the
user's own.

Only a creator was missing -- zelda3 ships an applier in C but nothing that
produces a patch.
"""
import struct, sys, zlib


def _varint(n):
    out = bytearray()
    while True:
        x = n & 0x7F
        n >>= 7
        if n == 0:
            out.append(0x80 | x)
            break
        out.append(x)
        n -= 1
    return bytes(out)


def _signed_varint(n):
    return _varint((abs(n) << 1) | (1 if n < 0 else 0))


def _read_varint(data, pos):
    value, shift = 0, 1
    while True:
        x = data[pos]
        pos += 1
        value += (x & 0x7F) * shift
        if x & 0x80:
            break
        shift <<= 7
        value += shift
    return value, pos


# Action kinds, as defined by the BPS format.
SOURCE_READ, TARGET_READ, SOURCE_COPY, TARGET_COPY = 0, 1, 2, 3

_MIN_MATCH = 8          # shorter matches cost more to encode than they save
_INDEX_CHUNK = 8
_MAX_CANDIDATES = 64    # cap per hash bucket: keeps worst-case time sane


def _build_index(buf):
    index = {}
    for i in range(0, len(buf) - _INDEX_CHUNK + 1):
        key = buf[i:i + _INDEX_CHUNK]
        bucket = index.setdefault(key, [])
        if len(bucket) < _MAX_CANDIDATES:
            bucket.append(i)
    return index


def _match_len(a, a_pos, b, b_pos, limit):
    n = 0
    while n < limit and a[a_pos + n] == b[b_pos + n]:
        n += 1
    return n


def create(source, target, metadata=b""):
    """Build a BPS patch turning 'source' into 'target'."""
    src_index = _build_index(source)
    out = bytearray(b"BPS1")
    out += _varint(len(source))
    out += _varint(len(target))
    out += _varint(len(metadata))
    out += metadata

    actions = bytearray()
    literals = bytearray()
    src_rel = 0
    tgt_rel = 0
    pos = 0
    n = len(target)

    def flush_literals():
        nonlocal literals
        if literals:
            actions.extend(_varint(((len(literals) - 1) << 2) | TARGET_READ))
            actions.extend(literals)
            literals = bytearray()

    while pos < n:
        # A run where target and source already agree at the same offset is the
        # cheapest action there is: no operand at all.
        run = 0
        while (pos + run < n and pos + run < len(source)
               and source[pos + run] == target[pos + run]):
            run += 1
        if run >= _MIN_MATCH:
            flush_literals()
            actions.extend(_varint(((run - 1) << 2) | SOURCE_READ))
            pos += run
            continue

        best_len, best_pos, best_kind = 0, 0, None
        if pos + _INDEX_CHUNK <= n:
            key = bytes(target[pos:pos + _INDEX_CHUNK])
            for cand in src_index.get(key, ()):
                limit = min(len(source) - cand, n - pos)
                length = _match_len(source, cand, target, pos, limit)
                if length > best_len:
                    best_len, best_pos, best_kind = length, cand, SOURCE_COPY
            # Repeats within the target itself are common in tile data.
            back = target.rfind(key, max(0, pos - 0x20000), pos)
            if back >= 0:
                limit = min(pos - back, n - pos)
                length = _match_len(target, back, target, pos, limit)
                if length > best_len:
                    best_len, best_pos, best_kind = length, back, TARGET_COPY

        if best_len >= _MIN_MATCH:
            flush_literals()
            actions.extend(_varint(((best_len - 1) << 2) | best_kind))
            if best_kind == SOURCE_COPY:
                actions.extend(_signed_varint(best_pos - src_rel))
                src_rel = best_pos + best_len
            else:
                actions.extend(_signed_varint(best_pos - tgt_rel))
                tgt_rel = best_pos + best_len
            pos += best_len
        else:
            literals.append(target[pos])
            pos += 1

    flush_literals()
    out += actions
    out += struct.pack("<I", zlib.crc32(source) & 0xFFFFFFFF)
    out += struct.pack("<I", zlib.crc32(target) & 0xFFFFFFFF)
    out += struct.pack("<I", zlib.crc32(bytes(out)) & 0xFFFFFFFF)
    return bytes(out)


def apply(source, patch):
    """Apply a BPS patch, verifying every checksum it carries."""
    if patch[:4] != b"BPS1":
        raise ValueError("not a BPS1 patch")
    src_crc, tgt_crc, patch_crc = struct.unpack("<III", patch[-12:])
    if zlib.crc32(patch[:-4]) & 0xFFFFFFFF != patch_crc:
        raise ValueError("patch is corrupt")
    if zlib.crc32(source) & 0xFFFFFFFF != src_crc:
        raise ValueError("source does not match the patch")

    p = 4
    _, p = _read_varint(patch, p)
    target_size, p = _read_varint(patch, p)
    meta_size, p = _read_varint(patch, p)
    p += meta_size

    out = bytearray(target_size)
    pos = src_rel = tgt_rel = 0
    end = len(patch) - 12
    while p < end:
        cmd, p = _read_varint(patch, p)
        kind, length = cmd & 3, (cmd >> 2) + 1
        if kind == SOURCE_READ:
            out[pos:pos + length] = source[pos:pos + length]
            pos += length
        elif kind == TARGET_READ:
            out[pos:pos + length] = patch[p:p + length]
            p += length
            pos += length
        else:
            raw, p = _read_varint(patch, p)
            delta = (raw >> 1) * (-1 if raw & 1 else 1)
            if kind == SOURCE_COPY:
                src_rel += delta
                out[pos:pos + length] = source[src_rel:src_rel + length]
                src_rel += length
                pos += length
            else:
                tgt_rel += delta
                for _ in range(length):
                    out[pos] = out[tgt_rel]
                    pos += 1
                    tgt_rel += 1
    result = bytes(out)
    if zlib.crc32(result) & 0xFFFFFFFF != tgt_crc:
        raise ValueError("result does not match the patch checksum")
    return result


if __name__ == "__main__":
    if len(sys.argv) != 5 or sys.argv[1] not in ("create", "apply"):
        sys.exit("usage: bps.py create <source> <target> <out.bps>\n"
                 "       bps.py apply  <source> <patch.bps> <out>")
    mode, a, b, c = sys.argv[1:]
    data_a = open(a, "rb").read()
    data_b = open(b, "rb").read()
    result = create(data_a, data_b) if mode == "create" else apply(data_a, data_b)
    open(c, "wb").write(result)
    print(f"{mode}: wrote {c} ({len(result)} bytes)")
