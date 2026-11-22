#!/usr/bin/env python3
"""Plot transmittance.csv: estimated vs exact transmittance with error bars.

Left panel: estimate vs exact (y = x is perfect agreement), error bars = 2 standard errors.
Right panel: z-score (estimate - exact) / standard_error per fixture; |z| <= 3 band shaded.
Usage: plot_transmittance.py <transmittance.csv> <output.png>
"""
import csv
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    rows = list(csv.DictReader(open(sys.argv[1])))
    for r in rows:
        for key in ("trials", "mean", "std_error", "analytic", "z_score"):
            r[key] = float(r[key])
    estimators = sorted({r["estimator"] for r in rows})
    style = {"ratio_tracking": ("tab:blue", "o"), "delta_tracking_escape": ("tab:orange", "s")}
    fixtures = []
    for r in rows:
        if r["fixture"] not in fixtures:
            fixtures.append(r["fixture"])
    trials = int(rows[0]["trials"])

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12.5, 5.2),
                                   gridspec_kw={"width_ratios": [1, 1.25]})
    ax1.plot([0, 1], [0, 1], "k--", lw=1, label="exact (y = x)")
    for est in estimators:
        sub = [r for r in rows if r["estimator"] == est]
        color, marker = style.get(est, (None, "o"))
        ax1.errorbar([r["analytic"] for r in sub], [r["mean"] for r in sub],
                     yerr=[2 * r["std_error"] for r in sub], fmt=marker, color=color,
                     ms=5, capsize=3, lw=1, label=est.replace("_", " "))
    ax1.set_xlabel("exact transmittance  exp(-tau)")
    ax1.set_ylabel("estimated transmittance (mean of %d trials)" % trials)
    ax1.set_title("Estimate vs exact (bars: +/- 2 standard errors)")
    ax1.set_xlim(-0.02, 1.02)
    ax1.set_ylim(-0.02, 1.02)
    ax1.legend(loc="upper left", fontsize=8)
    ax1.grid(alpha=0.3)

    ypos = {name: i for i, name in enumerate(fixtures)}
    ax2.axvspan(-3, 3, color="0.9", zorder=0)
    ax2.axvline(0, color="k", lw=0.8)
    for est in estimators:
        sub = [r for r in rows if r["estimator"] == est]
        color, marker = style.get(est, (None, "o"))
        offset = -0.15 if est == estimators[0] else 0.15
        ax2.scatter([r["z_score"] for r in sub], [ypos[r["fixture"]] + offset for r in sub],
                    marker=marker, color=color, s=28, label=est.replace("_", " "), zorder=3)
    ax2.set_yticks(range(len(fixtures)))
    ax2.set_yticklabels(fixtures, fontsize=8)
    ax2.invert_yaxis()
    ax2.set_xlabel("z = (mean - exact) / standard error   (shaded: |z| <= 3)")
    ax2.set_title("Deviation in standard errors (z = 0 where SE = 0)")
    ax2.set_xlim(-5, 5)
    ax2.legend(loc="lower right", fontsize=8)
    ax2.grid(alpha=0.3, axis="x")

    fig.suptitle("Transmittance estimators vs exact optical depth (single thread)", fontsize=11)
    fig.tight_layout()
    fig.savefig(sys.argv[2], dpi=160)
    print("wrote", sys.argv[2])


if __name__ == "__main__":
    main()
