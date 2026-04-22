# Captures

Raw packet captures from Phase 1. One file per button. One packet per line, hex bytes space-separated, no preamble, no address, no CRC (the nRF24 hardware handled those).

Example `power.txt`:

```
aa 55 01 00 1c
aa 55 01 01 2d
aa 55 01 02 3e
...
```

Keep ≥ 10 presses per button. If captures are noisy (occasional drops / garbled lines), annotate with `#` comments — the analysis in `decode.md` just needs clean repetitions to find byte-wise patterns.

If you're using Bastille's `nrf24-scanner.py` output, the `P:` field is what goes here.
