# Packing coloring of K6 clique chains

This research package gives a computer-assisted counterexample to Conjecture 2 of Furmańczyk, Gözüpek and Özkan, *Packing coloring of graphs with long paths*, [arXiv:2511.12761v1](https://arxiv.org/abs/2511.12761).

Let H_t consist of t vertex-disjoint copies of K6, with distinct vertices L_j and R_j in each clique and bridges R_j L_(j+1). A packing coloring requires vertices of color i to have graph distance strictly greater than i. The supplied proof and certificates establish

\[
\chi_p(H_t)\le 23\quad(t\ge1),\qquad
\chi_p(H_t)=23\quad(t\ge75).
\]

In particular, H75 (450 vertices, 1199 edges) disproves the proposed uniform upper bound of 22, and 23 is the sharp uniform bound for this family. The graph is the original path-aligned clique chain for every allowed alignment parameter 2≤ℓ≤6. This does not determine whether 75 is the least counterexample length, or determine every value for t<75. No worldwide-priority claim is made.

## Proof and certificates

- [Full mathematical proof](PROOF.zh-CN.md) (Chinese): graph distances, a necessary scheduling relaxation, capacity bounds, the decreasing-rank lemma, and the periodic upper bound.
- [English manuscript (PDF)](manuscript/main.pdf) and [LaTeX source](manuscript/main.tex): the complete argument and certificate verification details.
- [Exact certificate format](docs/CERTIFICATE_FORMAT.md): enough information to implement another checker.
- [Reproduction instructions](REPRODUCIBILITY.md): integrity checks, compiler commands, and expected full-verification counts.
- [Recorded full verification results](verification/negative_full_results.json): two completed implementations, with source hashes. They share the mathematical model; they are not two unrelated proofs.
- `certificates/k6_23color_period480_certificate.json`: a complete 480-clique periodic 23-color pattern.
- `certificates/H75_graph_and_23color_witness.json`: the explicit finite graph and positive witness.

The negative rank certificate has 16,646,562 records and 133,172,496 decompressed bytes. It is distributed separately as the **H75_rank_certificate.bin.gz** release asset. Its compressed and raw hashes are in [CERTIFICATE_ASSET.json](CERTIFICATE_ASSET.json). The source repository intentionally excludes the raw binary and compressed asset; download the asset from the release matching this source package, then follow the reproduction instructions. The certificate, rather than a search log, carries the finite proof evidence.

Verification requires Python 3.10+ standard library and a C++17 compiler. The main wrapper uses a GCC/Clang-compatible compiler. A portable checker and an additional standard C++ checker are included for MSVC as well. Full verification uses roughly 400 MB for certificate data and lookup storage before implementation-dependent overhead. No solver or certificate search is required. The optional generator is provided separately from the checking code.

The proof is computer-assisted, with ordinary trust in the checker, compiler and execution environment. No proof-assistant formalization or external expert-review claim is made. This work was completed with AI assistance; see [AI_USAGE.md](AI_USAGE.md).

[中文说明](README.zh-CN.md) · [Rights and attribution](RIGHTS_AND_LICENSE.md)
