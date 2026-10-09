param(
    [Parameter(Mandatory=$true)][string]$SystemCSourceDir,
    [string]$CMake = 'cmake',
    [string]$Compiler = '',
    [int]$Jobs = 8
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build/vp'
$configureArgs = @('-S', $projectRoot, '-B', $buildDirectory, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', "-DSYSTEMC_SOURCE_DIR=$SystemCSourceDir")
if ($Compiler) { $configureArgs += "-DCMAKE_CXX_COMPILER=$Compiler" }
& $CMake @configureArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& $CMake --build $buildDirectory -j $Jobs
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
$cmakePath = (Get-Command $CMake).Source
$ctest = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
& $ctest --test-dir $buildDirectory --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
