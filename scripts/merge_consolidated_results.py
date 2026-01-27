#!/usr/bin/env python3

"""
merge_consolidated_results.py - Merge and Compare Multiple Regulation Mechanisms

Description:
    Merges multiple consolidated results CSV files, each representing a different
    regulation mechanism. Combines slowdown ratios and degradation percentages
    for easy comparison across regulation strategies. Generates grouped bar charts
    showing slowdown ratios for each workload across all regulation mechanisms.

Usage:
    ./merge_consolidated_results.py <csv_file1> <csv_file2> [csv_file3 ...] [OPTIONS]

Arguments:
    csv_files        - Multiple consolidated CSV files (one per regulation mechanism)

Options:
    -o OUTPUT        - Output merged CSV file (default: merged_results.csv)
    -p PLOT          - Output plot file (default: slowdown_comparison.png)
    -l LABELS        - Comma-separated labels for each CSV (e.g., "MemGuard,SWAP,NoRegulation")
    -v, --verbose    - Enable verbose debug output
    -h, --help       - Show this help message
    --no-plot        - Skip generating the plot

CSV Input Format:
    Each consolidated CSV file should have:
    # workload_name, runalone_ipc, regulated_ipc, slowdown_ratio, degradation_percent
    500.perlbench_r, 1.75, 1.58, 1.1076, 9.71
    502.gcc_r, 1.21, 1.09, 1.1101, 9.92
    ...

Output CSV Format:
    workload_name, mechanism1_slowdown, mechanism1_degradation, mechanism2_slowdown, mechanism2_degradation, ...
    500.perlbench_r, 1.1076, 9.71, 1.0823, 7.45, ...
    
Output Plot:
    Grouped bar chart with:
    - X-axis: Workload names
    - Y-axis: Slowdown ratio
    - Bars: One bar per regulation mechanism for each workload

Author: SPEC Benchmark Analysis Tool
Date: 2026-01-26
"""

import sys
import argparse
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from typing import List, Dict, Optional
from pathlib import Path

#==============================================================================
# CONFIGURATION
#==============================================================================

DEFAULT_OUTPUT_CSV = "merged_results.csv"
DEFAULT_OUTPUT_PLOT = "slowdown_comparison.png"
DEFAULT_FIGSIZE = (16, 8)
DEFAULT_DPI = 300

#==============================================================================
# UTILITY FUNCTIONS
#==============================================================================

def debug(message: str, verbose: bool = False) -> None:
    """Print debug message if verbose mode is enabled."""
    if verbose:
        print(f"[DEBUG] {message}", file=sys.stderr)


def error_exit(message: str, exit_code: int = 1) -> None:
    """Print error message and exit."""
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(exit_code)


def validate_file(filepath: str, verbose: bool = False) -> None:
    """Validate that a file exists and is readable."""
    import os
    
    if not os.path.exists(filepath):
        error_exit(f"File not found: {filepath}")
    
    if not os.path.isfile(filepath):
        error_exit(f"Not a file: {filepath}")
    
    if not os.access(filepath, os.R_OK):
        error_exit(f"File is not readable: {filepath}")
    
    debug(f"Validated file: {filepath}", verbose)


#==============================================================================
# DATA LOADING FUNCTIONS
#==============================================================================

def load_consolidated_csv(filepath: str, mechanism_label: str, verbose: bool = False) -> pd.DataFrame:
    """
    Load a consolidated results CSV file.
    
    Args:
        filepath: Path to the CSV file
        mechanism_label: Label for this regulation mechanism
        verbose: Enable debug output
    
    Returns:
        DataFrame with columns: workload_name, slowdown_ratio, degradation_percent
    """
    debug(f"Loading {mechanism_label} from: {filepath}", verbose)
    
    try:
        # Read CSV, skip comment lines
        df = pd.read_csv(filepath, comment='#', header=None)
        
        debug(f"Raw CSV shape: {df.shape}", verbose)
        
        # Expected columns: workload_name, runalone_ipc, regulated_ipc, slowdown_ratio, degradation_percent
        if df.shape[1] < 5:
            error_exit(f"CSV file must have at least 5 columns: {filepath}")
        
        # Select relevant columns
        df = df.iloc[:, [0, 3, 4]]  # workload_name, slowdown_ratio, degradation_percent
        df.columns = ['workload_name', f'{mechanism_label}_slowdown', f'{mechanism_label}_degradation']
        
        # Strip whitespace from workload names
        df['workload_name'] = df['workload_name'].str.strip()
        
        # Convert metrics to numeric
        df[f'{mechanism_label}_slowdown'] = pd.to_numeric(df[f'{mechanism_label}_slowdown'], errors='coerce')
        df[f'{mechanism_label}_degradation'] = pd.to_numeric(df[f'{mechanism_label}_degradation'], errors='coerce')
        
        # Remove rows with NaN
        rows_before = len(df)
        df = df.dropna()
        rows_after = len(df)
        
        if rows_before > rows_after:
            debug(f"Dropped {rows_before - rows_after} invalid rows", verbose)
        
        debug(f"Loaded {len(df)} workloads from {mechanism_label}", verbose)
        
        if verbose and len(df) > 0:
            debug(f"Sample data from {mechanism_label}:", verbose)
            for idx, row in df.head(3).iterrows():
                debug(f"  {row['workload_name']}: slowdown={row[f'{mechanism_label}_slowdown']:.4f}, "
                      f"degradation={row[f'{mechanism_label}_degradation']:.2f}%", verbose)
        
        return df
    
    except FileNotFoundError:
        error_exit(f"CSV file not found: {filepath}")
    except pd.errors.EmptyDataError:
        error_exit(f"CSV file is empty: {filepath}")
    except Exception as e:
        error_exit(f"Failed to load CSV file {filepath}: {e}")


