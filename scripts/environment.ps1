# EIM installs tool families directly under its tools directory. The stock
# export.ps1 expects an additional tools/ level; use the verified EIM paths.
$env:IDF_PATH = 'E:\MCU\esp32\.espressif\v6.1\esp-idf'
$env:ESP_IDF_VERSION = '6.1'
$env:IDF_VERSION = '6.1.0'
$env:PYTHONUTF8 = '1'
$env:IDF_TOOLS_PATH = 'E:\MCU\esp32\.espressif\v6.1\tools'
$env:IDF_PYTHON_ENV_PATH = Join-Path $env:IDF_TOOLS_PATH 'python_env\v6.1\venv'
$env:IDF_COMPONENT_LOCAL_STORAGE_URL = 'file://E:/MCU/esp32/.espressif/v6.1/tools'
$env:ESP_ROM_ELF_DIR = Join-Path $env:IDF_TOOLS_PATH 'esp-rom-elfs\20241011'
$taskToolBins = @(
    'python_env\v6.1\venv\Scripts',
    'riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin',
    'cmake\4.0.3\bin', 'ninja\1.12.1',
    'ccache\4.12.1\ccache-4.12.1-windows-x86_64')
$env:PATH = (($taskToolBins | ForEach-Object { Join-Path $env:IDF_TOOLS_PATH $_ }) -join ';') + ';' + $env:PATH
function global:Invoke-MosicrokeyIdf {
    & (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe') (Join-Path $env:IDF_PATH 'tools\idf.py') @args
    $global:LASTEXITCODE = $LASTEXITCODE
    if ($LASTEXITCODE -ne 0) { throw "idf.py failed with exit code $LASTEXITCODE" }
}
Set-Alias -Name idf.py -Value Invoke-MosicrokeyIdf -Scope Global -Force
