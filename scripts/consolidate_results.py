#!/usr/bin/env python3

"""
consolidate_results.py - SPEC Benchmark Results Consolidator (Pandas Version)

Description:
    Consolidates SPEC benchmark results by comparing runalone (reference) IPC values
    with coscheduled (regulated) IPC values. Calculates performance degradation and 
    slowdown metrics using pandas DataFrames.

Usage:
    ./consolidate_results.py <reference_csv> <coscheduled_csv> [-o OUTPUT] [-v] [-h]

Arguments:
    reference_csv    - CSV file with runalone workload names and IPC values
    coscheduled_csv  - CSV file with coscheduled workload names and IPC values

Options:
    -o OUTPUT        - Output CSV file name (default: consolidated_results.csv)
    -v, --verbose    - Enable verbose debug output
    -h, --help       - Show this help message

CSV Input Format:
    Both input CSV files support these formats:
    
    Format 1 (2 columns):
    # workload_name, IPC
    500.perlbench_r, 1.75
    502.gcc_r, 1.21
    ...
    
    Format 2 (4 columns - only first 2 are used):
    # fg_workload, fg_IPC, bg_workload, bg_IPC
    500.perlbench_r, 1.75, 519.lbm_r, 0.82
    502.gcc_r, 1.21, 519.lbm_r, 0.85
    ...
    
    Note: When using 4-column format, only the first 2 columns 
    (foreground workload and its IPC) are used for consolidation.

Output Format:
    # workload_name, runalone_ipc, regulated_ipc, slowdown_ratio, degradation_percent
    
    Where:
    - runalone_ipc: IPC from reference_csv (runalone performance)
    - regulated_ipc: IPC from coscheduled_csv (coscheduled performance)
    - slowdown_ratio: runalone_ipc / regulated_ipc
    - degradation_percent: ((runalone - regulated) / runalone) × 100


Date: 2026-01-26
"""

import sys
import argparse
import pandas as pd
from typing import Optional

#==============================================================================
# CONFIGURATION
#==============================================================================

DEFAULT_OUTPUT_FILE = "consolidated_results.csv"

#==============================================================================
# UTILITY FUNCTIONS
#==============================================================================

def debug(message: str, verbose: bool = False) -> None:
    """
    Print debug message if verbose mode is enabled.
    
    Args:
        message: Message to print
        verbose: Whether verbose mode is enabled
    """
    if verbose:
        print(f"[DEBUG] {message}", file=sys.stderr)


def error_exit(message: str, exit_code: int = 1) -> None:
    """
    Print error message and exit.
    
    Args:
        message: Error message to print
        exit_code: Exit code (default: 1)
    """
    print(f"ERROR: {message}", file=sys.stderr)
    sys.exit(exit_code)


def validate_file(filepath: str, file_type: str, verbose: bool = False) -> None:
    """
    Validate that a file exists and is readable.
    
    Args:
        filepath: Path to the file
        file_type: Description of file type (for error messages)
        verbose: Enable debug output
    """
    import os
    
    if not os.path.exists(filepath):
        error_exit(f"{file_type} file not found: {filepath}")
    
    if not os.path.isfile(filepath):
        error_exit(f"{file_type} is not a file: {filepath}")
    
    if not os.access(filepath, os.R_OK):
        error_exit(f"{file_type} file is not readable: {filepath}")
    
    debug(f"Validated {file_type}: {filepath}", verbose)


#==============================================================================
# DATA LOADING FUNCTIONS
#==============================================================================

