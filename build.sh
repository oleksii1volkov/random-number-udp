#!/bin/bash

BuildType="Debug"

if [ "$1" != "" ]; then
    BuildType="$1"
fi

OS=$(uname)
if [ "$OS" == "Linux" ]; then
    OSProfile="linux"
elif [ "$OS" == "Darwin" ]; then
    OSProfile="mac"
else
    echo "Unsupported OS"
    exit 1
fi

ProfilePath="./profiles/${OSProfile}/gcc-x86_64-${BuildType,,}"
conan install . --profile:build=$ProfilePath --profile:host=$ProfilePath --output-folder=build --build=missing

cd "./build"
cmake .. -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=$BuildType
cmake --build . --config $BuildType
