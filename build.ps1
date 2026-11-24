param(
    [switch]$Run,
    [switch]$SmokeTest,
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

# Locate CMake (PATH or Visual Studio 2022 bundled CMake)
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source)
if (-not $cmake) {
    $vsCmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCmake) {
        $cmake = $vsCmake
    } else {
        throw "CMake not found. Please install CMake or Visual Studio 2022 C++ CMake tools."
    }
}

Write-Host "[Build] Using CMake: $cmake" -ForegroundColor Cyan

if (-not (Test-Path "build\CMakeCache.txt")) {
    & $cmake -S . -B build -G "Visual Studio 17 2022" -A x64
}

& $cmake --build build --config $Config --parallel

$exe = Join-Path (Get-Location) "build\$Config\AshenCrownRPG.exe"
Write-Host "[Build] Executable ready: $exe" -ForegroundColor Green

if ($SmokeTest) {
    & $exe --smoke-test
} elseif ($Run) {
    & $exe
}
