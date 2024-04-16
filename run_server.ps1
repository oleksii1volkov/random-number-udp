param(
    [string]$BuildType = "Debug"
)

$projectDirectory = Get-Location | Select-Object -ExpandProperty Path
$executablePath = "$projectDirectory\build\$BuildType\udp_server.exe"
$configPath = "$projectDirectory\config\server.json"
$logPath = "$projectDirectory\build\$BuildType\logs\server"

& $executablePath --config-path $configPath --logs-path $logPath
