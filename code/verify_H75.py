"""Verify the explicit H75 graph and its finite 23-color witness by edge-list BFS.

This checks the positive witness only. The lower bound is proved by the
separate full negative rank certificate and its checker.
"""
from __future__ import annotations
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import time

def verify(path: Path) -> dict:
    start = time.monotonic()
    raw = path.read_bytes()
    data = json.loads(raw)
    if data['t'] != 75 or data['clique_size'] != 6 or data['vertex_count'] != 450:
        raise ValueError('Not the claimed H75 instance')
    vertices = data['vertices']
    if vertices != list(range(450)):
        raise ValueError('Vertex IDs must be exactly 0 through 449')
    colors = data['colors']
    if len(colors) != 450 or any(type(c) is not int or not 1 <= c <= 23 for c in colors):
        raise ValueError('Invalid coloring')
    given = []
    for edge in data['edges']:
        if len(edge) != 2 or any(type(v) is not int or not 0 <= v < 450 for v in edge):
            raise ValueError('Invalid edge')
        a, b = edge
        if a >= b:
            raise ValueError('Edges must be strictly ordered')
        given.append((a, b))
    if len(given) != len(set(given)):
        raise ValueError('Duplicate edge')
    expected = set()
    for a in range(450):
        for b in range(a + 1, 450):
            if a // 6 == b // 6 or (a % 6 == 5 and b == a + 1):
                expected.add((a, b))
    if set(given) != expected:
        raise ValueError('Edge list is not the specified bridge chain')
    adj = [[] for _ in vertices]
    for a, b in given:
        adj[a].append(b); adj[b].append(a)
    diameter = 0
    pairs = same_color = 0
    min_same = [None] * 24
    for source in vertices:
        dist = [-1] * 450; dist[source] = 0
        queue = deque([source])
        while queue:
            a = queue.popleft()
            for b in adj[a]:
                if dist[b] < 0:
                    dist[b] = dist[a] + 1
                    queue.append(b)
        if -1 in dist:
            raise ValueError('Disconnected graph')
        diameter = max(diameter, max(dist))
        for v in range(source + 1, 450):
            pairs += 1
            if colors[source] == colors[v]:
                same_color += 1
                c = colors[v]
                if dist[v] <= c:
                    raise ValueError(('Packing violation', source, v, c, dist[v]))
                min_same[c] = dist[v] if min_same[c] is None else min(min_same[c], dist[v])
    return {'status': 'PASS', 'certificate_sha256': hashlib.sha256(raw).hexdigest(),
            'scope': 'Explicit graph identity and finite 23-color upper bound only',
            't': 75, 'vertices': 450, 'edges': len(given), 'diameter': diameter,
            'colors_used': sorted(set(colors)), 'bfs_sources': 450,
            'unordered_vertex_pairs_checked': pairs, 'same_color_pairs_checked': same_color,
            'minimum_same_color_distances': min_same[1:],
            'seconds': time.monotonic() - start}

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('certificate', type=Path)
    ap.add_argument('--out', type=Path)
    args = ap.parse_args()
    text = json.dumps(verify(args.certificate), indent=2)
    print(text)
    if args.out:
        args.out.write_text(text + '\n')
