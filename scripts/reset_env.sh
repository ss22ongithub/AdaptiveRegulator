#! /bin/bash 
#To be run as root

if [ "$EUID" -ne 0 ]; then
  echo "Please run as root"
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


sudo cset set -l

for set_name in $(sudo cset set -l | awk 'NR > 3 && $1 != "root" {print $1}'); do
    echo "Destroying cpuset: $set_name"
    sudo cset set -d "$set_name"
done

execute_config "Enable CPU core 1"  "echo 1 >  /sys/devices/system/cpu/cpu1/online"
execute_config "Enable CPU core 2"  "echo 1 >  /sys/devices/system/cpu/cpu2/online"
execute_config "Enable CPU core 3"  "echo 1 >  /sys/devices/system/cpu/cpu3/online"
execute_config "Enable CPU core 4"  "echo 1 >  /sys/devices/system/cpu/cpu4/online"
execute_config "Enable CPU core 5"  "echo 1 >  /sys/devices/system/cpu/cpu5/online"



sudo cset set -l