#==============================================================================
# MERGING FUNCTIONS
#==============================================================================

def merge_dataframes(dfs: List[pd.DataFrame], labels: List[str], verbose: bool = False) -> pd.DataFrame:
    """
    Merge multiple DataFrames on workload_name.
    
    Args:
        dfs: List of DataFrames to merge
        labels: List of mechanism labels
        verbose: Enable debug output
    
    Returns:
        Merged DataFrame with all mechanisms
    """
    debug(f"Merging {len(dfs)} DataFrames...", verbose)
    
    # Start with the first DataFrame
    merged_df = dfs[0]
    
    # Merge with remaining DataFrames
    for i, df in enumerate(dfs[1:], start=1):
        debug(f"Merging {labels[i]}...", verbose)
        merged_df = pd.merge(
            merged_df,
            df,
            on='workload_name',
            how='outer'  # Use outer join to keep all workloads
        )
    
    # Sort by workload name
    merged_df = merged_df.sort_values('workload_name').reset_index(drop=True)
    
    debug(f"Merged DataFrame shape: {merged_df.shape}", verbose)
    debug(f"Total unique workloads: {len(merged_df)}", verbose)
    
    # Report workloads with missing data
    if verbose:
        for label in labels:
            missing_count = merged_df[f'{label}_slowdown'].isna().sum()
            if missing_count > 0:
                print(f"\n[WARNING] {missing_count} workloads missing {label} data", file=sys.stderr)
    
    return merged_df


#==============================================================================
# OUTPUT FUNCTIONS
#==============================================================================

def write_merged_csv(df: pd.DataFrame, output_file: str, verbose: bool = False) -> None:
    """
    Write merged DataFrame to CSV file.
    
    Args:
        df: Merged DataFrame
        output_file: Output file path
        verbose: Enable debug output
    """
    if df.empty:
        if verbose:
            print("No results to write to CSV", file=sys.stderr)
        return
    
    debug(f"Writing {len(df)} workloads to {output_file}", verbose)
    
    try:
        with open(output_file, 'w') as f:
            # Write header with column names
            columns = df.columns.tolist()
            f.write("# " + ", ".join(columns) + "\n")
            
            # Write data
            df.to_csv(f, index=False, header=False, float_format='%.4f')
        
        if verbose:
            print(f"Merged results written to: {output_file}", file=sys.stderr)
    
    except IOError as e:
        error_exit(f"Failed to write output file {output_file}: {e}")


def create_comparison_plot(
    df: pd.DataFrame,
    labels: List[str],
    output_file: str,
    verbose: bool = False
) -> None:
    """
    Create grouped bar chart comparing slowdown ratios across mechanisms.
    
    Args:
        df: Merged DataFrame
        labels: List of mechanism labels
        output_file: Output plot file path
        verbose: Enable debug output
    """
    debug(f"Creating comparison plot: {output_file}", verbose)
    
    # Extract slowdown columns
    slowdown_cols = [f'{label}_slowdown' for label in labels]
    
    # Drop rows with any missing slowdown data for cleaner visualization
    plot_df = df[['workload_name'] + slowdown_cols].dropna()
    
    if plot_df.empty:
        error_exit("No complete data available for plotting")
    
    debug(f"Plotting {len(plot_df)} workloads with complete data", verbose)
    
    # Prepare data for plotting
    workloads = plot_df['workload_name'].tolist()
    slowdowns = {label: plot_df[f'{label}_slowdown'].tolist() for label in labels}
    
    # Set up the plot
    fig, ax = plt.subplots(figsize=DEFAULT_FIGSIZE)
    
    # Set the width of bars and positions
    num_mechanisms = len(labels)
    bar_width = 0.8 / num_mechanisms
    x = np.arange(len(workloads))
    
    # Create bars for each mechanism
    colors = plt.cm.Set3(np.linspace(0, 1, num_mechanisms))
    
    for i, label in enumerate(labels):
        offset = (i - num_mechanisms / 2 + 0.5) * bar_width
        ax.bar(
            x + offset,
            slowdowns[label],
            bar_width,
            label=label,
            color=colors[i],
            edgecolor='black',
            linewidth=0.5
        )
    
    # Customize the plot
    ax.set_xlabel('Workload Name', fontsize=12, fontweight='bold')
    ax.set_ylabel('Slowdown Ratio', fontsize=12, fontweight='bold')
    ax.set_title('Slowdown Ratio Comparison Across Regulation Mechanisms', 
                 fontsize=14, fontweight='bold', pad=20)
    ax.set_xticks(x)
    ax.set_xticklabels(workloads, rotation=45, ha='right')
    ax.legend(title='Regulation Mechanism', loc='upper left', frameon=True, fontsize=10)
    ax.grid(axis='y', alpha=0.3, linestyle='--')
    ax.axhline(y=1.0, color='red', linestyle='--', linewidth=1, alpha=0.5, label='No Slowdown (1.0x)')
    
    # Adjust layout to prevent label cutoff
    plt.tight_layout()
    
    # Save the plot
    try:
        plt.savefig(output_file, dpi=DEFAULT_DPI, bbox_inches='tight')
        if verbose:
            print(f"Plot saved to: {output_file}", file=sys.stderr)
    except Exception as e:
        error_exit(f"Failed to save plot {output_file}: {e}")
    
    plt.close()


