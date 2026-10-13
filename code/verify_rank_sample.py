"""Independent, standard-library, DIRECT-formula audit of a SAMPLE of rank states.

This is additional testing, NOT a replacement for the full C++ rank verifier.
It tests all labeled four-copy choices of the root and selected sample states.
Unlike the C++ checker it uses direct child-state capacities and binary search
in the immutable, sorted certificate, without divisor-mask transformations.
"""
from __future__ import annotations
import argparse
import hashlib
from itertools import combinations
import json
import mmap
from pathlib import Path
import random
import struct
import time

MASK = (1 << 44) - 1
Q = tuple(q for q in range(2, 12) for _ in range(2))

def decode(identifier: int) -> list[int]:
    state = []
    for q in range(2, 12):
        identifier, digit = divmod(identifier, q * (q + 1) // 2)
        b = 0
        while (b + 1) * (b + 2) // 2 <= digit:
            b += 1
        a = digit - b * (b + 1) // 2
        if not 0 <= a <= b < q:
            raise ValueError('Invalid state digit')
        state.extend((a, b))
    if identifier:
        raise ValueError('Invalid state identifier')
    return state

def encode(state: list[int]) -> int:
    identifier, multiplier = 0, 1
    for i, q in enumerate(range(2, 12)):
        a, b = sorted(state[2 * i:2 * i + 2])
        identifier += multiplier * (b * (b + 1) // 2 + a)
        multiplier *= q * (q + 1) // 2
    return identifier

def capacity(state: list[int], h: int) -> int:
    return sum(0 if h <= r else 1 + (h - 1 - r) // q
               for q, r in zip(Q, state))

def verify(path: Path, samples: int, seed: int) -> dict:
    start = time.monotonic()
    if samples < 1 or path.stat().st_size % 8:
        raise ValueError('Invalid size/sample count')
    count = path.stat().st_size // 8
    with path.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        def entry(i: int) -> int:
            return struct.unpack_from('<Q', data, i * 8)[0]
        def lookup(identifier: int) -> int:
            key, lo, hi = identifier + 1, 0, count
            while lo < hi:
                mid = (lo + hi) // 2
                if (entry(mid) & MASK) < key:
                    lo = mid + 1
                else:
                    hi = mid
            return entry(lo) >> 44 if lo < count and (entry(lo) & MASK) == key else 0
        if (entry(0) & MASK) != 1 or lookup(0) != 75:
            raise ValueError('The H75 certificate must contain the unrestricted all-zero root with rank 75')
        indices = {0, count - 1}
        indices.update(random.Random(seed).sample(range(count), min(samples, count)))
        moves = ranked = bounded = 0
        maximum_h = 0
        for index in sorted(indices):
            w = entry(index)
            identifier, rank = (w & MASK) - 1, w >> 44
            s = decode(identifier)
            if encode(s) != identifier or rank < 1:
                raise ValueError('Invalid record')
            for selected in combinations([i for i, r in enumerate(s) if r == 0], 4):
                moves += 1
                child = [max(r - 1, 0) for r in s]
                for i in selected:
                    child[i] = Q[i] - 1
                child_rank = lookup(encode(child))
                if 0 < child_rank < rank:
                    ranked += 1
                    continue
                for h in range(1, min(72, rank - 1) + 1):
                    if capacity(child, h) < 4 * h:
                        bounded += 1
                        maximum_h = max(maximum_h, h)
                        break
                else:
                    raise ValueError(('Uncovered successor', identifier, rank, selected, child_rank))
        digest = hashlib.sha256(data).hexdigest()
    return {'status': 'PASS_SAMPLED_ONLY', 'certificate_sha256': digest,
            'first_step_restricted': False, 'root_rank': 75,
            'total_certificate_states': count, 'sampled_state_count': len(indices),
            'sample_indices': sorted(indices), 'seed': seed,
            'labeled_choices_checked': moves, 'decreasing_rank_choices': ranked,
            'direct_capacity_choices': bounded, 'largest_capacity_horizon_used': maximum_h,
            'seconds': time.monotonic() - start,
            'scope_note': 'Sampled audit only. Full proof requires verify_full_rank.cpp on every record.'}

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('certificate', type=Path)
    ap.add_argument('--samples', type=int, default=128)
    ap.add_argument('--seed', type=int, default=20261013)
    ap.add_argument('--out', type=Path)
    args = ap.parse_args()
    result = verify(args.certificate, args.samples, args.seed)
    text = json.dumps(result, indent=2)
    print(text)
    if args.out:
        args.out.write_text(text + '\n')
