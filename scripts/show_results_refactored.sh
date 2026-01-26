#!/usr/bin/env bash

#==============================================================================
# show_results.sh - SPEC CPU2017 Benchmark Results Processor
#==============================================================================
# Description:
#   Processes SPEC CPU2017 benchmark results, extracting IPC values for both
#   foreground and background workloads and outputting to CSV format.
#
# Usage:
#   ./show_results.sh [-d RUNDT] [-p BASE_PATH] [-o OUTPUT_FILE] [-v] [-h]
#
# Options:
#   -d RUNDT        Run directory timestamp
#   -p BASE_PATH    Base data path (default: ~/Workspace/data/without_regulation/)
#   -o OUTPUT_FILE  Output CSV file (default: benchmark_results.csv)
#   -v              Enable verbose debug output
#   -h              Show help message
#
# Author: Converted from original show_results.sh
# Date: 2026-01-26
#==============================================================================

set -euo pipefail  # Exit on error, undefined variables, and pipe failures

#==============================================================================
# CONFIGURATION
#==============================================================================

# Default paths
readonly BASE_DATA_PATH="${HOME}/Workspace/data"
readonly MEMG_PATH="without_regulation"
readonly DEFAULT_OUTPUT_FILE="benchmark_results.csv"

# Background benchmark
readonly BENCHMARK_BG="519.lbm_r"

# Benchmark arrays
readonly BENCHMARKS_ALL=(
    "500.perlbench_r" "502.gcc_r" "503.bwaves_r" "505.mcf_r"
    "507.cactuBSSN_r" "508.namd_r" "510.parest_r" "511.povray_r"
    "519.lbm_r" "520.omnetpp_r" "521.wrf_r" "523.xalancbmk_r"
    "525.x264_r" "526.blender_r" "527.cam4_r" "531.deepsjeng_r"
    "538.imagick_r" "541.leela_r" "544.nab_r" "548.exchange2_r"
    "549.fotonik3d_r" "554.roms_r" "557.xz_r" "600.perlbench_s"
    "602.gcc_s" "605.mcf_s" "603.bwaves_s" "607.cactuBSSN_s"
    "619.lbm_s" "620.omnetpp_s" "621.wrf_s" "623.xalancbmk_s"
    "625.x264_s" "627.cam4_s" "628.pop2_s" "631.deepsjeng_s"
    "638.imagick_s" "641.leela_s" "644.nab_s" "648.exchange2_s"
    "649.fotonik3d_s" "654.roms_s" "657.xz_s"
)

readonly BENCHMARKS_INTRATE=(
    "500.perlbench_r" "502.gcc_r" "505.mcf_r" "520.omnetpp_r"
    "523.xalancbmk_r" "525.x264_r" "531.deepsjeng_r" "541.leela_r"
    "548.exchange2_r" "557.xz_r"
)

readonly BENCHMARKS_INTSPEED=(
    "600.perlbench_s" "602.gcc_s" "605.mcf_s" "620.omnetpp_s"
    "623.xalancbmk_s" "625.x264_s" "631.deepsjeng_s" "641.leela_s"
    "648.exchange2_s" "657.xz_s"
)

readonly BENCHMARKS_FPRATE=(
    "503.bwaves_r" "507.cactuBSSN_r" "508.namd_r" "510.parest_r"
    "511.povray_r" "519.lbm_r" "521.wrf_r" "526.blender_r"
    "527.cam4_r" "538.imagick_r" "544.nab_r" "549.fotonik3d_r"
    "554.roms_r"
)

readonly BENCHMARKS_FPSPEED=(
    "603.bwaves_s" "607.cactuBSSN_s" "619.lbm_s" "621.wrf_s"
    "627.cam4_s" "628.pop2_s" "638.imagick_s" "644.nab_s"
    "649.fotonik3d_s" "654.roms_s"
)

#==============================================================================
# GLOBAL VARIABLES
#==============================================================================

# Command line arguments
RUNDT=""
BASE_PATH="${BASE_DATA_PATH}/${MEMG_PATH}"
OUTPUT_FILE="${DEFAULT_OUTPUT_FILE}"
VERBOSE=0

