param(
    [string]$ArduinoCli = 'D:\Mixly3.0\arduino-cli\arduino-cli.exe',
    [string]$ArduinoEnvironment = '',
    [string]$Port = 'COM3',
    [switch]$Upload
)

$ErrorActionPreference = 'Stop'
$boardRoot = $PSScriptRoot
$repoRoot = Split-Path -Parent $boardRoot
$firmwareRoot = Join-Path $boardRoot 'firmware'
$taskStage = Join-Path $boardRoot 'build\midea_s3_webota'
$taskBuild = Join-Path $boardRoot 'build\compiled_ascii'
$arduinoEnv = if ($ArduinoEnvironment) { [IO.Path]::GetFullPath($ArduinoEnvironment) } else { Join-Path $boardRoot 'build\ArduinoEnv' }
if (-not (Test-Path -LiteralPath (Join-Path $arduinoEnv 'data\packages\esp32\hardware\esp32'))) {
    $sharedS3Env = Join-Path $repoRoot 'ESP32-S3-MINI\build\ArduinoEnv'
    if (-not $ArduinoEnvironment -and (Test-Path -LiteralPath (Join-Path $sharedS3Env 'data\packages\esp32\hardware\esp32'))) {
        $arduinoEnv = $sharedS3Env
    } else { throw 'No installed ESP32-S3 toolchain found. Set -ArduinoEnvironment to an existing Arduino environment.' }
}

if (-not (Test-Path -LiteralPath $ArduinoCli)) { throw 'Set -ArduinoCli to your installed arduino-cli.exe.' }
if (-not (Test-Path -LiteralPath (Join-Path $firmwareRoot 'secrets.h'))) { throw 'Copy firmware\secrets.example.h to firmware\secrets.h and configure it first.' }
$taskFiles = @('miot_bridge.h','miot_profile.h','miot_auto_mode.h','miot_simulator.h','miot_simulated_properties.h','midea_uart_state.h','bridge_ble.h','bridge_ble.cpp','device_metrics.h','web_pages.h','responsive_web_server.h','http_request_gate.h','http_response_pump.h','wifi_health.h','wifi_health_policy.h','build_opt.h','secrets.h','partitions.csv')
foreach ($file in $taskFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $firmwareRoot $file))) { throw "Missing firmware\$file." }
}


