param([string]$Port, [switch]$Flash)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'environment.ps1')
Push-Location $taskRoot
try {
    & (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe') (Join-Path $PSScriptRoot 'verify-sources.py')
    if ($LASTEXITCODE -ne 0) { throw 'Source snapshot verification failed' }
    & (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe') (Join-Path $PSScriptRoot 'verify-patches.py')
    if ($LASTEXITCODE -ne 0) { throw 'Upstream patch verification failed' }
    idf.py build
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    if ($Flash) {
        if (-not $Port) { throw 'Supply the confirmed board COM port with -Port' }
        idf.py -p $Port flash
        if ($LASTEXITCODE -ne 0) { throw 'Flash failed' }
    }
} finally { Pop-Location }
