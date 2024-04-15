param(
    [string]$BuildType = "Debug"
)

$projectDirectory = Get-Location | Select-Object -ExpandProperty Path
$executablePath = "$projectDirectory\build\$BuildType\udp_client.exe"
$configPath = "$projectDirectory\config\client.json"
$logPath = "$projectDirectory\build\$BuildType\logs\client"
$numbersPath = "$projectDirectory\build\$BuildType\numbers.bin"

& $executablePath --config-path $configPath --logs-path $logPath --numbers-path $numbersPath
