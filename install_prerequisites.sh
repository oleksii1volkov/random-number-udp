#!/bin/bash

# Update package list for Linux
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    sudo apt update
fi

# Function to install Clang 18 from LLVM's repository and set it as default for Ubuntu
install_clang_18_and_set_default() {
    echo "Installing Clang 18..."
    
    sudo bash -c "$(wget -O - https://apt.llvm.org/llvm.sh)"

    echo "Setting Clang 18 as default..."
    sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 \
    --slave /usr/bin/clang++ clang++ /usr/bin/clang++-18
    sudo update-alternatives --install /usr/bin/cc cc /usr/bin/clang-18 100 \
    --slave /usr/bin/c++ c++ /usr/bin/clang++-18
}

# Install Python based on OS
if ! command -v python3 &> /dev/null; then
    echo "Python could not be found. Attempting to install Python."

    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        sudo apt install -y python3 python3-pip
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        brew install python
    else
        echo "Unsupported OS. Please install Python manually."
        exit 1
    fi
fi

# Install Conan
pip3 install conan
conan --version
echo "Conan has been installed successfully."

# Install CMake based on OS
if ! command -v cmake &> /dev/null; then
    echo "CMake could not be found. Attempting to install CMake."

    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        sudo apt install -y cmake
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        brew install cmake
    else
        echo "Unsupported OS. Please install CMake manually."
        exit 1
    fi
fi
echo "CMake has been installed successfully."

# Check for Clang version and install Clang 18 if on Ubuntu and not installed or if an older version is installed
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if command -v clang &> /dev/null; then
        installed_clang_version=$(clang -dumpversion | cut -d. -f1)
        if [ "$installed_clang_version" -lt 18 ]; then
            install_clang_18_and_set_default
        else
            echo "Clang version 18 or newer is already installed and set as default."
        fi
    else
        echo "Clang is not installed. Installing Clang 18 and setting it as default."
        install_clang_18_and_set_default
    fi
fi

echo "All prerequisites have been installed and configured successfully."
