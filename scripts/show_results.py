#!/usr/bin/env python3

import os
import subprocess
import argparse
import re
import glob
import csv
import pandas as pd

# Configuration
BASE_DATA_PATH = "~/Workspace/data/"
MEMG_PATH = "without_regulation/"
WORKLOAD_CHARACTERISTICS_CSV = "workload_characteristics.csv"  # Path to classification CSV

# Benchmark arrays
BENCHMARKS_SINGLE = [
    "525.x264_r"
]

BENCHMARKS_ALL = [
    "500.perlbench_r", "502.gcc_r", "503.bwaves_r", "505.mcf_r",
    "507.cactuBSSN_r", "508.namd_r", "510.parest_r", "511.povray_r",
    "519.lbm_r", "520.omnetpp_r", "521.wrf_r", "523.xalancbmk_r",
    "525.x264_r", "526.blender_r", "527.cam4_r", "531.deepsjeng_r",
    "538.imagick_r", "541.leela_r", "544.nab_r", "548.exchange2_r",
    "549.fotonik3d_r", "554.roms_r", "557.xz_r", "600.perlbench_s",
    "602.gcc_s", "605.mcf_s", "603.bwaves_s", "607.cactuBSSN_s",
    "619.lbm_s", "620.omnetpp_s", "621.wrf_s", "623.xalancbmk_s",
    "625.x264_s", "627.cam4_s", "628.pop2_s", "631.deepsjeng_s",
    "638.imagick_s", "641.leela_s", "644.nab_s", "648.exchange2_s",
    "649.fotonik3d_s", "654.roms_s", "657.xz_s"
]

BENCHMARKS_INTRATE = [
    "500.perlbench_r", "502.gcc_r", "505.mcf_r", "520.omnetpp_r",
    "523.xalancbmk_r", "525.x264_r", "531.deepsjeng_r", "541.leela_r",
    "548.exchange2_r", "557.xz_r"
]

BENCHMARKS_INTSPEED = [
    "600.perlbench_s", "602.gcc_s", "605.mcf_s", "620.omnetpp_s",
    "623.xalancbmk_s", "625.x264_s", "631.deepsjeng_s", "641.leela_s",
    "648.exchange2_s", "657.xz_s"
]

BENCHMARKS_FPRATE = [
    "503.bwaves_r", "507.cactuBSSN_r", "508.namd_r", "510.parest_r",
    "511.povray_r", "519.lbm_r", "521.wrf_r", "526.blender_r",
    "527.cam4_r", "538.imagick_r", "544.nab_r", "549.fotonik3d_r",
    "554.roms_r"
]

BENCHMARKS_FPSPEED = [
    "603.bwaves_s", "607.cactuBSSN_s", "619.lbm_s", "621.wrf_s",
    "627.cam4_s", "628.pop2_s", "638.imagick_s", "644.nab_s",
    "649.fotonik3d_s", "654.roms_s"
]

def load_workload_classifications(csv_path, verbose=False):
    """
    Load workload classification data from CSV file.
    
    Args:
        csv_path: Path to the workload characteristics CSV file
        verbose: Enable debug output
    
    Returns:
        Dictionary mapping workload name to classification
    """
    classifications = {}
    
    try:
        df = pd.read_csv(csv_path, encoding='utf-8-sig')  # utf-8-sig handles BOM
        
        if verbose:
            print(f"[DEBUG] Loaded classification CSV: {csv_path}")
            print(f"[DEBUG] CSV columns: {df.columns.tolist()}")
            print(f"[DEBUG] CSV shape: {df.shape}")
        
        # Create dictionary mapping workload name to classification
        for idx, row in df.iterrows():
            workload = row['Workload Name'].strip()
            classification = row['Classification'].strip()
            classifications[workload] = classification
        
        if verbose:
            print(f"[DEBUG] Loaded {len(classifications)} workload classifications")
            print(f"[DEBUG] Sample classifications: {list(classifications.items())[:5]}")
        
        return classifications
    
    except FileNotFoundError:
        if verbose:
            print(f"[DEBUG] Classification file not found: {csv_path}")
        return {}
    except Exception as e:
        if verbose:
            print(f"[DEBUG] Error loading classifications: {e}")
        return {}


