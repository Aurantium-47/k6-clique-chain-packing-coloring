"""Standard-library independent verifier for a repeated clique-chain coloring.

It builds the graph from its edges and runs BFS. No search code, solver,
third-party package, or assert statement is needed. A separate exact formula
and successive-occurrence check are also performed.
"""
from __future__ import annotations
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import time

def graph(cliques: int, size: int) -> list[list[int]]:
    if cliques < 1 or size < 2:
        raise ValueError('Need positive clique count and at least two vertices')
    adj = [[] for _ in range(cliques * size)]
    def edge(a: int, b: int) -> None:
        adj[a].append(b); adj[b].append(a)
    for j in range(cliques):
        for a in range(size):
            for b in range(a + 1, size):
                edge(j * size + a, j * size + b)
        if j:
            edge(j * size - 1, j * size)
    return adj

def bfs(adj: list[list[int]], source: int) -> list[int]:
    distances = [-1] * len(adj)
    distances[source] = 0
    queue = deque([source])
    while queue:
        u = queue.popleft()
        for v in adj[u]:
            if distances[v] < 0:
                distances[v] = distances[u] + 1
                queue.append(v)
    return distances

def verify(data: dict) -> dict:
    start = time.monotonic()
    rows = data['rows']; k = data['k']; n = data['n']; p = len(rows)
    if type(k) is not int or type(n) is not int or k < 1 or n < 2 or p < 1:
        raise ValueError('Invalid dimensions')
    for j, row in enumerate(rows):
        if len(row) != n or len(set(row)) != n:
            raise ValueError(('Repeated color or wrong row size', j))
        if any(type(c) is not int or not 1 <= c <= k for c in row):
            raise ValueError(('Invalid color', j))
    radius = (k + 1) // 2
    adj = graph(p + radius, n)
    checked_pairs = 0
    bfs_sources = 0
    formula_comparisons = 0
    for j, row in enumerate(rows):
        for a, c in enumerate(row):
            distances = bfs(adj, j * n + a)
            bfs_sources += 1
            for h in range(1, radius + 1):
                for b, d in enumerate(rows[(j + h) % p]):
                    actual = distances[(j + h) * n + b]
                    formula = 2 * h - 1 + (a != n - 1) + (b != 0)
                    formula_comparisons += 1
                    if actual != formula:
                        raise ValueError(('BFS/formula disagreement', j, a, h, b))
                    if c == d:
                        checked_pairs += 1
                        if actual <= c:
                            raise ValueError(('Packing violation', j, a, h, b, c, actual))
    per_color = []
    successive_checks = 0
    for c in range(1, k + 1):
        occ = [(j, row.index(c)) for j, row in enumerate(rows) if c in row]
        gaps = []
        for index, (j, a) in enumerate(occ):
            jj, b = occ[(index + 1) % len(occ)]
            if index == len(occ) - 1:
                jj += p
            h = jj - j
            distance = 2 * h - 1 + (a != n - 1) + (b != 0)
            if distance <= c:
                raise ValueError(('Successive violation', c, j, jj, distance))
            gaps.append({'from_clique': j, 'to_clique': jj,
                         'from_position': a, 'to_position': b,
                         'clique_gap': h, 'graph_distance': distance})
            successive_checks += 1
        per_color.append({'color': c, 'count': len(occ),
                          'minimum_distance': min((x['graph_distance'] for x in gaps), default=None),
                          'successive_pairs': gaps})
    return {'status': 'PASS', 'clique_size': n, 'colors': k, 'period': p,
            'bfs_graph_cliques': p + radius, 'bfs_sources': bfs_sources,
            'bfs_formula_comparisons': formula_comparisons,
            'bfs_same_color_pairs_checked': checked_pairs,
            'successive_pairs_checked': successive_checks,
            'far_pair_distance_lower_bound': 2 * (radius + 1) - 1,
            'per_color': per_color, 'seconds': time.monotonic() - start}

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('certificate', type=Path)
    ap.add_argument('--out', type=Path)
    args = ap.parse_args()
    raw = args.certificate.read_bytes()
    result = verify(json.loads(raw))
    result['certificate_sha256'] = hashlib.sha256(raw).hexdigest()
    text = json.dumps(result, indent=2)
    print(text)
    if args.out:
        args.out.write_text(text + '\n')
