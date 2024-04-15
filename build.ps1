param(
    [string]$BuildType = "Debug"
)

& conan install . --profile=.\profiles\msvc-x86_64-$($BuildType.ToLower()) --output-folder=build --build=missing
cd ".\build"
cmake .. -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=$BuildType
cmake --build . --config $BuildType
