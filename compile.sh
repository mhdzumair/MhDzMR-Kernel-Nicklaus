#!/bin/sh
# Copyright (c) 2020 MhDzMR-Kernel <mhdzumair@gmail.com>

KERNEL_DIR=$PWD
ZIMAGE=$KERNEL_DIR/out/arch/arm/boot/zImage-dtb
XIMAGE=$KERNEL_DIR/outdir/AnyKernel3/zImage-dtb
OUT=$KERNEL_DIR/out
BUILD_START=$(date +"%s")
blue='\033[0;34m'
cyan='\033[0;36m'
yellow='\033[0;33m'
green='\033[0;92m'
red='\033[0;31m'
purple='\033[0;95m'
white='\033[0;97m'
nocol='\033[0m'

banner(){
echo  "$blue*****************************************************"
echo  "*****************************************************"
echo  "$purple Script create by MhDzuMAiR "
echo "$purple

  ███╗   ███╗██╗  ██╗██████╗ ███████╗███╗   ███╗██████╗     
  ████╗ ████║██║  ██║██╔══██╗╚══███╔╝████╗ ████║██╔══██╗    
  ██╔████╔██║███████║██║  ██║  ███╔╝ ██╔████╔██║██████╔╝    
  ██║╚██╔╝██║██╔══██║██║  ██║ ███╔╝  ██║╚██╔╝██║██╔══██╗    
  ██║ ╚═╝ ██║██║  ██║██████╔╝███████╗██║ ╚═╝ ██║██║  ██║    
  ╚═╝     ╚═╝╚═╝  ╚═╝╚═════╝ ╚══════╝╚═╝     ╚═╝╚═╝  ╚═╝    
"
echo  "$yellow ________________________________________________________ "
echo  "$blue*****************************************************"
echo  "*****************************************************$nocol"
}

check_dir() {
#make kernel compiling dir...
if ! [ -f $out ];
then
echo "$green create out dir fresh"
mkdir -p out
fi
}

export_things(){
#export toolchain , custom build_user , custom build_host , arch
export ARCH=arm
export ARCH_MTK_PLATFORM=mt6735
export CROSS_COMPILE=$KERNEL_DIR/gcc-linaro-7.5.0-2019.12-x86_64_arm-eabi/bin/arm-eabi-
export KBUILD_BUILD_USER="CheRRy"
export KBUILD_BUILD_HOST="JiLeBi"
#clean the build
make clean mrproper
#defconfig
make -C $PWD O=out ARCH=arm nicklaus_defconfig
}

compile_kernel ()
{
echo
echo
echo "$blue***********************************************"
echo "          Compiling MhDzMR™.anDroid Kernel...          "
echo "***********************************************$nocol"
echo ""
#start compile
make -j8 -C $PWD O=out ARCH=arm
if ! [ -f $ZIMAGE ];
then
echo "$red Kernel Compilation failed! Fix the errors! $nocol"
exit 1
fi
}

zipping ()
{
echo ""
echo ""
echo  "$cyan***********************************************"
echo "          Packing MhDzMR™ anDroid Kernel...          "
echo  "***********************************************$nocol"
echo ""
echo  "$yellow It's Time for COOK MhDzMR™.anDroid Kernel $nocol"
echo ""

echo "$yellow Checking if there is already zImage $nocol"
if [ -f $XIMAGE ];
then
rm $XIMAGE
echo "$red Deleting existing zImage"
fi

echo "$yellow Copying zImage-dtb to outdir/Anykernel3 $nocol"
cp out/arch/arm/boot/zImage-dtb outdir/AnyKernel3/

#using AnyKernel3 templete
cd outdir/AnyKernel3
make
sleep 0.6;
echo ""
echo ""
echo "" "Done Making Recovery Flashable Zip"
echo ""
echo ""
echo "Locate MhDzMR™.anDroid Kernel in the following path : "
echo "outdir/Anykernel3"
echo ""
echo  "$blue***********************************************"
echo "      MhDzMR™.anDroid Kernel "
echo  "***********************************************$nocol"
echo ""
BUILD_END=$(date +"%s")
echo " l.o.a.d.i.n.g..."
sleep 0.4;
echo "   please wait... Calculating the build period"
sleep 0.1;
echo ""
echo ""
echo ""
echo ""
DIFF=$(($BUILD_END - $BUILD_START))
echo "$yellow Build completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds.$n"
banner
sleep 5.0;
}

banner
check_dir
export_things
compile_kernel
zipping
