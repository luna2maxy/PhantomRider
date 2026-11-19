#!/usr/bin/env python3
"""Statistical analysis of PHR-256-YP audit harness output.

Reads dist.json and avalanche.json produced by phr_harness and writes
stats.json, stats.md, and three SVG charts. Uses only the Python stdlib so
results are reproducible on machines without numpy/scipy.
"""
from __future__ import annotations

import json
import math
import pathlib
import statistics
import sys

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE / "out"


def gammainc_q(a: float, x: float) -> float:
    """Regularized upper incomplete gamma Q(a,x)."""
    if x <= 0:
        return 1.0
    gln = math.lgamma(a)
    if x < a + 1.0:
        ap = a
        total = 1.0 / a
        term = total
        for _ in range(10000):
            ap += 1.0
            term *= x / ap
            total += term
            if abs(term) < abs(total) * 1e-15:
                break
        lower = total * math.exp(-x + a * math.log(x) - gln)
        return max(0.0, min(1.0, 1.0 - lower))
    # Lentz continued fraction for the upper tail
    tiny = 1e-300
    b = x + 1.0 - a
    c = 1.0 / tiny
    d = 1.0 / b if abs(b) > tiny else 1.0 / tiny
    h = d
    for i in range(1, 10000):
        an = -i * (i - a)
        b += 2.0
        d = an * d + b
        if abs(d) < tiny:
            d = tiny
        c = b + an / c
        if abs(c) < tiny:
            c = tiny
        d = 1.0 / d
        delta = d * c
        h *= delta
        if abs(delta - 1.0) < 1e-15:
            break
    upper = math.exp(-x + a * math.log(x) - gln) * h
    return max(0.0, min(1.0, upper))


def chi_square_p(chi2: float, df: int) -> float:
    return gammainc_q(df / 2.0, chi2 / 2.0)


def normal_two_sided_p(z: float) -> float:
    return math.erfc(abs(z) / math.sqrt(2.0))


def pearson(xs: list[float], ys: list[float]) -> float:
    n = len(xs)
    mx = sum(xs) / n
    my = sum(ys) / n
    sx = sum((v - mx) ** 2 for v in xs)
    sy = sum((v - my) ** 2 for v in ys)
    if sx <= 0 or sy <= 0:
        return 0.0
    return sum((xs[i] - mx) * (ys[i] - my) for i in range(n)) / math.sqrt(sx * sy)


def autocol(seq: list[float], lag: int) -> float:
    return pearson(seq[:-lag], seq[lag:])


def svg_bars(title: str, values: list[float], expected: float, width: int = 900, height: int = 260) -> str:
    pad_l, pad_b, pad_t = 55, 34, 28
    pw, ph = width - pad_l - 12, height - pad_b - pad_t
    vmax = max(max(values), expected) * 1.05
    bw = pw / len(values)
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">',
             f'<rect width="100%" height="100%" fill="white"/>',
             f'<text x="10" y="18" font-family="sans-serif" font-size="13">{title}</text>']
    y = lambda v: pad_t + ph - v / vmax * ph
    parts.append(f'<line x1="{pad_l}" y1="{y(expected):.1f}" x2="{width-12}" y2="{y(expected):.1f}" stroke="#cc0000" stroke-dasharray="4,3"/>')
    parts.append(f'<text x="2" y="{y(expected)+4:.1f}" font-size="10" fill="#cc0000">expected</text>')
    for i, v in enumerate(values):
        parts.append(f'<rect x="{pad_l+i*bw:.2f}" y="{y(v):.2f}" width="{max(bw-0.4,0.6):.2f}" height="{pad_t+ph-y(v):.2f}" fill="#3366aa"/>')
    parts.append(f'<line x1="{pad_l}" y1="{pad_t+ph}" x2="{width-12}" y2="{pad_t+ph}" stroke="black"/>')
    parts.append(f'<text x="{pad_l}" y="{height-8}" font-family="sans-serif" font-size="11">index 0</text>')
    parts.append(f'<text x="{width-80}" y="{height-8}" font-family="sans-serif" font-size="11">index {len(values)-1}</text>')
    parts.append('</svg>')
    return "\n".join(parts)


