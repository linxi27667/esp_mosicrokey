param([switch]$Build)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'environment.ps1')
if ($Build) { & (Join-Path $PSScriptRoot 'build.ps1') }
$taskPython = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
& $taskPython (Join-Path $PSScriptRoot 'verify-sources.py')
if ($LASTEXITCODE -ne 0) { throw 'Source snapshot verification failed' }
& $taskPython (Join-Path $PSScriptRoot 'verify-patches.py')
if ($LASTEXITCODE -ne 0) { throw 'Upstream patch verification failed' }
$taskBuild = Join-Path $taskRoot 'build'
$taskFlash = Get-Content -LiteralPath (Join-Path $taskBuild 'flasher_args.json') -Raw | ConvertFrom-Json
if ($taskFlash.extra_esptool_args.chip -ne 'esp32s31') { throw 'Unexpected firmware target' }
$taskRelease = Join-Path $taskRoot 'release'
New-Item -ItemType Directory -Path $taskRelease -Force | Out-Null
$taskDescription = Get-Content -LiteralPath (Join-Path $taskBuild 'project_description.json') -Raw | ConvertFrom-Json
$taskVersion = $taskDescription.project_version
if ($taskVersion -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid project release version' }
$taskImage = Join-Path $taskRelease "mosicrokey-v$taskVersion-esp32s31.bin"
$taskArguments = @('-m', 'esptool', '--chip', 'esp32s31', 'merge-bin', '-o', $taskImage,
    '--pad-to-size', '16MB') + @($taskFlash.write_flash_args)
$taskSegments = @()
foreach ($taskEntry in $taskFlash.flash_files.PSObject.Properties | Sort-Object { [Convert]::ToInt32($_.Name, 16) }) {
    $taskFile = Join-Path $taskBuild $taskEntry.Value
    if (-not (Test-Path -LiteralPath $taskFile)) { throw "Missing firmware segment: $taskFile" }
    $taskArguments += @($taskEntry.Name, $taskFile)
    $taskSegments += [ordered]@{ offset = $taskEntry.Name; file = $taskEntry.Value;
        bytes = (Get-Item -LiteralPath $taskFile).Length;
        sha256 = (Get-FileHash -LiteralPath $taskFile -Algorithm SHA256).Hash.ToLower() }
}
& $taskPython @taskArguments
if ($LASTEXITCODE -ne 0) { throw 'Firmware merge failed' }
if ((Get-Item -LiteralPath $taskImage).Length -ne 16MB) { throw 'Incorrect merged image size' }
& $taskPython (Join-Path $PSScriptRoot 'check-firmware.py')
if ($LASTEXITCODE -ne 0) { throw 'Firmware integrity check failed' }
Copy-Item -LiteralPath (Join-Path $taskBuild 'mosicrokey.elf') -Destination $taskRelease -Force
Copy-Item -LiteralPath (Join-Path $taskRoot 'sdkconfig') -Destination $taskRelease -Force
Copy-Item -LiteralPath (Join-Path $taskRoot 'dependencies.lock') -Destination $taskRelease -Force
Copy-Item -LiteralPath (Join-Path $taskRoot 'partitions.csv') -Destination $taskRelease -Force
$taskLicenseRoot = Join-Path $taskRelease 'licenses'
$taskLicenseSources = @(
    @{ Path = (Join-Path $taskRoot 'third_party'); Name = 'third_party' },
    @{ Path = (Join-Path $taskRoot 'managed_components'); Name = 'managed_components' },
    @{ Path = (Join-Path $taskRoot 'assets/fonts'); Name = 'fonts' },
    @{ Path = (Join-Path $env:IDF_PATH 'components'); Name = 'esp-idf/components' })
foreach ($taskLicenseSource in $taskLicenseSources) {
    foreach ($taskLicense in Get-ChildItem -LiteralPath $taskLicenseSource.Path -Recurse -File | Where-Object { $_.Name -match '^(LICEN[CS]E|COPYING|NOTICE|OFL|UFL)(\.[^.]+)?$' }) {
        $taskRelative = $taskLicense.FullName.Substring($taskLicenseSource.Path.Length + 1)
        $taskDestination = Join-Path (Join-Path $taskLicenseRoot $taskLicenseSource.Name) $taskRelative
        New-Item -ItemType Directory -Path (Split-Path $taskDestination -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $taskLicense.FullName -Destination $taskDestination -Force
    }
}
New-Item -ItemType Directory -Path (Join-Path $taskLicenseRoot 'esp-idf') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $env:IDF_PATH 'LICENSE') -Destination (Join-Path $taskLicenseRoot 'esp-idf/LICENSE') -Force
Copy-Item -LiteralPath (Join-Path $taskRoot 'managed_components/georgik__lua/lua/lua.h') -Destination (Join-Path $taskLicenseRoot 'Lua-copyright-lua.h') -Force
$taskManifest = [ordered]@{
    version = $taskVersion; target = 'esp32s31'; sdk = '6.1.0'; flash_size = '16MB'; flash_offset = '0x0';
    created_at = (Get-Date).ToUniversalTime().ToString('o');
    image = (Split-Path $taskImage -Leaf); bytes = (Get-Item -LiteralPath $taskImage).Length;
    sha256 = (Get-FileHash -LiteralPath $taskImage -Algorithm SHA256).Hash.ToLower();
    elf_sha256 = (Get-FileHash -LiteralPath (Join-Path $taskRelease 'mosicrokey.elf') -Algorithm SHA256).Hash.ToLower();
    source_manifest_sha256 = (Get-FileHash -LiteralPath (Join-Path $taskRoot 'source-manifest.json') -Algorithm SHA256).Hash.ToLower();
    upstream = (Get-Content -LiteralPath (Join-Path $taskRoot 'upstream-lock.json') -Raw | ConvertFrom-Json);
    segments = $taskSegments;
    validation = @{ target_build = 'artifacts merged; see docs/validation for build evidence'; host_tests = 'see docs/validation; packaging does not run tests';
        hardware = 'see docs/validation/hardware-2026-10-01; USB results do not certify external peripherals';
        live_ai = 'see dated hardware evidence for actual firmware hash and service scope';
        two_hour_stability = 'see dated device-soak summary; packaging does not run the soak' }
}
$taskUtf8 = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $taskRelease 'manifest.json'), ($taskManifest | ConvertTo-Json -Depth 12), $taskUtf8)
[IO.File]::WriteAllText((Join-Path $taskRelease 'SHA256SUMS.txt'), "$($taskManifest.sha256)  $($taskManifest.image)`n$($taskManifest.elf_sha256)  mosicrokey.elf`n", $taskUtf8)
Write-Output "Release: $taskImage (16MB), SHA256 $($taskManifest.sha256)"
