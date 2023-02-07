#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=/home/geoffreyjensen/Desktop/ECEA5305/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc
CROSS_COMPILE_PATH=/home/geoffreyjensen/Desktop/ECEA5305/arm-gnu-toolchain-12.2.rel1-x86_64-aarch64-none-linux-gnu/bin

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

if [[ ":$PATH:" ==  *":${CROSS_COMPILE_PATH}:"* ]]
then
	echo "Cross compile location is already in your PATH"
else
	echo "Cross compile location is not in your PATH!!!"
	echo "Adding it now"
	export PATH=$PATH:${CROSS_COMPILE_PATH}	
	echo $PATH
fi

mkdir -p ${OUTDIR}

cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "Not able to find previously built Image file so building Kernel"
    #Deep clean Kernel Build tree
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    #Configure for virt arm device to be simulated in QEMU
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    #Build Kernel image for booting with QEMU
    make -j4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    #Build Kernel modules
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
    #Build Kernel Device Tree
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

echo "Copying compiled kernel Image file into ${OUTDIR}"
sudo cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image
echo "Creating the staging directory for the root filesystem: ${OUTDIR}/rootfs"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "${OUTDIR}/rootfs already exists. Deleting rootfs directory and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs
echo "Creating filesystem tree in ${OUTDIR}/rootfs"
mkdir -p ${OUTDIR}/rootfs/bin
mkdir -p ${OUTDIR}/rootfs/dev
mkdir -p ${OUTDIR}/rootfs/etc
mkdir -p ${OUTDIR}/rootfs/lib
mkdir -p ${OUTDIR}/rootfs/lib64
mkdir -p ${OUTDIR}/rootfs/home
mkdir -p ${OUTDIR}/rootfs/proc
mkdir -p ${OUTDIR}/rootfs/sys
mkdir -p ${OUTDIR}/rootfs/sbin
mkdir -p ${OUTDIR}/rootfs/tmp
mkdir -p ${OUTDIR}/rootfs/usr
mkdir -p ${OUTDIR}/rootfs/usr/bin
mkdir -p ${OUTDIR}/rootfs/usr/sbin
mkdir -p ${OUTDIR}/rootfs/var
mkdir -p ${OUTDIR}/rootfs/var/tmp

echo "Cloning and building default config for busybox"
cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
echo "Cross Compile Busybox with ${CROSS_COMPILE} and install in ${OUTDIR}/rootfs"
make ARCH=arm64 CONFIG_PREFIX=${OUTDIR}/rootfs CROSS_COMPILE=${CROSS_COMPILE} install

echo "Checking Library dependencies"
cd ${OUTDIR}/rootfs
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "Copying Library dependencies from ${SYSROOT} to ${OUTDIR}/rootfs/lib"
cp ${SYSROOT}/lib/ld-linux-aarch64.so.1 lib
cp ${SYSROOT}/lib64/libm.so.6 lib64
cp ${SYSROOT}/lib64/libresolv.so.2 lib64
cp ${SYSROOT}/lib64/libc.so.6 lib64

# TODO: Make device nodes
echo "Making Device Nodes"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
echo "Clean and build the writer utility from Assignment 1 part 2"
cd /home/geoffreyjensen/Desktop/ECEA5305/Assignment1/assignment-1-geoffreyjensen6/finder-app
make clean
make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "Copy writer, finder.sh, finder-test.sh, autorun-qemu.sh, and the conf directory to ${OUTDIR}/rootfs"
cp writer $OUTDIR/rootfs/home/writer
cp finder.sh $OUTDIR/rootfs/home/finder.sh
cp finder-test.sh $OUTDIR/rootfs/home/finder-test.sh
cp -r ../conf/ $OUTDIR/rootfs/home/conf/
cp autorun-qemu.sh $OUTDIR/rootfs/home/autorun-qemu.sh

# TODO: Chown the root directory
echo "Change Ownership of ${OUTDIR}/rootfs to root:root"
cd $OUTDIR/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
echo "Create initramfs.cpio.gz"
if [ -e ${OUTDIR}/initramfs.cpio.gz ]
then
	sudo rm -r $OUTDIR/initramfs.cpio.gz
fi
find . -print0 | cpio --null -ov --format=newc | gzip -9 > $OUTDIR/initramfs.cpio.gz
sudo chown -R root:root $OUTDIR/initramfs.cpio.gz
