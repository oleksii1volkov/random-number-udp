#!/bin/bash

BuildType="Debug"

if [ "$1" != "" ]; then
    BuildType="$1"
fi

projectDirectory=$(pwd)

executablePath="$projectDirectory/build/$BuildType/udp_server"
configPath="$projectDirectory/config/server.json"
logPath="$projectDirectory/build/$BuildType/logs/server"

"$executablePath" --config-path "$configPath" --logs-path "$logPath"
