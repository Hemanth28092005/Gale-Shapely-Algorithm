#!/usr/bin/env python3
import os
import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def plot_benchmarks(csv_n="benchmarks/benchmark_n.csv", csv_l="benchmarks/benchmark_l.csv", output_png="benchmarks/benchmark_results.png"):
    if not os.path.exists(csv_n) or not os.path.exists(csv_l):
        print(f"Error: Missing CSV benchmark files ({csv_n} or {csv_l}). Run benchmark binary first.")
        sys.exit(1)

    df_n = pd.read_csv(csv_n)
    df_l = pd.read_csv(csv_l)

    plt.style.use('seaborn-v0_8-whitegrid' if 'seaborn-v0_8-whitegrid' in plt.style.available else 'default')
    fig, axes = plt.subplots(2, 2, figsize=(14, 10), dpi=300)
    fig.suptitle("Gale-Shapley (Hospitals/Residents) Performance & Scaling Analysis", fontsize=16, fontweight='bold', y=0.98)

    # Palette
    c_blue = '#1f77b4'
    c_orange = '#ff7f0e'
    c_green = '#2ca02c'
    c_purple = '#9467bd'

    # --- Plot 1: Runtime vs N ---
    ax1 = axes[0, 0]
    ax1.plot(df_n['N'], df_n['AvgTimeMs'], marker='o', linewidth=2.2, color=c_blue, label='Empirical Runtime')
    # Fit O(N) or linear line for comparison
    fit_coeff = np.polyfit(df_n['N'], df_n['AvgTimeMs'], 1)
    fit_line = np.poly1d(fit_coeff)
    ax1.plot(df_n['N'], fit_line(df_n['N']), '--', color='gray', alpha=0.8, label=f'Linear Trend ({fit_coeff[0]:.4f} ms/N)')
    ax1.set_title("Runtime vs Market Size (N Residents, M = N/2)", fontsize=12, fontweight='semibold')
    ax1.set_xlabel("Number of Residents (N)", fontsize=11)
    ax1.set_ylabel("Execution Time (ms)", fontsize=11)
    ax1.legend(loc='upper left', frameon=True)
    ax1.grid(True, linestyle='--', alpha=0.6)

    # --- Plot 2: Proposals & Bumps vs N ---
    ax2 = axes[0, 1]
    ax2.plot(df_n['N'], df_n['AvgProposals'], marker='s', linewidth=2, color=c_orange, label='Total Proposals')
    ax2.plot(df_n['N'], df_n['AvgBumps'], marker='^', linewidth=2, color=c_purple, label='Bumping Events')
    ax2.set_title("Algorithm Operations vs Market Size (N)", fontsize=12, fontweight='semibold')
    ax2.set_xlabel("Number of Residents (N)", fontsize=11)
    ax2.set_ylabel("Count of Operations", fontsize=11)
    ax2.legend(loc='upper left', frameon=True)
    ax2.grid(True, linestyle='--', alpha=0.6)

    # --- Plot 3: Runtime vs Preference List Length L ---
    ax3 = axes[1, 0]
    ax3.plot(df_l['L'], df_l['AvgTimeMs'], marker='o', linewidth=2.2, color=c_green, label='Empirical Runtime')
    ax3.set_title("Runtime vs Resident Preference Length L (Fixed N=2000, M=500)", fontsize=12, fontweight='semibold')
    ax3.set_xlabel("Preference List Length (L)", fontsize=11)
    ax3.set_ylabel("Execution Time (ms)", fontsize=11)
    ax3.legend(loc='lower right', frameon=True)
    ax3.grid(True, linestyle='--', alpha=0.6)

    # --- Plot 4: Proposals vs Preference List Length L ---
    ax4 = axes[1, 1]
    ax4.plot(df_l['L'], df_l['AvgProposals'], marker='s', linewidth=2, color=c_orange, label='Total Proposals')
    ax4.plot(df_l['L'], df_l['AvgBumps'], marker='^', linewidth=2, color=c_purple, label='Bumping Events')
    ax4.set_title("Operations vs Preference Length L (N=2000, M=500)", fontsize=12, fontweight='semibold')
    ax4.set_xlabel("Preference List Length (L)", fontsize=11)
    ax4.set_ylabel("Count of Operations", fontsize=11)
    ax4.legend(loc='lower right', frameon=True)
    ax4.grid(True, linestyle='--', alpha=0.6)

    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    plt.savefig(output_png, bbox_inches='tight')
    plt.close()
    print(f"Successfully generated benchmark chart at: {output_png}")

if __name__ == "__main__":
    csv_n = sys.argv[1] if len(sys.argv) > 1 else "benchmarks/benchmark_n.csv"
    csv_l = sys.argv[2] if len(sys.argv) > 2 else "benchmarks/benchmark_l.csv"
    out_png = sys.argv[3] if len(sys.argv) > 3 else "benchmarks/benchmark_results.png"
    plot_benchmarks(csv_n, csv_l, out_png)
