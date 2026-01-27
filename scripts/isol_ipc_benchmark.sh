#! /bin/bash 


#if SPEC Installation exist 
export SPEC=$SPEC
export CPU_CORE_FOREGROUND=1

export BASE_DATA_PATH=/home/ss22/Workspace/data/
export MEMG_PATH=without_memguard/

export benchmarks_all=(
"500.perlbench_r"
"502.gcc_r"
"503.bwaves_r"
"505.mcf_r"
"507.cactuBSSN_r"
"508.namd_r"
"510.parest_r"
"511.povray_r"
"519.lbm_r"
"520.omnetpp_r"
"521.wrf_r"
"523.xalancbmk_r"
"525.x264_r"
"526.blender_r"
"527.cam4_r"
"531.deepsjeng_r"
"538.imagick_r"
"541.leela_r"
"544.nab_r"
"548.exchange2_r"
"549.fotonik3d_r"
"554.roms_r"
"557.xz_r"
"600.perlbench_s"
"602.gcc_s"
"603.bwaves_s"
"605.mcf_s"
"607.cactuBSSN_s"
"619.lbm_s"
"620.omnetpp_s"
"621.wrf_s"
"623.xalancbmk_s"
"625.x264_s"
"627.cam4_s"
"628.pop2_s"
"631.deepsjeng_s"
"638.imagick_s"
"641.leela_s"
"644.nab_s"
"648.exchange2_s"
"649.fotonik3d_s"
"654.roms_s"
"657.xz_s"
)





export benchmarks_intrate=(
"500.perlbench_r"
"502.gcc_r"
"505.mcf_r"
"520.omnetpp_r"
"523.xalancbmk_r"
"525.x264_r"
"531.deepsjeng_r"
"541.leela_r"
"548.exchange2_r"
"557.xz_r"
)

export benchmarks_intspeed=(
"600.perlbench_s"
"602.gcc_s"
"605.mcf_s"
"620.omnetpp_s"
"623.xalancbmk_s"
"625.x264_s"
"631.deepsjeng_s"
"641.leela_s"
"648.exchange2_s"
"657.xz_s"
)

export benchmarks_fprate=(
"503.bwaves_r"
"507.cactuBSSN_r"	
"508.namd_r"
"510.parest_r"
"511.povray_r"
"519.lbm_r"
"521.wrf_r"
"526.blender_r"
"527.cam4_r"
"538.imagick_r"
"544.nab_r"
"549.fotonik3d_r"
"554.roms_r"
)

export benchmarks_fpspeed=(
"603.bwaves_s"
"607.cactuBSSN_s"	
"619.lbm_s"
"621.wrf_s"
"627.cam4_s"
"628.pop2_s"
"638.imagick_s"
"644.nab_s"
"649.fotonik3d_s"
"654.roms_s"
)


function cleanup() {

#	LOG_FILEPATH=`ls -tr $SPEC/result/CPU2017*log | tail -1`
#	echo "Copying $LOG_FILEPATH to $BASE_DATA_PATH$MEMG_PATH$bm/"
#	cp "$LOG_FILEPATH" "$BASE_DATA_PATH$MEMG_PATH$bm/"

	echo "Stopping Perf($PERF_LLC_PID)"
	kill -9 $PERF_LLC_PID
}


# function exit() {
# 	echo "CLEANING UP ..."
# 	cleanup
# 	echo "EXITING..."
# 	exit
# }

 execute_config() {
    start_msg=$1
    cmd=$2
    end_msg="DONE"
   
    echo "$start_msg..."   
    echo "sudo $cmd" | bash
    echo $end_msg
 }



 function preprocess_separate_load_store_misses(){
	if [ -n "$1" ]; then
    	FILEPATH=$1
    	grep "LLC-load-misses"	$FILEPATH > "$FILEPATH.lm"
    	grep "LLC-store-misses"	$FILEPATH > "$FILEPATH.sm"
 	fi
}

##################################################################################################

# function xxxxxxx() {
	
# 	export bm=$1
	
# 	dry_run=""
# 	if [ -n "$2" ]; then
#    	dry_run="--fakereport"
# 	fi

# 	RUN_DATA_PATH="$BASE_DATA_PATH$MEMG_PATH$bm"

# 	export dt=`date +"%Y-%m-%d-%H-%M-%S"`
	
# 	export INTERVAL_MSEC=1000


# 	execute_config "Creating $RUN_DATA_PATH" "mkdir -p $RUN_DATA_PATH ; sudo chmod 777 $RUN_DATA_PATH" 

