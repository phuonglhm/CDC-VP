param(
    [string]$SystemCHome = $env:SYSTEMC_HOME,
    [string]$Distro = 'Ubuntu'
)
$ErrorActionPreference = 'Stop'
$projectPath = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($SystemCHome)) {
    throw 'Pass -SystemCHome with the Linux path to an installed SystemC library (see README.md).'
}
# Use WSLENV so paths containing spaces reach bash intact on Windows PowerShell.
$oldSystemC = $env:SYSTEMC_HOME
$oldWslEnv = $env:WSLENV
try {
    $env:SYSTEMC_HOME = $SystemCHome
    $env:WSLENV = (@($oldWslEnv, 'SYSTEMC_HOME') | Where-Object { $_ }) -join ':'
    Push-Location -LiteralPath $projectPath
    try {
        & wsl.exe -d $Distro -- bash scripts/build.sh
        if ($LASTEXITCODE -ne 0) { throw "Build/test failed: $LASTEXITCODE" }
    } finally { Pop-Location }
} finally {
    $env:SYSTEMC_HOME = $oldSystemC
    $env:WSLENV = $oldWslEnv
}
