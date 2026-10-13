# Exact negative certificate format and verifier contract

This is the **unrestricted-root, full necessary relaxation** certificate. It is not the synchronized-pair certificate and not the optional first-high certificate.

## Mathematical contract

Tasks have cooldowns `Q = [2,2,3,3,...,11,11]`. At each row choose exactly four distinct currently available task copies. A copy is available iff its residual is zero. A selected q-copy has next residual q-1; an unselected copy has next residual max(r-1,0). Sort the two residuals of each q. Initial residuals are all zero.

The reason this is necessary for the K6 target is in the report. Each clique has at least four colors from 3..22. Choosing any four and dropping all other appearances preserves the necessary clique-index spacing ceil(color/2). Equal cooldown copies are symmetric only after passing to this weaker model.

For h positive, define `M(q,h,r) = 0 if h <= r else 1+(h-1-r)//q` and sum M over all twenty copies to obtain C(s,h). `C(s,h) < 4*h` certifies no h-row extension.

A state with certificate rank R must have every legal successor either (i) capacity-blocked at some 1 <= h < R, or (ii) listed with positive smaller rank. Induction proves no R-row extension. The root rank is 75.

## Binary encoding

`certificates/full_fast50m.bin` is 133172496 bytes: 16646562 little-endian uint64 records. No header. Let `MASK=(1<<44)-1`.

```
key = word & MASK
rank = word >> 44
state_id = key - 1
```

For q=2,...,11, sorted residuals are 0 <= a_q <= b_q < q.

```
radix_q = q*(q+1)//2
digit_q = b_q*(b_q+1)//2 + a_q
state_id = digit_2 + radix_2*digit_3 + radix_2*radix_3*digit_4 + ...
```

Recover each digit by Euclidean division, then use the unique triangular interval b(b+1)/2 <= digit < (b+1)(b+2)/2. Keys are strictly increasing. The number of possible keys is 9336040560000, below the 44-bit bound. All ranks must be positive. Root key=1, rank=75. SHA-256:

`082c86b92fa216f25c8f4dcb2abf97bfa88c3f98ca93b2de150041e8eb5e4fb0`

## Simplest independent checking pseudocode

```
read and validate every record into map R
require R[all_zero] == 75
for each state s in R:
    available = [copy indices i whose residual is zero]
    for EVERY 4-element subset A of available:
        t[i] = Q[i]-1 if i in A else max(s[i]-1,0)
        sort each equal-Q pair of t
        accept this transition if 0 < R.get(t,0) < R[s]
        otherwise require an integer h in 1..min(72,R[s]-1)
            with sum(M(Q[i],h,t[i]) for i in range(20)) < 4*h
accept only if all transitions passed
```

Checking h only through72 is sound because existence of any such obstruction suffices. It need not recognize every truly impossible state.

The C++ checker reverses the two tests to improve runtime and merges mathematically identical capacity tests. It uses the exact identity

`C(t,h) = C(base,h) - 4 + count(selected copies whose q divides h)`,

where base residuals are max(s[i]-1,0). This follows by explicitly counting occurrences at 0,q,2q,... versus q-1,2q-1,... . Mathematical checks use integers; elapsed-time reporting alone uses floating point.

Every labeled 4-subset is checked, not just grouped moves. Actual output covers 1755492299 choices. The optional `--first-high` switch belongs exclusively to a separate exploratory certificate; **do not supply it for the primary H75 certificate**. The reproduction wrapper checks the returned flag is false.

## Trust boundary

No solver needs to be installed or trusted to verify the result. The generator, Z3, heuristics and timeout reports are not proof dependencies after the exact certificate is available. As with ordinary computer-assisted proofs, file integrity, the small checker implementations, and their execution environment remain auditable components. SHA-256 is for file integrity, not a replacement for mathematical checks. The Python sample checker is additional cross-checking and not a full negative proof by itself.
