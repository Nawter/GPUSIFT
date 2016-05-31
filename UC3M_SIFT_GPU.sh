#!/bin/bash
# UNIX timestamp concatenated with nanoseconds
T="$(date +%s%N)" 
imgDir=$(pwd)/Rendimiento/Imagenes
execDir=$(pwd)/GPU/SiftGPU/bin/
makeDir=$(pwd)/GPU/SiftGPU/
mainDir=$(pwd)
find . -name "*.sift"  -exec rm -rf {} \;
cd $makeDir
echo "***********************make clean****************************"
make clean
echo "***********************make ****************************"
make
for imgDirs in "$imgDir"/*; do
# distinguir entre dirs y files
    if [ -d "${imgDirs}" ] ; 
		then 
		    echo "*****************Run*******************************************"
  		    echo "The folder is ::::" $imgDirs 
			cd $execDir
			./SimpleSIFT_Test4	$imgDirs
			# Time interval in nanoseconds
			T="$(($(date +%s%N)-T))"
			# Seconds
			S="$((T/1000000000))"
			# Milliseconds
			M="$((T/1000000))"
			printf "The total time is : %02d:%02d:%02d:%02d.%03d\n" "$((S/86400))" "$((S/3600%24))" "$((S/60%60))" "$((S%60))" "${M}"
			T="$(date +%s%N)"			
            cd $mainDir         
	fi	
done
