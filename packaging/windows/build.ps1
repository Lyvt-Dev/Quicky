param(
    [string]$Version = '0.1.0-preview.1',
    [Parameter(Mandatory=$true)][string]$QtRoot,
    [Parameter(Mandatory=$true)][string]$OpenSSLRoot,
    [string]$Build = 'build-windows'
)
$ErrorActionPreference = 'Stop'
function Run([scriptblock]$Command) {
    & $Command
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code $LASTEXITCODE" }
}
if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+(-[A-Za-z0-9.]+)?$') { throw 'Invalid version' }
$Root = (Resolve-Path "$PSScriptRoot/../..").Path
Push-Location $Root
try {
    $env:PATH = "$QtRoot/bin;$OpenSSLRoot/bin;$env:PATH"
    Run { cmake -S . -B $Build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtRoot" "-DOPENSSL_ROOT_DIR=$OpenSSLRoot" }
    Run { cmake --build $Build --parallel 2 }
    Run { ctest --test-dir $Build --output-on-failure }
    $Stage = Join-Path $Root "$Build/stage"
    New-Item -ItemType Directory -Force $Stage | Out-Null
    Copy-Item "$Build/Quicky.exe" $Stage
    Run { & "$QtRoot/bin/windeployqt.exe" --release --qmldir "$Root/apps/native" --dir $Stage "$Stage/Quicky.exe" }
    Copy-Item "$OpenSSLRoot/bin/libcrypto-3-x64.dll" $Stage
    Copy-Item "$OpenSSLRoot/bin/libssl-3-x64.dll" $Stage
    Copy-Item README.md,THIRD_PARTY_NOTICES.md $Stage
    Copy-Item -Recurse licenses $Stage -Force
    # Keep dynamically linked libraries replaceable for LGPL compliance.
    $Installer = Join-Path ${env:ProgramFiles(x86)} 'NSIS/makensis.exe'
    if (!(Test-Path $Installer)) { $Installer = (Get-Command makensis.exe).Source }
    Push-Location $Build
    try {
        Run { & $Installer "/DVERSION=$Version" "/DSTAGE=$Stage" "$Root/packaging/windows/Quicky.nsi" }
        $Exe = "Quicky-$Version-windows-x64-setup.exe"
        Get-FileHash -Algorithm SHA256 $Exe | ForEach-Object { "$($_.Hash.ToLower())  $Exe" } | Set-Content "$Exe.sha256" -Encoding ascii
    } finally { Pop-Location }
} finally { Pop-Location }
