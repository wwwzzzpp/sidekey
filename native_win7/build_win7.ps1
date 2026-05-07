$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$outDir = Join-Path $root "build\native_win7\x64\Release"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$vsRoot = "${env:ProgramFiles}\Microsoft Visual Studio\18\Insiders"
$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
  throw "Visual Studio vcvars64.bat not found: $vcvars"
}

$source = Join-Path $PSScriptRoot "sidekey_win7.cpp"
$resource = Join-Path $PSScriptRoot "sidekey_win7.rc"
$resOut = Join-Path $outDir "sidekey_win7.res"
$objOut = Join-Path $outDir "sidekey_win7.obj"
$exeOut = Join-Path $outDir "sidekey-win7.exe"

$bat = Join-Path $outDir "build_sidekey_win7.bat"
Set-Content -LiteralPath $bat -Encoding ASCII -Value @"
@echo on
call "$vcvars"
if errorlevel 1 exit /b %errorlevel%
rc /nologo /fo "$resOut" "$resource"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /O2 /MT /EHsc /std:c++17 /utf-8 /DUNICODE /D_UNICODE /DWINVER=0x0601 /D_WIN32_WINNT=0x0601 "$source" "$resOut" /Fo:"$objOut" /Fe:"$exeOut" /link /SUBSYSTEM:WINDOWS,6.01 user32.lib gdi32.lib shell32.lib advapi32.lib comctl32.lib
exit /b %errorlevel%
"@

cmd.exe /c "`"$bat`""
if ($LASTEXITCODE -ne 0) {
  throw "Native Win7 build failed with exit code $LASTEXITCODE"
}

Write-Host "Built $exeOut"
