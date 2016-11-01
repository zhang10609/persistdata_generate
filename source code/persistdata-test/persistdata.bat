@echo off

if not exist mkpdimg.exe (
	echo "mkpdimg.exe not exist!"
	pause
	exit
)

%SPARSE_TOOL% %1 %2 %3 %4 %5 %6 %7 %8 -o persistdata.img

fastboot flash persistdata persistdata.img