def main():
    """Main function to merge consolidated results and create comparison plots."""
    
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        description='Merge multiple consolidated results and compare regulation mechanisms',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # Basic usage with auto-generated labels
    %(prog)s memguard.csv swap.csv noregulation.csv
    
    # With custom labels
    %(prog)s memguard.csv swap.csv noregulation.csv -l "MemGuard,SWAP,NoReg"
    
    # Custom output files
    %(prog)s file1.csv file2.csv file3.csv -o merged.csv -p comparison.png
    
    # Verbose mode
    %(prog)s memguard.csv swap.csv -v
    
    # Skip plot generation (CSV only)
    %(prog)s file1.csv file2.csv --no-plot

Input CSV Format:
    # workload_name, runalone_ipc, regulated_ipc, slowdown_ratio, degradation_percent
    500.perlbench_r, 1.75, 1.58, 1.1076, 9.71
    ...

Output CSV Format:
    # workload_name, mech1_slowdown, mech1_degradation, mech2_slowdown, mech2_degradation, ...
    500.perlbench_r, 1.1076, 9.71, 1.0823, 7.45, ...
        """
    )
    
    parser.add_argument(
        'csv_files',
        nargs='+',
        help='Consolidated CSV files (one per regulation mechanism)'
    )
    
    parser.add_argument(
        '-o', '--output',
        default=DEFAULT_OUTPUT_CSV,
        help=f'Output merged CSV file (default: {DEFAULT_OUTPUT_CSV})'
    )
    
    parser.add_argument(
        '-p', '--plot',
        default=DEFAULT_OUTPUT_PLOT,
        help=f'Output plot file (default: {DEFAULT_OUTPUT_PLOT})'
    )
    
    parser.add_argument(
        '-l', '--labels',
        help='Comma-separated labels for each CSV file (e.g., "MemGuard,SWAP,NoReg")'
    )
    
    parser.add_argument(
        '--no-plot',
        action='store_true',
        help='Skip generating the comparison plot'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Enable verbose debug output'
    )
    
    args = parser.parse_args()
    
    # Validate we have at least 2 CSV files
    if len(args.csv_files) < 2:
        error_exit("At least 2 CSV files are required for comparison")
    
    # Generate labels
    if args.labels:
        labels = [label.strip() for label in args.labels.split(',')]
        if len(labels) != len(args.csv_files):
            error_exit(f"Number of labels ({len(labels)}) must match number of CSV files ({len(args.csv_files)})")
    else:
        # Auto-generate labels from filenames
        labels = [Path(f).stem for f in args.csv_files]
        debug(f"Auto-generated labels: {labels}", args.verbose)
    
    # Validate input files
    debug("Validating input files...", args.verbose)
    for csv_file in args.csv_files:
        validate_file(csv_file, args.verbose)
    
    # Load all CSV files
    debug("Loading consolidated CSV files...", args.verbose)
    dfs = []
    for csv_file, label in zip(args.csv_files, labels):
        df = load_consolidated_csv(csv_file, label, args.verbose)
        dfs.append(df)
    
    # Merge DataFrames
    debug("Merging DataFrames...", args.verbose)
    merged_df = merge_dataframes(dfs, labels, args.verbose)
    
    # Write merged CSV
    debug(f"Writing merged CSV to {args.output}...", args.verbose)
    write_merged_csv(merged_df, args.output, args.verbose)
    
    # Create comparison plot
    if not args.no_plot:
        debug(f"Creating comparison plot...", args.verbose)
        create_comparison_plot(merged_df, labels, args.plot, args.verbose)
    
    # Success message
    if args.verbose:
        print(f"\n✓ Successfully merged {len(args.csv_files)} regulation mechanisms", file=sys.stderr)
        print(f"✓ Total workloads: {len(merged_df)}", file=sys.stderr)
        print(f"✓ CSV output: {args.output}", file=sys.stderr)
        if not args.no_plot:
            print(f"✓ Plot output: {args.plot}", file=sys.stderr)
    else:
        msg = f"Merged {len(args.csv_files)} mechanisms ({len(merged_df)} workloads) → {args.output}"
        if not args.no_plot:
            msg += f", {args.plot}"
        print(msg)


if __name__ == "__main__":
    main()
