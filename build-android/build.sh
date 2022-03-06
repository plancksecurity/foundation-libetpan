#!/bin/bash

build_version=2
ANDROID_PLATFORM=android-21
package_name=libetpan-android
arch=$1

current_dir="`pwd`"

if test "x$ANDROID_NDK" = x ; then
  echo should set ANDROID_NDK before running this script.
  exit 1
fi

function build {
  rm -rf "$current_dir/obj"
  echo "$ANDROID_NDK/ndk-build TARGET_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI"
  
  cd "$current_dir/jni"
  $ANDROID_NDK/ndk-build TARGET_PLATFORM=$ANDROID_PLATFORM TARGET_ARCH_ABI=$TARGET_ARCH_ABI \
    ICONV_PATH=$ICONV_PREFIX

# Copy lib to arch/lib
  mkdir -p "$current_dir/$package_name-$build_version/$TARGET_ARCH_ABI/lib"
  cp "$current_dir/obj/local/$TARGET_ARCH_ABI/libetpan.a" "$current_dir/$package_name-$build_version/$TARGET_ARCH_ABI/lib"
  rm -rf "$current_dir/obj"
}

cd "$current_dir/.."
# Source: https://gitea.pep.foundation/pEp.foundation/libetpan/src/commit/5e697c9cfec1a9746bf01226e70c3088b382f117/build-mac/run-autogen.sh
echo "preparing"
./autogen.sh --without-openssl --without-gnutls --without-sasl --without-curl --without-expat --without-zlib --disable-dependency-tracking 
./configure
make stamp-prepare

# Copy public headers to include
cp -RL include/libetpan "$current_dir/include"
mkdir -p "$current_dir/$package_name-$build_version/$TARGET_ARCH_ABI/include"
cp -RL include/libetpan "$current_dir/$package_name-$build_version/$TARGET_ARCH_ABI/include"

echo "Building for arch = $arch"
TARGET_ARCH_ABI=$arch

build
