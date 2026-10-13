# Reproducing the certificate checks

Use a 64-bit environment, Python 3.10 or later, and a C++17 compiler. The checkers use integer arithmetic and standard libraries; no SAT/MILP solver is needed. Allow roughly 400 MB for the full certificate and lookup table, plus runtime/compiler overhead. Verification is distinct from regenerating the certificate. Do not interpret an interrupted verification, search limit, or sampled check as a complete lower-bound proof.

## 1. Download, check and decompress the release asset

Download `H75_rank_certificate.bin.gz` from the release that accompanies this repository. Place it in the repository root, or use its actual path in the commands. `CERTIFICATE_ASSET.json` specifies the exact expected bytes and SHA256 hashes.

From the repository root, verify the compressed file:

```sh
python -c "import hashlib,json,pathlib; m=json.loads(pathlib.Path('CERTIFICATE_ASSET.json').read_text()); p=pathlib.Path(m['release_asset']); assert p.stat().st_size==m['compressed_bytes']; assert hashlib.sha256(p.read_bytes()).hexdigest()==m['compressed_sha256']; print('Compressed asset hash OK')"
```

Decompress without overwriting an existing file:

```sh
python -c "import gzip,shutil; s=gzip.open('H75_rank_certificate.bin.gz','rb'); d=open('certificates/full_fast50m.bin','xb'); shutil.copyfileobj(s,d); d.close(); s.close()"
```

Verify the decompressed file with bounded memory:

```sh
python -c "import hashlib,json,pathlib; m=json.loads(pathlib.Path('CERTIFICATE_ASSET.json').read_text()); p=pathlib.Path(m['decompressed_path']); h=hashlib.sha256(); f=p.open('rb'); [h.update(b) for b in iter(lambda:f.read(1048576),b'')]; f.close(); assert p.stat().st_size==m['decompressed_bytes']; assert h.hexdigest()==m['decompressed_sha256']; print('Raw certificate hash OK')"
```

The raw file is 133,172,496 bytes, with SHA256 `082c86b92fa216f25c8f4dcb2abf97bfa88c3f98ca93b2de150041e8eb5e4fb0`. Neither asset nor raw binary belongs in a Git commit; `.gitignore` excludes both.

## 2. Complete primary verification

With GCC or a compatible Clang driver on PATH:

```sh
python reproduce_primary.py
```

The unchanged wrapper checks the eight primary file hashes, compiles `code/verify_full_rank.cpp`, runs the entire negative certificate, checks the infinite periodic witness by BFS, checks H75 by BFS, and runs an additional Python sample check. It rejects a restricted first step and requires the complete expected state/transition counts. It creates a fresh `recheck/` directory; for another run use a new `--out-dir`. `--compiler` selects a GCC/Clang-compatible executable. It does not accept MSVC's command-line syntax.

The original checker supports a `--first-high` exploratory option. **Do not use that option for this theorem.** The wrapper rejects it. The additional Python sample verifier is supplementary only, never a substitute for the full negative check.

## 3. Second complete checker and portable first checker

The second checker is separate from the certificate generator and uses its own binary parsing, table and successor encoding. It still shares the state model and capacity/rank argument. With GCC/Clang:

```sh
g++ -O3 -std=c++17 code/independent_rank_check.cpp -o independent_rank_check
./independent_rank_check certificates/full_fast50m.bin
```

From an MSVC developer shell:

```text
cl /nologo /O2 /EHsc /std:c++17 code\independent_rank_check.cpp /Fe:independent_rank_check.exe
.\independent_rank_check.exe certificates\full_fast50m.bin
cl /nologo /O2 /EHsc /std:c++17 code\verify_full_rank_portable.cpp /Fe:verify_full_rank_portable.exe
.\verify_full_rank_portable.exe certificates\full_fast50m.bin
```

The portable first checker replaces only GNU byte-swap and population-count intrinsics with standard integer loops. This is the source variant represented in the retained first-checker execution record. Its source and the second checker's source are unchanged from their completed full runs. Source hashes appear in `verification/negative_full_results.json`.

Both normal complete runs must return success, root rank 75, 16,646,562 states, and 1,755,492,299 enumerated choices. Of these, 1,694,728,516 are covered by capacity and 60,763,783 by decreasing rank. The second checker also reports `formal_metadata: true`; **that field means its fixed-size certificate mode, not proof-assistant formalization**. Its `--self-test` and `--fixture` modes do not establish the theorem.

## 4. Positive certificates separately

```sh
python code/verify_periodic_certificate.py certificates/k6_23color_period480_certificate.json
python code/verify_H75.py certificates/H75_graph_and_23color_witness.json
```

The first checks the 480-row pattern against actual graph distances in all possible conflict windows, including repeated-period boundaries: 2880 BFS sources, 207360 distance comparisons and 17536 same-color pairs. For more distant cliques, distance is at least25>23. The second confirms the 450-vertex,1199-edge finite graph and its positive coloring, using450 BFS sources and8485 same-color pairs. These two checks establish upper bounds; the negative rank certificate is separately needed for the lower bound.

## Recorded evidence and trust boundary

`verification/negative_full_results.json` retains the mathematical outputs of two completed full executions, with timing and machine-specific data omitted. The positive records in `outputs/` retain completed exact checks with timing fields omitted. This packaging step did not repeat the full negative verification. A reproducer should run the above commands on the actual certificate instead of treating recorded PASS as a proof by itself.

SHA256 checks establish file identity. They do not certify compiler semantics or the authenticity of unsigned execution logs. This package supplies no proof-assistant kernel proof and makes no external peer-review claim. The generator is not a proof dependency: a search output is useful only if the separate full checker accepts it. The optional generator source is included for inspection; rerunning it is not required by any command above.
