#!/bin/bash

BuildType="Debug"

if [ "$1" != "" ]; then
    BuildType="$1"
fi

projectDirectory=$(pwd)

executablePath="$projectDirectory/build/$BuildType/udp_client"
configPath="$projectDirectory/config/client.json"
logPath="$projectDirectory/build/$BuildType/logs/client"
numbersPath="$projectDirectory/build/$BuildType/numbers.bin"

"$executablePath" --config-path "$configPath" --logs-path "$logPath" --numbers-path "$numbersPath"
