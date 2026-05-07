$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Build = Join-Path $Root "build\win32"
$AppDist = Join-Path $Root "dist\app"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
  throw "Missing CMake. Install CMake and add it to PATH."
}
Remove-Item -Recurse -Force $Build, $AppDist -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $AppDist | Out-Null

cmake -S $Root -B $Build -A Win32
cmake --build $Build --config Release --parallel

$Exe = Join-Path $Build "Release\sidekey.exe"
if (-not (Test-Path $Exe)) {
  throw "Build finished, but $Exe was not found."
}

Copy-Item $Exe (Join-Path $AppDist "sidekey.exe") -Force

$InstallerRes = Join-Path $Root "dist\installer.res"
$InstallerOut = Join-Path $Root "dist\SideKey-Win7-Setup.exe"

Push-Location $Root
try {
  & rc.exe /nologo /fo $InstallerRes "installer\installer.rc"
  & cl.exe /nologo /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_WIN32_WINNT=0x0601 `
    "installer\installer.cpp" `
    $InstallerRes `
    "/Fe:$InstallerOut" `
    /link /SUBSYSTEM:WINDOWS,6.01 user32.lib shell32.lib advapi32.lib ole32.lib uuid.lib
} finally {
  Pop-Location
}

Write-Host "Built installer:"
Write-Host $InstallerOut