New-Item -ItemType Directory -Path $taskStage -Force | Out-Null
New-Item -ItemType Directory -Path $taskBuild -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $arduinoEnv 'data') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $arduinoEnv 'downloads') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $arduinoEnv 'user') -Force | Out-Null
function Copy-TaskSource {
    param([string]$Source, [string]$Destination)
    if ((Test-Path -LiteralPath $Destination) -and
        (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash -eq
        (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash) { return }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}
Copy-TaskSource (Join-Path $firmwareRoot 'midea_s3_bridge.ino') (Join-Path $taskStage 'midea_s3_webota.ino')
foreach ($file in $taskFiles) {
    Copy-TaskSource (Join-Path $firmwareRoot $file) (Join-Path $taskStage $file)
}
$qr = Join-Path $firmwareRoot 'pairing_qr.h'
if (Test-Path -LiteralPath $qr) { Copy-TaskSource $qr (Join-Path $taskStage 'pairing_qr.h') }

# The Xtensa linker in the Windows toolchain cannot reliably create files under
# a Unicode path. A temporary SUBST drive gives the compiler ASCII-only paths.
$usedDrives = @(Get-PSDrive -PSProvider FileSystem | ForEach-Object { $_.Name.ToUpperInvariant() })
$driveLetter = @('S','T','U','V','W','X','Y','Z') | Where-Object { $_ -notin $usedDrives } | Select-Object -First 1
$environmentDriveLetter = @('S','T','U','V','W','X','Y','Z') | Where-Object { $_ -notin $usedDrives -and $_ -ne $driveLetter } | Select-Object -First 1
if (-not $driveLetter) { throw 'No free drive letter from R: to Z: is available for the temporary S3 build path.' }
if (-not $environmentDriveLetter) { throw 'Two free drive letters are required for ASCII source and toolchain paths.' }
$substExe = Join-Path $env:SystemRoot 'System32\subst.exe'
$substName = $driveLetter + ':'
$environmentSubstName = $environmentDriveLetter + ':'
$taskPreviousTemp = $env:TEMP
$taskPreviousTmp = $env:TMP
& $substExe $substName $boardRoot
if ($LASTEXITCODE -ne 0) { throw 'Could not create a temporary ASCII build path with SUBST.' }
& $substExe $environmentSubstName $arduinoEnv
if ($LASTEXITCODE -ne 0) {
    & $substExe $substName /d
    throw 'Could not create a temporary ASCII toolchain path with SUBST.'
}

try {
    $nativeRoot = $driveLetter + ':\'
    $nativeBoardRoot = $nativeRoot
    $nativeTaskStage = Join-Path $nativeBoardRoot 'build\midea_s3_webota'
    $nativeTaskBuild = Join-Path $nativeBoardRoot 'build\compiled_ascii'
    $nativeArduinoEnv = $environmentSubstName + '\'
    $taskTemp = Join-Path $nativeBoardRoot 'build\temp'
    New-Item -ItemType Directory -Path $taskTemp -Force | Out-Null
    $env:TEMP = $taskTemp
    $env:TMP = $taskTemp
    $configFile = Join-Path $nativeBoardRoot 'build\arduino-cli-ascii.yaml'
    $dataPath = (Join-Path $nativeArduinoEnv 'data').Replace('\','/')
    $downloadsPath = (Join-Path $nativeArduinoEnv 'downloads').Replace('\','/')
    $userPath = (Join-Path $nativeArduinoEnv 'user').Replace('\','/')
    $config = "board_manager:`n  additional_urls:`n    - https://dl.espressif.com/dl/package_esp32_index.json`ndirectories:`n  data: $dataPath`n  downloads: $downloadsPath`n  user: $userPath`n"
    [IO.File]::WriteAllText($configFile, $config, [Text.UTF8Encoding]::new($false))

    $fqbn = 'esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,PartitionScheme=min_spiffs'
    & $ArduinoCli --config-file $configFile compile --fqbn $fqbn --build-property 'build.partitions=s3_16MB' --build-property 'upload.maximum_size=6553600' --build-path $nativeTaskBuild $nativeTaskStage
    if ($LASTEXITCODE -ne 0) { throw 'S3 firmware build failed; upload was not attempted.' }

    $version = [regex]::Match([IO.File]::ReadAllText((Join-Path $firmwareRoot 'bridge_ble.h')), '#define\s+BRIDGE_VERSION\s+"([0-9A-Za-z._-]+)"').Groups[1].Value
    if (-not $version) { throw 'Cannot read BRIDGE_VERSION for the OTA package name.' }
    $otaDir = Join-Path $boardRoot 'dist\firmware'
    New-Item -ItemType Directory -Path $otaDir -Force | Out-Null
    $otaBin = Join-Path $otaDir "Midea-Bridge-$version-ESP32-S3-MINI-web-OTA.bin"
    Copy-Item -LiteralPath (Join-Path $taskBuild 'midea_s3_webota.ino.bin') -Destination $otaBin -Force
    $hash = (Get-FileHash -LiteralPath $otaBin -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath ($otaBin + '.sha256') -Encoding ASCII -Value ($hash + '  ' + [IO.Path]::GetFileName($otaBin))
    $fullFlash = Join-Path $otaDir "Midea-Bridge-$version-ESP32-S3-MINI-full-flash-16MB.bin"
    Copy-Item -LiteralPath (Join-Path $taskBuild 'midea_s3_webota.ino.merged.bin') -Destination $fullFlash -Force
    $fullHash = (Get-FileHash -LiteralPath $fullFlash -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -LiteralPath ($fullFlash + '.sha256') -Encoding ASCII -Value ($fullHash + '  ' + [IO.Path]::GetFileName($fullFlash))
    Write-Host "S3 web OTA firmware: $otaBin"
    Write-Host "S3 complete 16 MB serial flash image: $fullFlash"
    Write-Host "Board options: $fqbn"
    Write-Host 'Flash layout: app0 6.25 MiB + app1 6.25 MiB, SPIFFS 3.375 MiB, NVS/OTA/coredump reserved.'
    Write-Host 'Use only the web-OTA image in the web updater. The full-flash image is for initial serial flashing at offset 0 and replaces the entire flash contents.'
    Write-Host 'Do not upload either S3 image to the classic ESP32 DevKitC.'

    if ($Upload) {
        & $ArduinoCli --config-file $configFile upload --fqbn $fqbn --port $Port --input-dir $nativeTaskBuild $nativeTaskStage
        if ($LASTEXITCODE -ne 0) { throw 'Firmware upload failed.' }
    }
}
finally {
    if ($null -eq $taskPreviousTemp) { Remove-Item Env:TEMP -ErrorAction SilentlyContinue } else { $env:TEMP = $taskPreviousTemp }
    if ($null -eq $taskPreviousTmp) { Remove-Item Env:TMP -ErrorAction SilentlyContinue } else { $env:TMP = $taskPreviousTmp }
    & $substExe $substName /d
    & $substExe $environmentSubstName /d
}