def load_csv_to_dataframe(filepath: str, verbose: bool = False) -> pd.DataFrame:
    """
    Load a CSV file into a pandas DataFrame.
    
    Args:
        filepath: Path to the CSV file
        verbose: Enable debug output
    
    Returns:
        DataFrame with columns: workload, ipc
    
    CSV Format (supports both):
        Format 1 (2 columns):
        # workload_name, IPC
        500.perlbench_r, 1.75
        
        Format 2 (4 columns - uses only first 2):
        # fg_workload, fg_IPC, bg_workload, bg_IPC
        500.perlbench_r, 1.75, 519.lbm_r, 0.82
    """
    debug(f"Loading CSV file: {filepath}", verbose)
    
    try:
        # Read CSV, skip comment lines starting with #
        df = pd.read_csv(filepath, comment='#', header=None)
        
        debug(f"Raw CSV shape: {df.shape}", verbose)
        
        # Check if we have at least 2 columns
        if df.shape[1] < 2:
            error_exit(f"CSV file must have at least 2 columns: {filepath}")
        
        # Use only first 2 columns (workload name and IPC)
        df = df.iloc[:, :2]
        df.columns = ['workload', 'ipc']
        
        # Strip whitespace from workload names
        df['workload'] = df['workload'].str.strip()
        
        # Convert IPC to numeric, coerce errors to NaN
        df['ipc'] = pd.to_numeric(df['ipc'], errors='coerce')
        
        # Count rows before cleaning
        rows_before = len(df)
        
        # Remove rows with NaN values
        df = df.dropna()
        
        # Remove duplicate workloads (keep first occurrence)
        df = df.drop_duplicates(subset=['workload'], keep='first')
        
        rows_after = len(df)
        rows_dropped = rows_before - rows_after
        
        if rows_dropped > 0:
            debug(f"Dropped {rows_dropped} invalid/duplicate rows", verbose)
        
        debug(f"Loaded {len(df)} valid workloads from {filepath}", verbose)
        
        if verbose and len(df) > 0:
            debug(f"Sample data from {filepath}:", verbose)
            for idx, row in df.head(3).iterrows():
                debug(f"  {row['workload']} -> {row['ipc']:.4f}", verbose)
        
        return df
    
    except FileNotFoundError:
        error_exit(f"CSV file not found: {filepath}")
    except pd.errors.EmptyDataError:
        error_exit(f"CSV file is empty: {filepath}")
    except Exception as e:
        error_exit(f"Failed to load CSV file {filepath}: {e}")


#==============================================================================
# CONSOLIDATION FUNCTIONS
#==============================================================================

def consolidate_dataframes(
    reference_df: pd.DataFrame,
    coscheduled_df: pd.DataFrame,
    verbose: bool = False
) -> pd.DataFrame:
    """
    Consolidate reference and coscheduled DataFrames.
    
    Args:
        reference_df: DataFrame with runalone workload data (workload, ipc)
        coscheduled_df: DataFrame with coscheduled workload data (workload, ipc)
        verbose: Enable debug output
    
    Returns:
        Consolidated DataFrame with columns:
        - workload_name
        - runalone_ipc
        - regulated_ipc
        - slowdown_ratio
        - degradation_percent
    """
    debug("Consolidating dataframes...", verbose)
    debug(f"Reference workloads: {len(reference_df)}", verbose)
    debug(f"Coscheduled workloads: {len(coscheduled_df)}", verbose)
    
    # Rename columns for clarity
    reference_df = reference_df.rename(columns={'ipc': 'runalone_ipc'})
    coscheduled_df = coscheduled_df.rename(columns={'ipc': 'regulated_ipc'})
    
    # Merge on workload name (inner join - only matching workloads)
    merged_df = pd.merge(
        reference_df,
        coscheduled_df,
        on='workload',
        how='inner'
    )
    
    debug(f"Merged workloads: {len(merged_df)}", verbose)
    
    if len(merged_df) == 0:
        error_exit("No matching workloads found between reference and coscheduled data")
    
    # Calculate slowdown ratio: runalone_ipc / regulated_ipc
    merged_df['slowdown_ratio'] = merged_df['runalone_ipc'] / merged_df['regulated_ipc']
    
    # Calculate degradation percent: ((runalone - regulated) / runalone) * 100
    merged_df['degradation_percent'] = (
        (merged_df['runalone_ipc'] - merged_df['regulated_ipc']) / merged_df['runalone_ipc']
    ) * 100
    
    # Rename workload column to workload_name
    merged_df = merged_df.rename(columns={'workload': 'workload_name'})
    
    # Reorder columns
    merged_df = merged_df[[
        'workload_name',
        'runalone_ipc',
        'regulated_ipc',
        'slowdown_ratio',
        'degradation_percent'
    ]]
    
    # Sort by workload name
    merged_df = merged_df.sort_values('workload_name').reset_index(drop=True)
    
    # Report missing workloads
    if verbose:
        reference_workloads = set(reference_df['workload'])
        coscheduled_workloads = set(coscheduled_df['workload'])
        
        missing_in_coscheduled = reference_workloads - coscheduled_workloads
        missing_in_reference = coscheduled_workloads - reference_workloads
        
        if missing_in_coscheduled:
            print(f"\n[WARNING] {len(missing_in_coscheduled)} workloads in reference but not in coscheduled:",
                  file=sys.stderr)
            for wl in sorted(list(missing_in_coscheduled)[:5]):
                print(f"  - {wl}", file=sys.stderr)
            if len(missing_in_coscheduled) > 5:
                print(f"  ... and {len(missing_in_coscheduled) - 5} more", file=sys.stderr)
        
        if missing_in_reference:
            print(f"\n[WARNING] {len(missing_in_reference)} workloads in coscheduled but not in reference:",
                  file=sys.stderr)
            for wl in sorted(list(missing_in_reference)[:5]):
                print(f"  - {wl}", file=sys.stderr)
            if len(missing_in_reference) > 5:
                print(f"  ... and {len(missing_in_reference) - 5} more", file=sys.stderr)
    
    if verbose:
        debug("\nSample consolidated data:", verbose)
        for idx, row in merged_df.head(3).iterrows():
            debug(f"  {row['workload_name']}: runalone={row['runalone_ipc']:.4f}, "
                  f"regulated={row['regulated_ipc']:.4f}, "
                  f"slowdown={row['slowdown_ratio']:.4f}x, "
                  f"degradation={row['degradation_percent']:.2f}%", verbose)
    
    return merged_df