# 	# monitor the LLC cache misses 
# 	export PERF_CSV_FILEPATH="$BASE_DATA_PATH$MEMG_PATH$bm/$bm-llc-$dry_run-miss-$dt.csv"
# 	echo "PERF Into $PERF_CSV_FILEPATH"
# 	perf stat -x , -e  LLC-load-misses,LLC-store-misses -C $CPU_CORE_ID  -I $INTERVAL_MSEC -o $PERF_CSV_FILEPATH &
# 	export PERF_PID=$!

# 	echo "Started PERF stat($PERF_PID)"

# 	# Run specific task inside the shield, option paramters are passed to COMMAND not to cset
# 	# cset shield -e  COMMAND --  option1 option2 option-n
# 	# runcpu options
# 	# --nobuild does not build the banchmark , previously built 
# 	# --copies=1 only one instance of the benchmark 
# 	# --noreportable  = no PDF report 
# 	# --iterations= no. of iteration 
# 	# --tune=base - only base test  
# 	# --output_format=none  - disable reports in deafult formats 
# 	# --fakereport  , runs only the per script without the benchmark 
	

	
# # Schedule an benchmark in isolation on core 5 and retriev the IPC using perf 
#  #cset shield -e perf -- stat -e instructions,cycles runcpu  --config=15March2024.cfg --size=test --nobuild --copies=1  --noreportable --iterations=3 --tune=base --output_format=none 500.perlbench_r
 
# 	cleanup

# 	preprocess_separate_load_store_misses "$PERF_CSV_FILEPATH"



# }
################# MAIN ##################################



######## Step 1 : Ensure the setup is ready #############
                                                                                                                                                                                                                                                                                                                  
#sudo bash -c isol_setup.sh 

########Step 2: ###########################################
function run_benchmark() {
	dry_run=""
	export BENCHMARK_NAME=$1
	export rundt=$2
	export dt=`date +"%Y-%m-%d-%H-%M-%S"`
	export ITERATIONS=10
	export INTERVAL_MSEC=1000
	export RUN_DATA_PATH="$BASE_DATA_PATH$MEMG_PATH$BENCHMARK_NAME/RUN-$rundt"


	RUN_EXE_PATH="$SPEC/benchspec/CPU/$BENCHMARK_NAME/run/run_base_test_all-suites-execution-times-m64.0000/"
	echo "RUNEXE_PATH = $RUN_EXE_PATH"

	execute_config "Creating $RUN_DATA_PATH" "mkdir -p $RUN_DATA_PATH ; sudo chmod 777 $RUN_DATA_PATH" 

	# monitor the LLC cache misses 
	export PERF_LLC_FILEPATH="$RUN_DATA_PATH/$BENCHMARK_NAME-llc$dry_run-$dt.csv"
	
	perf stat -x, -e  LLC-load-misses,LLC-store-misses -C $CPU_CORE_FOREGROUND -I $INTERVAL_MSEC -o $PERF_LLC_FILEPATH &
	export PERF_LLC_PID=$!
	echo "PERF Recording LLC Misses $PERF_LLC_FILEPATH PID ($PERF_LLC_PID) on Core ($CPU_CORE_FOREGROUND)"
    	sleep 1
	
	#sudo bash -c "cd $SPEC; source shrc; cd -; \
	cset proc -s C1  -e  specinvoke -- -i $ITERATIONS  -d $RUN_EXE_PATH &
	export BENCHMARK_PID=$!
	echo "BENCHMARK started on Core($CPU_CORE_FOREGROUND) PID ($BENCHMARK_PID)"

	export PERF_IPC_FILEPATH="$RUN_DATA_PATH/$BENCHMARK_NAME-IPC$dry_run-$dt.txt"
	echo "PERF Recording IPC in  $PERF_IPC_FILEPATH"
	perf stat -e  instructions,cycles -o $PERF_IPC_FILEPATH  -p $BENCHMARK_PID 
	
	# cleanup
	sleep 1
	echo "Stopping Perf($PERF_LLC_PID)"
	kill -9 $PERF_LLC_PID
 
	preprocess_separate_load_store_misses "$PERF_LLC_FILEPATH"

}



### MAIN #####
rundt=`date +"%Y-%m-%d-%H-%M-%S"`

##TODO: confirm  the CPUs are isolated  run "setup_4_core_env.sh" or "setup_2_core_env.sh" before executing this script

export benchmarks_bg=(
"519.lbm_r"
)

for benchmark in "${benchmarks_all[@]}";
do
 	echo "============================================$benchmark START============================================"
 	run_benchmark $benchmark $rundt

 	echo "============================================$benchmark ENDS ============================================"
done

##############################################################################################
