#! /bin/bash 

export MSR_WRITE_PERMISSION_PATH=/sys/module/msr/parameters/allow_writes
export MSR_REG_HW_PREFETECHER=0x1A4
export CPU_CORE_0=0
export CPU_CORE_1=1
export CPU_CORE_2=2
export CPU_CORE_3=3
export CPU_CORE_4=4
export CPU_CORE_5=5


#if SPEC Installation exist 
export SPEC=$SPEC

# Ensure to run as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root or use sudo"
  exit 1
fi


##############################################################################################################
execute_config() {
   start_msg=$1
   cmd=$2
   end_msg="DONE"
   
   echo "$start_msg..."   
   echo "sudo $cmd" | bash
   echo $end_msg
}

 stop_all_unwanted_services(){
   execute_config "Truning off Snapd..." "systemctl disable --now snapd.service snapd.socket"
   execute_config "Turn Off rootkit deamon " "systemctl stop rtkit-daemon"
   execute_config "Turn Off CUPS"  "systemctl stop cups"


}


##############################################################################################################

main_setup(){

#Turn on the permission for writing into msr registers.
execute_config "Turn on the permission for writing into msr" "echo on > $MSR_WRITE_PERMISSION_PATH"

#Disable  hardware prefetchers
execute_config "Disable  hardware prefetcher on all cores" "wrmsr -a $MSR_REG_HW_PREFETECHER 0x00000000000000F" 
#wmsr -a 0x1A0 bit 9 not able to write 

execute_config "Enable system wide and uncore event collection" "echo -1 >  /proc/sys/kernel/perf_event_paranoid"

execute_config "Disable Real-time throttling " "echo -1 | sudo tee /proc/sys/kernel/sched_rt_runtime_us"

execute_config "Disable CPU core 5" "echo 0 >  /sys/devices/system/cpu/cpu5/online"

   #Remove any shields if it exists 
   sudo cset shield --reset 

   sudo cset set -l 

   sudo cset set -c $CPU_CORE_0 -s system

   sudo cset set -c $CPU_CORE_1 -s C1
   sudo cset set -c $CPU_CORE_2 -s C2
   sudo cset set -c $CPU_CORE_3 -s C3
   sudo cset set -c $CPU_CORE_4 -s C4
   

   cset set -l 
   echo "Moving all possible tasks to 'System' CPUSET"

   sudo cset proc -m -k --force  -f root -t system
   sudo cset proc -m -k --force  -f C1 -t system
   sudo cset proc -m -k --force  -f C2 -t system
   sudo cset proc -m -k --force  -f C3 -t system
   sudo cset proc -m -k --force  -f C4 -t system
    
   cset set -l 

   sleep 1

   stop_all_unwanted_services


}


main_setup 