#==============================================================================
# OUTPUT FUNCTIONS
#==============================================================================

def write_consolidated_csv(
    df: pd.DataFrame,
    output_file: str,
    verbose: bool = False
) -> None:
    """
    Write consolidated DataFrame to CSV file.
    
    Args:
        df: Consolidated DataFrame
        output_file: Output file path
        verbose: Enable debug output
    """
    if df.empty:
        if verbose:
            print("No results to write to CSV", file=sys.stderr)
        return
    
    debug(f"Writing {len(df)} results to {output_file}", verbose)
    
    try:
        with open(output_file, 'w') as f:
            # Write header with # prefix
            f.write("# workload_name, runalone_ipc, regulated_ipc, slowdown_ratio, degradation_percent\n")
            
            # Write data (without index, with proper formatting)
            df.to_csv(f, index=False, header=False, float_format='%.4f')
        
        if verbose:
            print(f"Results written to: {output_file}", file=sys.stderr)
    
    except IOError as e:
        error_exit(f"Failed to write output file {output_file}: {e}")


def print_summary_statistics(df: pd.DataFrame, verbose: bool = False) -> None:
    """
    Print summary statistics of the consolidated results.
    
    Args:
        df: Consolidated DataFrame
        verbose: Enable verbose output
    """
    if df.empty or not verbose:
        return
    
    print("\n" + "="*70, file=sys.stderr)
    print("SUMMARY STATISTICS", file=sys.stderr)
    print("="*70, file=sys.stderr)
    
    print(f"\nTotal workloads: {len(df)}", file=sys.stderr)
    
    # Degradation statistics
    print(f"\nDegradation (%):", file=sys.stderr)
    print(f"  Min:     {df['degradation_percent'].min():8.2f}%", file=sys.stderr)
    print(f"  Max:     {df['degradation_percent'].max():8.2f}%", file=sys.stderr)
    print(f"  Mean:    {df['degradation_percent'].mean():8.2f}%", file=sys.stderr)
    print(f"  Median:  {df['degradation_percent'].median():8.2f}%", file=sys.stderr)
    print(f"  Std Dev: {df['degradation_percent'].std():8.2f}%", file=sys.stderr)
    
    # Slowdown statistics
    # Filter out infinite values
    slowdown_finite = df[df['slowdown_ratio'] != float('inf')]['slowdown_ratio']
    
    if len(slowdown_finite) > 0:
        print(f"\nSlowdown Ratio (x):", file=sys.stderr)
        print(f"  Min:     {slowdown_finite.min():8.4f}x", file=sys.stderr)
        print(f"  Max:     {slowdown_finite.max():8.4f}x", file=sys.stderr)
        print(f"  Mean:    {slowdown_finite.mean():8.4f}x", file=sys.stderr)
        print(f"  Median:  {slowdown_finite.median():8.4f}x", file=sys.stderr)
        print(f"  Std Dev: {slowdown_finite.std():8.4f}x", file=sys.stderr)
    
    # Performance categories
    improved = len(df[df['degradation_percent'] < 0])
    unchanged = len(df[df['degradation_percent'] == 0])
    degraded = len(df[df['degradation_percent'] > 0])
    
    print(f"\nPerformance Impact:", file=sys.stderr)
    print(f"  Improved:   {improved:3d} ({100*improved/len(df):5.1f}%)", file=sys.stderr)
    print(f"  Unchanged:  {unchanged:3d} ({100*unchanged/len(df):5.1f}%)", file=sys.stderr)
    print(f"  Degraded:   {degraded:3d} ({100*degraded/len(df):5.1f}%)", file=sys.stderr)
    
    # Top 5 most degraded workloads
    print(f"\nTop 5 Most Degraded Workloads:", file=sys.stderr)
    top_degraded = df.nlargest(5, 'degradation_percent')
    for idx, row in top_degraded.iterrows():
        print(f"  {row['workload_name']:20s} {row['degradation_percent']:6.2f}% "
              f"(slowdown: {row['slowdown_ratio']:.3f}x)", file=sys.stderr)
    
    # Top 5 best performing workloads
    print(f"\nTop 5 Best Performing Workloads:", file=sys.stderr)
    top_performing = df.nsmallest(5, 'degradation_percent')
    for idx, row in top_performing.iterrows():
        print(f"  {row['workload_name']:20s} {row['degradation_percent']:6.2f}% "
              f"(slowdown: {row['slowdown_ratio']:.3f}x)", file=sys.stderr)
    
    print("="*70 + "\n", file=sys.stderr)