def main() -> int:
    dist = json.loads((OUT / "dist.json").read_text())
    av = json.loads((OUT / "avalanche.json").read_text())
    n = dist["n"]
    nac = av["n"]
    report: dict[str, object] = {}

    # ---- byte frequency (df=255)
    byte_freq = dist["byte_freq"]
    exp_byte = n * 32 / 256  # digest is 32 bytes, not 256
    chi_byte = sum((v - exp_byte) ** 2 / exp_byte for v in byte_freq)
    report["byte_frequency"] = {
        "samples": n * 256,
        "expected_per_byte": exp_byte,
        "chi_square": chi_byte,
        "df": 255,
        "p_value": chi_square_p(chi_byte, 255),
    }

    # ---- bit frequency (df=255 across the 256 output bit positions)
    bit_ones = dist["bit_ones"]
    exp_bit = n / 2.0
    chi_bit = sum((v - exp_bit) ** 2 / exp_bit for v in bit_ones)
    ones_total = sum(bit_ones)
    total_bits = n * 256
    prop = ones_total / total_bits
    z_prop = (prop - 0.5) / math.sqrt(0.25 / total_bits)
    report["bit_frequency"] = {
        "samples": total_bits,
        "ones": ones_total,
        "proportion_ones": prop,
        "chi_square_positions": chi_bit,
        "df_positions": 255,
        "p_value_positions": chi_square_p(chi_bit, 255),
        "z_global": z_prop,
        "p_global": normal_two_sided_p(z_prop),
    }

    # ---- autocorrelation of numeric hash values
    vals = [v / 2**64 for v in dist["values"]]
    ac = {str(lag): autocol(vals, lag) for lag in range(1, 11)}
    # serial correlation of consecutive bits in the concatenated bit stream
    bits: list[float] = []
    for v in dist["values"]:
        for k in range(64):
            bits.append((v >> k) & 1)
    bit_serial = autocol(bits, 1)
    report["correlation"] = {
        "autocorrelation_lag_1_to_10": ac,
        "bit_serial_correlation_lag1": bit_serial,
        "note": "|r| < 3/sqrt(N) is the approximate 99.7% band for white noise",
        "band_99_7": 3 / math.sqrt(n),
    }

    # ---- avalanche
    counts = av["counts"]
    mean = statistics.fmean(counts)
    sd = statistics.stdev(counts) if len(counts) > 1 else 0.0
    sem = sd / math.sqrt(len(counts))
    z = (mean - 128.0) / sem if sem else 0.0
    cfb = av["changed_per_bit"]
    exp_cfb = nac / 2.0
    chi_cfb = sum((v - exp_cfb) ** 2 / exp_cfb for v in cfb)
    report["avalanche"] = {
        "pairs": nac,
        "mean_bits_changed": mean,
        "mean_percent": mean / 256 * 100,
        "stddev": sd,
        "min": min(counts),
        "max": max(counts),
        "expected_mean": 128.0,
        "expected_stddev": 8.0,
        "z_vs_128": z,
        "p_vs_128": normal_two_sided_p(z),
        "chi_square_per_output_bit": chi_cfb,
        "df": 255,
        "p_value_per_output_bit": chi_square_p(chi_cfb, 255),
    }

    (OUT / "stats.json").write_text(json.dumps(report, indent=2))

    def status(p: float) -> str:
        return "PASS" if p >= 0.01 else ("WARN" if p >= 0.001 else "FAIL")

    lines = [
        "# PHR-256-YP statistical analysis (generated)",
        "",
        f"- Hash samples: **{n}** (dist), **{nac * 2}** (avalanche, {nac} pairs)",
        "",
        "| Test | Statistic | p-value | Status (alpha=0.01) |",
        "|---|---|---|---|",
        f"| Byte frequency chi-square | {chi_byte:.2f} (df=255) | {report['byte_frequency']['p_value']:.4g} | {status(report['byte_frequency']['p_value'])} |",
        f"| Bit frequency by position chi-square | {chi_bit:.2f} (df=255) | {report['bit_frequency']['p_value_positions']:.4g} | {status(report['bit_frequency']['p_value_positions'])} |",
        f"| Global ones proportion | {prop:.6f} (z={z_prop:.3f}) | {report['bit_frequency']['p_global']:.4g} | {status(report['bit_frequency']['p_global'])} |",
        f"| Avalanche per-bit chi-square | {chi_cfb:.2f} (df=255) | {report['avalanche']['p_value_per_output_bit']:.4g} | {status(report['avalanche']['p_value_per_output_bit'])} |",
        f"| Avalanche mean vs 128 | {mean:.3f} (z={z:.3f}) | {report['avalanche']['p_vs_128']:.4g} | {status(report['avalanche']['p_vs_128'])} |",
        "",
        "## Avalanche distribution",
        "",
        f"mean={mean:.3f} bits ({mean/256*100:.2f}%), stddev={sd:.3f}, min={min(counts)}, max={max(counts)}, expected N(128, 8).",
        "",
        "## Autocorrelation (lag 1..10, numeric hash values)",
        "",
        "| lag | r |",
        "|---|---|",
    ]
    for lag in range(1, 11):
        lines.append(f"| {lag} | {ac[str(lag)]:+.5f} |")
    lines += ["", f"Bit-stream serial correlation lag-1: {bit_serial:+.6f}; 99.7% band = +/-{report['correlation']['band_99_7']:.5f}.", ""]
    lines.append("PASS/FAIL here is only evidence of uniformity on sampled inputs; it is **not** proof of cryptographic security.")
    (OUT / "stats.md").write_text("\n".join(lines) + "\n")

    (OUT / "chart_byte_freq.svg").write_text(
        svg_bars("PHR-256-YP byte frequency (all digest bytes)", byte_freq, exp_byte))
    hist = [0] * 33
    for c in counts:
        hist[min(c // 8, 32)] += 1
    (OUT / "chart_avalanche_hist.svg").write_text(
        svg_bars("Avalanche: bits changed per hash (bucket = 8 bits)", hist, nac / 33))
    deviations = [(v - exp_cfb) / math.sqrt(exp_cfb / 2) for v in cfb]
    (OUT / "chart_avalanche_bits.svg").write_text(
        svg_bars("Avalanche: normalised deviation per output bit", deviations, 0.0))
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
