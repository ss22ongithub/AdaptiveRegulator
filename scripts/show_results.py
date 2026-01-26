#!/usr/bin/env python3

import os
import subprocess
import argparse
import re
import glob
import csv

# Configuration
BASE_DATA_PATH = "~/Workspace/data/"
MEMG_PATH = "without_regulation/"

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
    """Extract IPC value from perf output file"""
    try:
        with open(filepath, 'r') as f:
            content = f.read()
        
        # Look for "insn per cycle" pattern
        match = re.search(r'insn per cycle\s+#\s+([\d.]+)', content)
        if match:
            return match.group(1)
        
        # Alternative pattern
        match = re.search(r'([\d.]+)\s+insn per cycle', content)
        if match:
            return match.group(1)
        
        return None
    except FileNotFoundError:
        return None


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
    Get IPC value for a specific benchmark and run directory.
    
    Args:
        base_path: Base data path
        benchmark: Benchmark name
        rundt: Run directory timestamp
        verbose: Enable debug output
    
    Returns:
        IPC value as string or None if not found
    """
    ipc_file = find_ipc_file(base_path, benchmark, rundt, verbose)
    if verbose:
        print(f"[DEBUG] get_benchmark_ipc ipc_file: {ipc_file}")
    if not ipc_file:
        return None

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
    Process a foreground/background benchmark pair and extract IPC values.
    
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

    # Get foreground IPC
    fg_ipc = get_benchmark_ipc(base_path, fg_benchmark, current_rundt, verbose)
    if verbose:
        print(f"[DEBUG] fg_ipc: {fg_ipc}")
    if fg_ipc is None:
        return None
    
    # Get background IPC
    # Extract just the directory name from current_rundt if it's a full path
    rundt_name = os.path.basename(current_rundt) if current_rundt else None
    bg_rundt = get_background_rundt(rundt_name, fg_benchmark)
    
    # Resolve the full path for background run directory
    bg_rundt_full = get_rundt(base_path, bg_benchmark, bg_rundt, verbose)
    
    if verbose:
        print(f"[DEBUG] bg_rundt: {bg_rundt}, bg_rundt_full: {bg_rundt_full}")
    
    bg_ipc = None
    if bg_rundt_full:
        bg_ipc = get_benchmark_ipc(base_path, bg_benchmark, bg_rundt_full, verbose)
        if verbose:
            print(f"[DEBUG] bg_ipc: {bg_ipc}")
    
    # if bg_ipc is None:
    #     return None
    
    return {
        'fg_benchmark': fg_benchmark,
        'fg_ipc': fg_ipc,
        'bg_benchmark': bg_benchmark,
        'bg_ipc': bg_ipc,
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
                print(f"{result['fg_benchmark']}, {result['fg_ipc']}, "
                  f"{result['bg_benchmark']}, {result['bg_ipc']}")


def write_results_to_csv(results, output_file, verbose=False):
    """
    Write benchmark results to a CSV file.
    
    Args:
        results: A List of dictionaries. Each Dictionary containing benchmark results
        output_file: Path to the output CSV file
        verbose: Enable status messages
    """
    if not results:
        if verbose:
            print("No results to write to CSV")
        return
    
    with open(output_file, 'w', newline='') as csvfile:
        # Write header with # prefix
        csvfile.write("# fg_workload, fg_IPC, bg_workload, bg_IPC\n")
        
        # Write data rows
        writer = csv.writer(csvfile)
        for result in results:
            if result:
                writer.writerow([
                    result['fg_benchmark'],
                    result['fg_ipc'],
                    result['bg_benchmark'],
                    result['bg_ipc']
                ])
    
    if verbose:
        print(f"Results written to: {output_file}")


def process_all_benchmarks(base_path, benchmark_list, bg_benchmark, rundt=None, verbose=False):
    """
    Process all benchmarks in the list and display results.
    
    Args:
        base_path: Base data path
        benchmark_list: List of benchmarks to process
        bg_benchmark: Background benchmark name
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
        
        result  =  process_benchmark_pair(base_path, fg_benchmark, bg_benchmark, rundt, verbose)
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
    parser.add_argument('-v', '--verbose', action='store_true',
                       help='Enable verbose debug output')
    
    args = parser.parse_args()
    
    rundt = args.rundt if args.rundt else None
    base_data_path = args.path
    benchmark_bg = "519.lbm_r"
    output_file = args.output
    verbose = args.verbose
    
    # Process all benchmarks
    results = process_all_benchmarks(base_data_path, BENCHMARKS_ALL, benchmark_bg, rundt, verbose)
    if results:
        print_results(results, verbose)
        
        # Write results to CSV file
        write_results_to_csv(results, output_file, verbose)

    
    # Print configuration
    if verbose:
        print()
        print(f"Base data path: {base_data_path}")
        print(f"rundt: {results[0]['rundt'] if results and results[0] else 'None'}")


if __name__ == "__main__":
    main()