#==============================================================================
# MAIN
#==============================================================================

def main():
    """Main function to consolidate benchmark results."""
    
    # Parse command line arguments
    parser = argparse.ArgumentParser(
        description='Consolidate SPEC benchmark runalone and coscheduled results using pandas',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    %(prog)s reference.csv coscheduled.csv
    %(prog)s reference.csv coscheduled.csv -o output.csv
    %(prog)s reference.csv coscheduled.csv -v
    %(prog)s reference.csv coscheduled.csv -o results.csv -v

CSV Input Format:
    Both CSV files support 2-column or 4-column format (uses first 2 columns only):
    # fg_workload, fg_IPC[, bg_workload, bg_IPC]
    500.perlbench_r, 1.75
    502.gcc_r, 1.21
    ...

Output Format:
    # workload_name, runalone_ipc, regulated_ipc, slowdown_ratio, degradation_percent
        """
    )
    
    parser.add_argument(
        'reference_csv',
        help='CSV file with runalone (reference) workload names and IPC values'
    )
    
    parser.add_argument(
        'coscheduled_csv',
        help='CSV file with coscheduled workload names and IPC values'
    )
    
    parser.add_argument(
        '-o', '--output',
        default=DEFAULT_OUTPUT_FILE,
        help=f'Output CSV file name (default: {DEFAULT_OUTPUT_FILE})'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Enable verbose debug output'
    )
    
    args = parser.parse_args()
    
    # Validate input files
    debug("Validating input files...", args.verbose)
    validate_file(args.reference_csv, "Reference CSV", args.verbose)
    validate_file(args.coscheduled_csv, "Coscheduled CSV", args.verbose)
    
    # Load CSV files into DataFrames
    debug("Loading reference data...", args.verbose)
    reference_df = load_csv_to_dataframe(args.reference_csv, args.verbose)
    
    debug("Loading coscheduled data...", args.verbose)
    coscheduled_df = load_csv_to_dataframe(args.coscheduled_csv, args.verbose)
    
    # Check if we have data
    if reference_df.empty:
        error_exit(f"No valid data found in reference CSV: {args.reference_csv}")
    
    if coscheduled_df.empty:
        error_exit(f"No valid data found in coscheduled CSV: {args.coscheduled_csv}")
    
    # Consolidate DataFrames
    debug("Consolidating results...", args.verbose)
    consolidated_df = consolidate_dataframes(reference_df, coscheduled_df, args.verbose)
    
    # Write output
    debug(f"Writing output to {args.output}...", args.verbose)
    write_consolidated_csv(consolidated_df, args.output, args.verbose)
    
    # Print summary statistics
    print_summary_statistics(consolidated_df, args.verbose)
    
    # Success message
    if args.verbose:
        print(f"\n✓ Successfully consolidated {len(consolidated_df)} workloads", file=sys.stderr)
        print(f"✓ Output written to: {args.output}", file=sys.stderr)
    else:
        print(f"Consolidated {len(consolidated_df)} workloads → {args.output}")


if __name__ == "__main__":
    main()
