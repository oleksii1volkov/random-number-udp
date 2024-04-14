$buildType = "Debug"
$projectDirectory = Get-Location | Select-Object -ExpandProperty Path
$executablePath = "$projectDirectory\\build\\$buildType\\udp_server.exe"
$configPath = "$projectDirectory\\config\\client.json"
$logPath = "$projectDirectory\\build\\$buildType\\log"

& $executablePath --config-path $configPath --logs-path $logPath
