$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskMingw = 'E:\MCU\lvgl_tools\mingw64\mingw64\bin'
$taskCmake = 'E:\MCU\esp32\.espressif\v6.1\tools\cmake\4.0.3\bin\cmake.exe'
$taskNinja = 'E:\MCU\esp32\.espressif\v6.1\tools\ninja\1.12.1\ninja.exe'
$env:PATH = $taskMingw + ';' + $env:PATH
& $taskCmake -S "$taskRoot/tests/host" -B "$taskRoot/build-host" -G Ninja "-DCMAKE_MAKE_PROGRAM=$taskNinja" "-DCMAKE_C_COMPILER=$taskMingw/gcc.exe" "-DCMAKE_CXX_COMPILER=$taskMingw/g++.exe"
if ($LASTEXITCODE -ne 0) { throw 'Host configure failed' }
& $taskCmake --build "$taskRoot/build-host" --parallel 8
if ($LASTEXITCODE -ne 0) { throw 'Host build failed' }
& (Join-Path (Split-Path $taskCmake) 'ctest.exe') --test-dir "$taskRoot/build-host" --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
$taskPython = 'E:\MCU\esp32\.espressif\v6.1\tools\python_env\v6.1\venv\Scripts\python.exe'
& $taskPython "$taskRoot/tests/host/device_soak_tests.py"
if ($LASTEXITCODE -ne 0) { throw 'Serial soak harness fault tests failed' }
