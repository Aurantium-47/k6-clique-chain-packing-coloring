"""Reproduce the full K6 H75 proof checks, without a SAT/MILP solver.

Run from any directory: python /path/to/package/reproduce_primary.py
Outputs go to a fresh recheck/ directory, preserving all shipped evidence.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--compiler', default='g++')
    ap.add_argument('--out-dir', type=Path)
    args = ap.parse_args()
    root = Path(__file__).resolve().parent
    out = args.out_dir.resolve() if args.out_dir else root / 'recheck'
    if out.exists():
        raise ValueError(f'Output directory already exists; choose a fresh --out-dir: {out}')
    out.mkdir(parents=True)
    start = time.monotonic()
    commands = []
    summary = {'status': 'RUNNING', 'python': sys.version, 'platform': platform.platform(),
               'package_root': str(root), 'output_directory': str(out), 'commands': commands}

    def run(name: str, command: list[str]) -> dict:
        record = {'stage': name, 'command': command}
        commands.append(record)
        began = time.monotonic()
        with (out / f'{name}.stdout.txt').open('w') as stdout, (out / f'{name}.stderr.txt').open('w') as stderr:
            proc = subprocess.run(command, stdout=stdout, stderr=stderr, check=False, cwd=root)
        record.update(returncode=proc.returncode, seconds=time.monotonic() - began)
        if proc.returncode != 0:
            raise RuntimeError(f'{name} failed; inspect {out / (name + ".stderr.txt")}')
        return record

    try:
        expected = json.loads((root / 'outputs/primary_file_hashes.json').read_text())
        checked = []
        for name, info in expected.items():
            subdir = 'certificates' if name in {'full_fast50m.bin', 'k6_23color_period480_certificate.json', 'H75_graph_and_23color_witness.json'} else 'code'
            path = root / subdir / name
            actual = digest(path)
            if actual != info['sha256'] or path.stat().st_size != info['bytes']:
                raise ValueError(f'Primary file integrity check failed: {path}')
            checked.append({'path': str(path.relative_to(root)), 'sha256': actual, 'bytes': path.stat().st_size})
        summary['primary_hash_checks'] = checked
        compiler = shutil.which(args.compiler)
        if compiler is None:
            raise FileNotFoundError(f'C++ compiler not found: {args.compiler}')
        version = subprocess.run([compiler, '--version'], capture_output=True, text=True, check=True)
        summary['compiler_version'] = version.stdout
        executable = out / ('verify_full_rank.exe' if os.name == 'nt' else 'verify_full_rank')
        run('compile', [compiler, '-O3', '-std=c++17', str(root / 'code/verify_full_rank.cpp'), '-o', str(executable)])
        run('negative_full', [str(executable), str(root / 'certificates/full_fast50m.bin')])
        negative = json.loads((out / 'negative_full.stdout.txt').read_text())
        if not (negative['status'] == 'PASS' and negative['root_rank'] == 75
                and negative['first_step_contains_q11'] is False
                and negative['verified_states'] == 16646562
                and negative['covered_labeled_transition_choices'] == 1755492299):
            raise ValueError('Full negative output does not certify the claimed unrestricted H75 result')
        summary['negative_full'] = negative
        run('positive_periodic', [sys.executable, str(root / 'code/verify_periodic_certificate.py'),
                                str(root / 'certificates/k6_23color_period480_certificate.json'),
                                '--out', str(out / 'positive_periodic.json')])
        periodic = json.loads((out / 'positive_periodic.json').read_text())
        if not (periodic['status'] == 'PASS' and periodic['clique_size'] == 6
                and periodic['colors'] == 23 and periodic['period'] == 480
                and [x['minimum_distance'] for x in periodic['per_color']] == list(range(2, 25))):
            raise ValueError('Periodic23-color verification did not establish the stated result')
        summary['positive_periodic'] = {k: v for k, v in periodic.items() if k != 'per_color'}
        run('positive_H75', [sys.executable, str(root / 'code/verify_H75.py'),
                            str(root / 'certificates/H75_graph_and_23color_witness.json'),
                            '--out', str(out / 'positive_H75.json')])
        finite = json.loads((out / 'positive_H75.json').read_text())
        if not (finite['status'] == 'PASS' and finite['t'] == 75 and finite['vertices'] == 450 and finite['edges'] == 1199):
            raise ValueError('Explicit H75 graph/witness verification failed')
        summary['positive_H75'] = finite
        run('negative_python_sample', [sys.executable, str(root / 'code/verify_rank_sample.py'),
                                      str(root / 'certificates/full_fast50m.bin'),
                                      '--out', str(out / 'negative_python_sample.json')])
        sample = json.loads((out / 'negative_python_sample.json').read_text())
        if sample['status'] != 'PASS_SAMPLED_ONLY':
            raise ValueError('Additional Python sample failed')
        summary['negative_python_sample'] = {k: v for k, v in sample.items() if k != 'sample_indices'}
        summary['status'] = 'PASS'
        summary['conclusion'] = 'The full negative certificate proves no22-coloring of H75; verified23-color periodic witness proves chi_p(H_t)=23 for every t>=75, with uniform upper bound23 for all t>=1.'
    except Exception as exc:
        summary['status'] = 'FAIL'
        summary['error'] = str(exc)
    summary['seconds'] = time.monotonic() - start
    text = json.dumps(summary, ensure_ascii=False, indent=2)
    (out / 'summary.json').write_text(text + '\n')
    print(text)
    return 0 if summary['status'] == 'PASS' else 1

if __name__ == '__main__':
    raise SystemExit(main())