# Results storage
declare -a RESULTS_FG_BENCHMARK=()
declare -a RESULTS_FG_IPC=()
declare -a RESULTS_BG_BENCHMARK=()
declare -a RESULTS_BG_IPC=()

#==============================================================================
# UTILITY FUNCTIONS
#==============================================================================

##
# Print debug message if verbose mode is enabled
# Arguments:
#   $@ - Message to print
##
debug() {
    if [[ ${VERBOSE} -eq 1 ]]; then
        echo "[DEBUG] $*" >&2
    fi
}

##
# Print error message and exit
# Arguments:
#   $1 - Error message
#   $2 - Exit code (default: 1)
##
error_exit() {
    echo "ERROR: $1" >&2
    exit "${2:-1}"
}

##
# Print usage information
##
usage() {
    cat << EOF
Usage: ${0##*/} [-d RUNDT] [-p BASE_PATH] [-o OUTPUT_FILE] [-v] [-h]

Process SPEC CPU2017 benchmark results and output to CSV.

Options:
    -d RUNDT        Run directory timestamp
    -p BASE_PATH    Base data path (default: ~/Workspace/data/without_regulation/)
    -o OUTPUT_FILE  Output CSV file (default: benchmark_results.csv)
    -v              Enable verbose debug output
    -h              Show this help message

Examples:
    ${0##*/}
    ${0##*/} -v
    ${0##*/} -d "2024-01-25" -o results.csv
    ${0##*/} -p "/custom/path" -v

EOF
}

#==============================================================================
# CORE FUNCTIONS
#==============================================================================

##
# Get the most recent run directory for a benchmark
# Arguments:
#   $1 - Base path
#   $2 - Benchmark name
# Returns:
#   Prints the most recent run directory name, or empty string if none found
##
get_latest_rundt() {
    local base_path="$1"
    local benchmark="$2"
    local benchmark_path="${base_path}/${benchmark}"
    
    if [[ ! -d "${benchmark_path}" ]]; then
        debug "Benchmark path not found: ${benchmark_path}"
        return 1
    fi
    
    # Find most recent directory by modification time
    local latest_dir
    latest_dir=$(ls -t "${benchmark_path}" 2>/dev/null | head -n 1)
    
    if [[ -n "${latest_dir}" ]]; then
        echo "${latest_dir}"
        return 0
    fi
    
    return 1
}

##
# Get run directory matching a partial timestamp
# Arguments:
#   $1 - Base path
#   $2 - Benchmark name
#   $3 - Partial timestamp to match
# Returns:
#   Prints the full run directory name, or empty string if not found
##
get_rundt() {
    local base_path="$1"
    local benchmark="$2"
    local partial_timestamp="$3"
    local benchmark_path="${base_path}/${benchmark}"
    
    debug "get_rundt: Looking for *${partial_timestamp} in ${benchmark_path}"
    
    # Find directory matching pattern
    local matched_dir
    matched_dir=$(ls -d "${benchmark_path}"/*"${partial_timestamp}" 2>/dev/null | head -n 1)
    
    if [[ -n "${matched_dir}" ]]; then
        basename "${matched_dir}"
        return 0
    fi
    
    return 1
}

##
# Extract IPC value from a perf output file
# Arguments:
#   $1 - Path to IPC file
# Returns:
#   Prints the IPC value, or "None" if not found
##
extract_ipc_from_file() {
    local filepath="$1"
    
    if [[ ! -f "${filepath}" ]]; then
        echo "None"
        return 1
    fi
    
    # Try pattern 1: "insn per cycle # X.XX"
    local ipc
    ipc=$(grep "insn per cycle" "${filepath}" 2>/dev/null | awk '{print $4}' | head -n 1)
    
    if [[ -n "${ipc}" ]]; then
        echo "${ipc}"
        return 0
    fi
    
    # Try pattern 2: "X.XX insn per cycle"
    ipc=$(grep "insn per cycle" "${filepath}" 2>/dev/null | awk '{print $1}' | head -n 1)
    
    if [[ -n "${ipc}" ]]; then
        echo "${ipc}"
        return 0
    fi
    
    echo "None"
    return 1
}

##
# Find the most recent IPC file for a benchmark run
# Arguments:
#   $1 - Base path
#   $2 - Benchmark name
#   $3 - Run directory name
# Returns:
#   Prints the path to IPC file, or empty string if not found
##
find_ipc_file() {
    local base_path="$1"
    local benchmark="$2"
    local rundt="$3"
    local pattern="${base_path}/${benchmark}/${rundt}/*IPC*"
    
    debug "find_ipc_file: Pattern ${pattern}"
    
    local ipc_file
    ipc_file=$(ls -t ${pattern} 2>/dev/null | head -n 1)
    
    if [[ -n "${ipc_file}" ]]; then
        echo "${ipc_file}"
        return 0
    fi
    
    return 1
}

##
# Get IPC value for a specific benchmark and run directory
# Arguments:
#   $1 - Base path
#   $2 - Benchmark name
#   $3 - Run directory name
# Returns:
#   Prints the IPC value, or "None" if not found
##
get_benchmark_ipc() {
    local base_path="$1"
    local benchmark="$2"
    local rundt="$3"
    
    local ipc_file
    ipc_file=$(find_ipc_file "${base_path}" "${benchmark}" "${rundt}")
    
    debug "get_benchmark_ipc: IPC file ${ipc_file}"
    
    if [[ -z "${ipc_file}" ]]; then
        echo "None"
        return 1
    fi
    
    extract_ipc_from_file "${ipc_file}"
}

##
# Generate background run directory name from foreground run directory
# Arguments:
#   $1 - Foreground run directory name
#   $2 - Foreground benchmark name
# Returns:
#   Prints the background run directory name
##
get_background_rundt() {
    local fg_rundt="$1"
    local fg_benchmark="$2"
    
    # Replace first hyphen with "-{benchmark}-"
    echo "${fg_rundt/-/-${fg_benchmark}-}"
}

##
# Process a single foreground/background benchmark pair
# Arguments:
#   $1 - Base path
#   $2 - Foreground benchmark name
#   $3 - Background benchmark name
#   $4 - Run directory timestamp (optional)
# Returns:
#   0 on success, 1 on failure
#   Appends results to global arrays
##
process_benchmark_pair() {
    local base_path="$1"
    local fg_benchmark="$2"
    local bg_benchmark="$3"
    local rundt="$4"
    
    # Get run directory
    local current_rundt
    if [[ -z "${rundt}" ]]; then
        current_rundt=$(get_latest_rundt "${base_path}" "${fg_benchmark}")
        if [[ -z "${current_rundt}" ]]; then
            debug "No run directory found for ${fg_benchmark}"
            return 1
        fi
    else
        current_rundt=$(get_rundt "${base_path}" "${fg_benchmark}" "${rundt}")
        if [[ -z "${current_rundt}" ]]; then
            debug "Run directory not found for ${fg_benchmark} with timestamp ${rundt}"
            return 1
        fi
    fi
    
    debug "current_rundt: ${current_rundt}"
    
    # Get foreground IPC
    local fg_ipc
    fg_ipc=$(get_benchmark_ipc "${base_path}" "${fg_benchmark}" "${current_rundt}")
    
    debug "fg_ipc: ${fg_ipc}"
    
    if [[ "${fg_ipc}" == "None" ]]; then
        debug "Foreground IPC not found for ${fg_benchmark}"
        return 1
    fi
    
    # Get background IPC
    local rundt_name
    rundt_name=$(basename "${current_rundt}")
    local bg_rundt
    bg_rundt=$(get_background_rundt "${rundt_name}" "${fg_benchmark}")
    
    # Resolve full path for background
    local bg_rundt_full
    bg_rundt_full=$(get_rundt "${base_path}" "${bg_benchmark}" "${bg_rundt}")
    
    debug "bg_rundt: ${bg_rundt}, bg_rundt_full: ${bg_rundt_full}"
    
    local bg_ipc="None"
    if [[ -n "${bg_rundt_full}" ]]; then
        bg_ipc=$(get_benchmark_ipc "${base_path}" "${bg_benchmark}" "${bg_rundt_full}")
        debug "bg_ipc: ${bg_ipc}"
    fi
    
    # Store results in global arrays
    RESULTS_FG_BENCHMARK+=("${fg_benchmark}")
    RESULTS_FG_IPC+=("${fg_ipc}")
    RESULTS_BG_BENCHMARK+=("${bg_benchmark}")
    RESULTS_BG_IPC+=("${bg_ipc}")
    
    return 0
}

##
# Process all benchmarks in the list
# Arguments:
#   $1 - Base path
#   $2 - Background benchmark name
#   $3 - Run directory timestamp (optional)
##
process_all_benchmarks() {
    local base_path="$1"
    local bg_benchmark="$2"
    local rundt="$3"
    
    debug "base_path: ${base_path}, bg_benchmark: ${bg_benchmark}, rundt: ${rundt}"
    
    for fg_benchmark in "${BENCHMARKS_ALL[@]}"; do
        if [[ "${fg_benchmark}" == "${bg_benchmark}" ]]; then
            debug "Skipping ${fg_benchmark} .."
            continue
        fi
        
        process_benchmark_pair "${base_path}" "${fg_benchmark}" "${bg_benchmark}" "${rundt}"
    done
}

##
# Print results to console
##
print_results() {
    if [[ ${VERBOSE} -eq 1 ]]; then
        local num_results=${#RESULTS_FG_BENCHMARK[@]}
        for ((i=0; i<num_results; i++)); do
            echo "${RESULTS_FG_BENCHMARK[i]}, ${RESULTS_FG_IPC[i]}, ${RESULTS_BG_BENCHMARK[i]}, ${RESULTS_BG_IPC[i]}"
        done
    fi
}

##
# Write results to CSV file
# Arguments:
#   $1 - Output file path
##
write_results_to_csv() {
    local output_file="$1"
    
    # Check if we have results
    if [[ ${#RESULTS_FG_BENCHMARK[@]} -eq 0 ]]; then
        if [[ ${VERBOSE} -eq 1 ]]; then
            echo "No results to write to CSV"
        fi
        return 1
    fi
    
    # Write header
    echo "# fg_workload, fg_IPC, bg_workload, bg_IPC" > "${output_file}"
    
    # Write data rows
    local num_results=${#RESULTS_FG_BENCHMARK[@]}
    for ((i=0; i<num_results; i++)); do
        echo "${RESULTS_FG_BENCHMARK[i]},${RESULTS_FG_IPC[i]},${RESULTS_BG_BENCHMARK[i]},${RESULTS_BG_IPC[i]}" >> "${output_file}"
    done
    
    if [[ ${VERBOSE} -eq 1 ]]; then
        echo "Results written to: ${output_file}"
    fi
}

#==============================================================================
# MAIN
#==============================================================================

##
# Main function
##
main() {
    # Parse command line arguments
    while getopts "d:p:o:vh" opt; do
        case ${opt} in
            d)
                RUNDT="${OPTARG}"
                ;;
            p)
                BASE_PATH="${OPTARG}"
                ;;
            o)
                OUTPUT_FILE="${OPTARG}"
                ;;
            v)
                VERBOSE=1
                ;;
            h)
                usage
                exit 0
                ;;
            \?)
                usage
                exit 1
                ;;
        esac
    done
    
    # Expand tilde in BASE_PATH
    BASE_PATH="${BASE_PATH/#\~/$HOME}"
    
    # Process all benchmarks
    process_all_benchmarks "${BASE_PATH}" "${BENCHMARK_BG}" "${RUNDT}"
    
    # Print results to console (if verbose)
    print_results
    
    # Write results to CSV
    write_results_to_csv "${OUTPUT_FILE}"
    
    # Print configuration (if verbose)
    if [[ ${VERBOSE} -eq 1 ]]; then
        echo ""
        echo "Base data path: ${BASE_PATH}"
        if [[ ${#RESULTS_FG_BENCHMARK[@]} -gt 0 ]]; then
            echo "rundt: ${RUNDT:-latest}"
        fi
    fi
}

# Run main function
main "$@"
