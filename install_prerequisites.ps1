# Check and elevate privileges if not already running as Administrator
if (-NOT ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Start-Process powershell.exe -ArgumentList "-File `"$PSCommandPath`"" -Verb RunAs
    exit
}

# Requires running as Administrator and having Windows Package Manager (winget) available

# Check for Python and install it if not present
if (-Not (Get-Command "python" -ErrorAction SilentlyContinue)) {
    Write-Host "Python could not be found. Attempting to install Python using Windows Package Manager (winget)."
    winget install -i --id Python.Python.3 --accept-package-agreements --accept-source-agreements
}

# Install Conan
pip install conan

# Verify Conan installation
conan --version
Write-Host "Conan has been installed successfully."

# Check for CMake and install it if not present
if (-Not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    Write-Host "CMake could not be found. Attempting to install CMake using Windows Package Manager (winget)."
    winget install -i --id Kitware.CMake --accept-package-agreements --accept-source-agreements
}

# Verify CMake installation
cmake --version
Write-Host "CMake has been installed successfully."