def get_rundt(base_path,fg_benchmark, supplied_rundt, verbose=False):
    """Get the run directory associated with supplied_rundt"""
    pattern = os.path.join(base_path, fg_benchmark,"*"+supplied_rundt)
    if verbose:
        print(f"[DEBUG] get_rundt pattern: {pattern}")
    dirs = glob.glob(pattern)
    return dirs[0] if dirs else None



def get_latest_rundt(base_path, benchmark):
    """Get the most recent run directory"""
    benchmark_path = os.path.join(base_path, benchmark)
    try:
        dirs = sorted(
            [d for d in os.listdir(benchmark_path) if os.path.isdir(os.path.join(benchmark_path, d))],
            key=lambda x: os.path.getmtime(os.path.join(benchmark_path, x)),
            reverse=True
        )
        return dirs[0] if dirs else None
    except (FileNotFoundError, IndexError):
        return None


def extract_ipc_from_file(filepath):
    """Extract IPC value and execution time from perf output file"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
        
        # Extract IPC
        ipc = None
        # Look for "insn per cycle" pattern
        match = re.search(r'insn per cycle\s+#\s+([\d.]+)', content)
        if match:
            ipc = match.group(1)
        else:
            # Alternative pattern
            match = re.search(r'([\d.]+)\s+insn per cycle', content)
            if match:
                ipc = match.group(1)
        
        # Extract execution time
        exec_time = None
        time_match = re.search(r'([\d.]+)\s+seconds time elapsed', content)
        if time_match:
            exec_time = time_match.group(1)
        
        return ipc, exec_time
    except FileNotFoundError:
        return None, None


def find_llc_file(base_path, benchmark, rundt, verbose=False):
    """
    Find the LLC statistics CSV file for a given benchmark and run directory.
    
    Args:
        base_path: Base data path
        benchmark: Benchmark name
        rundt: Run directory timestamp
        verbose: Enable debug output
    
    Returns:
        Path to LLC CSV file or None if not found
    """
    pattern = os.path.join(base_path, benchmark, rundt, "*llc*.csv")
    if verbose:
        print(f"[DEBUG] find_llc_file pattern: {pattern}")
    try:
        result = subprocess.check_output(
            f"ls -t {pattern} 2>/dev/null | head -n 1",
            shell=True,
            text=True
        ).strip()
        if result:
            return result
    except subprocess.CalledProcessError:
        return None


def extract_llc_stats_from_file(filepath, verbose=False):
    """
    Extract LLC statistics from CSV file.
    
    Args:
        filepath: Path to the LLC CSV file
        verbose: Enable debug output
    
    Returns:
        Tuple of (llc_load_misses, llc_store_misses, imc_reads, imc_writes) or (None, None, None, None)
    """
    try:
        import pandas as pd
        
        # Read CSV file
        cols = ['time', 'B', 'C', 'D', 'E', 'F', 'G', 'H']
        drop_cols = ['C', 'E', 'F', 'G', 'H']
        
        df = pd.read_csv(filepath, sep=',', comment='#', names=cols)
        
        if verbose:
            print(f"[DEBUG] Original DataFrame shape: {df.shape}")
            print(f"[DEBUG] Original DataFrame columns: {df.columns.tolist()}")
            print(f"[DEBUG] Original DataFrame head:\n{df.head()}")
        
        df.drop(columns=drop_cols, inplace=True)
        
        # Pivot the dataframe
        pivoted_df = df.pivot(index='time', columns='D', values='B')
        
        if verbose:
            print(f"[DEBUG] Pivoted DataFrame shape: {pivoted_df.shape}")
            print(f"[DEBUG] Pivoted DataFrame columns: {pivoted_df.columns.tolist()}")
            print(f"[DEBUG] Pivoted DataFrame head:\n{pivoted_df.head()}")
        
        # Calculate means
        llc_load_misses = pivoted_df['LLC-load-misses'].mean()
        llc_store_misses = pivoted_df['LLC-store-misses'].mean()
        imc_reads = pivoted_df['uncore_imc/data_reads/'].mean()
        imc_writes = pivoted_df['uncore_imc/data_writes/'].mean()
        
        if verbose:
            print(f"[DEBUG] LLC stats - load: {llc_load_misses:.2f}, store: {llc_store_misses:.2f}, "
                  f"reads: {imc_reads:.2f}, writes: {imc_writes:.2f}")
        
        return (f"{llc_load_misses:.2f}", f"{llc_store_misses:.2f}", 
                f"{imc_reads:.2f}", f"{imc_writes:.2f}")
        
    except Exception as e:
        if verbose:
            print(f"[DEBUG] Error extracting LLC stats: {e}")
        return None, None, None, None


def get_benchmark_llc_stats(base_path, benchmark, rundt, verbose=False):
    """
    Get LLC statistics for a specific benchmark and run directory.
    
    Args:
        base_path: Base data path
        benchmark: Benchmark name
        rundt: Run directory timestamp
        verbose: Enable debug output
    
    Returns:
        Tuple of (llc_load_misses, llc_store_misses, imc_reads, imc_writes) or (None, None, None, None)
    """
    llc_file = find_llc_file(base_path, benchmark, rundt, verbose)
    if verbose:
        print(f"[DEBUG] get_benchmark_llc_stats llc_file: {llc_file}")
    if not llc_file:
        return None, None, None, None

    return extract_llc_stats_from_file(llc_file, verbose)


def find_ipc_file(base_path, benchmark, rundt, verbose=False):
    """
    Find the most recent IPC file for a given benchmark and run directory.
    
    Args:
        base_path: Base data path
        benchmark: Benchmark name
        rundt: Run directory timestamp
        verbose: Enable debug output
    
    Returns:
        Path to IPC file or None if not found
    """
    pattern = os.path.join(base_path, benchmark, rundt, "*IPC*")
    if verbose:
        print(f"[DEBUG] find_ipc_file pattern: {pattern}")
    try:
        result = subprocess.check_output(
            f"ls -t {pattern} 2>/dev/null | head -n 1",
            shell=True,
            text=True
        ).strip()
        if result:
            return result




    except subprocess.CalledProcessError:
        return None


def get_benchmark_ipc(base_path, benchmark, rundt, verbose=False):
    """
    Get IPC value and execution time for a specific benchmark and run directory.
    
    Args:
        base_path: Base data path
        benchmark: Benchmark name
        rundt: Run directory timestamp
        verbose: Enable debug output
    
    Returns:
        Tuple of (IPC value as string, execution time as string) or (None, None) if not found
    """
    ipc_file = find_ipc_file(base_path, benchmark, rundt, verbose)
    if verbose:
        print(f"[DEBUG] get_benchmark_ipc ipc_file: {ipc_file}")
    if not ipc_file:
        return None, None

    return extract_ipc_from_file(ipc_file)


def get_background_rundt(foreground_rundt, foreground_benchmark):
    """
    Generate background run directory name from foreground run directory.
    
    Args:
        foreground_rundt: Foreground run directory timestamp
        foreground_benchmark: Foreground benchmark name
    
    Returns:
        Background run directory name
    """
    return foreground_rundt.replace("-", f"-{foreground_benchmark}-", 1)


def process_benchmark_pair(base_path, fg_benchmark, bg_benchmark, rundt=None, verbose=False):
    """
    Process a foreground/background benchmark pair and extract IPC values, execution times, and LLC stats.
    
    Args:
        base_path: Base data path
        fg_benchmark: Foreground benchmark name
        bg_benchmark: Background benchmark name
        rundt: Optional run directory timestamp (uses latest if None)
        verbose: Enable debug output
    
    Returns:
        Dictionary with benchmark results or None if processing failed
    """
    # Get run directory
    current_rundt = rundt
    
    if not current_rundt:
        current_rundt = get_latest_rundt(base_path, fg_benchmark)
        if not current_rundt:
            return None
    else:
        #only timestamp will be gven , retrieve the run directory name
        current_rundt=get_rundt(base_path,fg_benchmark,current_rundt, verbose)
    
    if verbose:
        print(f"[DEBUG] current_rundt: {current_rundt}")

    # Get foreground IPC and execution time
    fg_ipc, fg_exec_time = get_benchmark_ipc(base_path, fg_benchmark, current_rundt, verbose)
    if verbose:
        print(f"[DEBUG] fg_ipc: {fg_ipc}, fg_exec_time: {fg_exec_time}")
    if fg_ipc is None:
        return None
    
    # Get foreground LLC statistics
    fg_llc_load, fg_llc_store, fg_imc_reads, fg_imc_writes = get_benchmark_llc_stats(
        base_path, fg_benchmark, current_rundt, verbose)
    
    # Get background IPC and execution time
    # Extract just the directory name from current_rundt if it's a full path
    rundt_name = os.path.basename(current_rundt) if current_rundt else None
    bg_rundt = get_background_rundt(rundt_name, fg_benchmark)
    
    # Resolve the full path for background run directory
    bg_rundt_full = get_rundt(base_path, bg_benchmark, bg_rundt, verbose)
    
    if verbose:
        print(f"[DEBUG] bg_rundt: {bg_rundt}, bg_rundt_full: {bg_rundt_full}")
    
    bg_ipc = None
    bg_exec_time = None
    bg_llc_load = None
    bg_llc_store = None
    bg_imc_reads = None
    bg_imc_writes = None
    
    if bg_rundt_full:
        bg_ipc, bg_exec_time = get_benchmark_ipc(base_path, bg_benchmark, bg_rundt_full, verbose)
        if verbose:
            print(f"[DEBUG] bg_ipc: {bg_ipc}, bg_exec_time: {bg_exec_time}")
        
        # Get background LLC statistics
        bg_llc_load, bg_llc_store, bg_imc_reads, bg_imc_writes = get_benchmark_llc_stats(
            base_path, bg_benchmark, bg_rundt_full, verbose)
    
    # if bg_ipc is None:
    #     return None
    
    return {
        'fg_benchmark': fg_benchmark,
        'fg_ipc': fg_ipc,
        'fg_exec_time': fg_exec_time,
        'fg_llc_load_misses': fg_llc_load,
        'fg_llc_store_misses': fg_llc_store,
        'fg_imc_reads': fg_imc_reads,
        'fg_imc_writes': fg_imc_writes,
        'bg_benchmark': bg_benchmark,
        'bg_ipc': bg_ipc,
        'bg_exec_time': bg_exec_time,
        'bg_llc_load_misses': bg_llc_load,
        'bg_llc_store_misses': bg_llc_store,
        'bg_imc_reads': bg_imc_reads,
        'bg_imc_writes': bg_imc_writes,
        'rundt': current_rundt
    }


def print_results(results, verbose=False):
    """
    Print benchmark results in CSV format.
    
    Args:
        results:A List of dicionaries. Each Dictionary containing benchmark results
        verbose: Enable console output
    """
    if verbose:
        for result in results:
            if result:
                print(f"{result['fg_benchmark']}, {result['fg_ipc']}, {result['fg_exec_time']}, "
                      f"{result['fg_llc_load_misses']}, {result['fg_llc_store_misses']}, "
                      f"{result['fg_imc_reads']}, {result['fg_imc_writes']}, "
                      f"{result['bg_benchmark']}, {result['bg_ipc']}, {result['bg_exec_time']}, "
                      f"{result['bg_llc_load_misses']}, {result['bg_llc_store_misses']}, "
                      f"{result['bg_imc_reads']}, {result['bg_imc_writes']}")


def write_results_to_csv(results, output_file, verbose=False, include_classification=False):
    """
    Write benchmark results to a CSV file.
    
    Args:
        results: A List of dictionaries. Each Dictionary containing benchmark results
        output_file: Path to the output CSV file
        verbose: Enable status messages
        include_classification: Whether to include classification columns
    """
    if not results:
        if verbose:
            print("No results to write to CSV")
        return
    
    with open(output_file, 'w', newline='') as csvfile:
        # Write header with # prefix
        if include_classification:
            csvfile.write("# fg_workload, fg_classification, fg_IPC, fg_exec_time, fg_LLC_load_misses, fg_LLC_store_misses, "
                         "fg_imc_reads, fg_imc_writes, bg_workload, bg_classification, bg_IPC, bg_exec_time, "
                         "bg_LLC_load_misses, bg_LLC_store_misses, bg_imc_reads, bg_imc_writes\n")
        else:
            csvfile.write("# fg_workload, fg_IPC, fg_exec_time, fg_LLC_load_misses, fg_LLC_store_misses, "
                         "fg_imc_reads, fg_imc_writes, bg_workload, bg_IPC, bg_exec_time, "
                         "bg_LLC_load_misses, bg_LLC_store_misses, bg_imc_reads, bg_imc_writes\n")
        
        # Write data rows
        writer = csv.writer(csvfile)
        for result in results:
            if result:
                if include_classification:
                    writer.writerow([
                        result['fg_benchmark'],
                        result.get('fg_classification', 'Unknown'),
                        result['fg_ipc'],
                        result['fg_exec_time'],
                        result['fg_llc_load_misses'],
                        result['fg_llc_store_misses'],
                        result['fg_imc_reads'],
                        result['fg_imc_writes'],
                        result['bg_benchmark'],
                        result.get('bg_classification', 'Unknown'),
                        result['bg_ipc'],
                        result['bg_exec_time'],
                        result['bg_llc_load_misses'],
                        result['bg_llc_store_misses'],
                        result['bg_imc_reads'],
                        result['bg_imc_writes']
                    ])
                else:
                    writer.writerow([
                        result['fg_benchmark'],
                        result['fg_ipc'],
                        result['fg_exec_time'],
                        result['fg_llc_load_misses'],
                        result['fg_llc_store_misses'],
                        result['fg_imc_reads'],
                        result['fg_imc_writes'],
                        result['bg_benchmark'],
                        result['bg_ipc'],
                        result['bg_exec_time'],
                        result['bg_llc_load_misses'],
                        result['bg_llc_store_misses'],
                        result['bg_imc_reads'],
                        result['bg_imc_writes']
                    ])
    
    if verbose:
        print(f"Results written to: {output_file}")


def process_all_benchmarks(base_path, benchmark_list, bg_benchmark, classifications, rundt=None, verbose=False):
    """
    Process all benchmarks in the list and display results.
    
    Args:
        base_path: Base data path
        benchmark_list: List of benchmarks to process
        bg_benchmark: Background benchmark name
        classifications: Dictionary mapping workload names to classifications
        rundt: Optional run directory timestamp (uses latest if None)
        verbose: Enable debug output
    """
    results = []
    if verbose:
        print(f"[DEBUG] base_path: {base_path}, bg_benchmark: {bg_benchmark}, rundt: {rundt}")

    for fg_benchmark in benchmark_list:
        if fg_benchmark == bg_benchmark:
            if verbose:
                print(f"[DEBUG] Skipping {fg_benchmark} ..")
            continue
        
        result = process_benchmark_pair(base_path, fg_benchmark, bg_benchmark, rundt, verbose)
        
        # Add classification to result
        if result:
            result['fg_classification'] = classifications.get(fg_benchmark, 'Unknown')
            result['bg_classification'] = classifications.get(bg_benchmark, 'Unknown')
        
        results.append(result)
    
    return results


        


def main():
    """Main function to process and display results"""
    parser = argparse.ArgumentParser(description='Show SPEC benchmark results')
    parser.add_argument('-d', '--rundt', default="", help='Run directory timestamp')
    parser.add_argument('-p', '--path', default=os.path.expanduser(os.path.join(BASE_DATA_PATH, MEMG_PATH)),
                       help='Base data path')
    parser.add_argument('-o', '--output', default="benchmark_results.csv",
                       help='Output CSV file name (default: benchmark_results.csv)')
    parser.add_argument('-c', '--classification', default=None,
                       help='Path to workload classification CSV file (optional)')
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Enable verbose debug output')
    
    args = parser.parse_args()
    
    rundt = args.rundt if args.rundt else None
    base_data_path = args.path
    benchmark_bg = "519.lbm_r"
    output_file = args.output
    verbose = args.verbose
    classification_file = args.classification
    
    # Load workload classifications (optional)
    classifications = {}
    if classification_file:
        classifications = load_workload_classifications(classification_file, verbose)
        if not classifications and verbose:
            print(f"[WARNING] Could not load classifications from {classification_file}")
    
    # Process all benchmarks
    results = process_all_benchmarks(base_data_path, BENCHMARKS_ALL, benchmark_bg, classifications, rundt, verbose)
    if results:
        print_results(results, verbose)
        
        # Write results to CSV file
        write_results_to_csv(results, output_file, verbose, include_classification=bool(classifications))

    
    # Print configuration
    if verbose:
        print()
        print(f"Base data path: {base_data_path}")
        if classification_file:
            print(f"Classification file: {classification_file}")
        print(f"rundt: {results[0]['rundt'] if results and results[0] else 'None'}")


if __name__ == "__main__":
    main()
