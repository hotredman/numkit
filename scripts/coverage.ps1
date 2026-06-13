<#
.SYNOPSIS
  Source-based code coverage for numkit (architecture-review risk #6).

.DESCRIPTION
  Builds the `coverage` CMake preset (Ninja + clang-cl, NUMKIT_COVERAGE=ON),
  runs the gtest suite under LLVM_PROFILE_FILE, merges the raw profiles with
  llvm-profdata and prints an llvm-cov line/region/function report scoped to
  src/ (third_party / _deps / test code filtered out).

  Ninja + clang-cl need the MSVC dev environment, so the script enters a VS
  Dev Shell first. Requires VS 2022 Professional (ships clang-cl + llvm-cov +
  llvm-profdata under VC/Tools/Llvm/x64).

.EXAMPLE
  powershell -File scripts/coverage.ps1
      Full suite, summary report.

.EXAMPLE
  powershell -File scripts/coverage.ps1 -Html --gtest_filter=*Normalize*:*Stats*
      Subset run (passthrough gtest args) + an HTML report under build/coverage/html.
#>
param(
    [switch]$Html,
    [switch]$NoBuild,
    [Parameter(ValueFromRemainingArguments = $true)] [string[]]$GtestArgs
)
$ErrorActionPreference = "Stop"
$repo  = (Resolve-Path "$PSScriptRoot/..").Path
$vs    = "C:/Program Files/Microsoft Visual Studio/2022/Professional"
$llvm  = "$vs/VC/Tools/Llvm/x64/bin"
$build = Join-Path $repo "build/coverage"
$ignore = '(third_party|_deps|googletest|benchmark|[\\/]tests[\\/])'

Set-Location $repo

# 1. MSVC dev environment (Ninja + clang-cl need INCLUDE / LIB / PATH).
Import-Module "$vs/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
Set-Location $repo

# 2. Configure + build the instrumented gtest runner.
if (-not $NoBuild) {
    cmake --preset coverage
    if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    cmake --build --preset coverage --target numkit_gtest
    if ($LASTEXITCODE -ne 0) { throw "coverage build failed" }
}

$exe = "$build/tests/gtest/numkit_gtest.exe"
if (-not (Test-Path $exe)) { throw "coverage build did not produce $exe" }

# 3. Run the suite. gtest is a single process, so write to one fixed,
# absolute profraw — no globbing, no %p/flush/cwd races.
$raw   = Join-Path $build "numkit.profraw"
$pdata = Join-Path $build "numkit.profdata"
Remove-Item $raw, $pdata -ErrorAction SilentlyContinue
$env:LLVM_PROFILE_FILE = $raw
& $exe @GtestArgs
$runExit = $LASTEXITCODE
$env:LLVM_PROFILE_FILE = $null
if ($runExit -ne 0) { Write-Warning "numkit_gtest exited $runExit — coverage below reflects only what ran." }

# 4. Merge + report, scoped to src/.
if (-not (Test-Path $raw)) { throw "no profraw at $raw — was the build instrumented (NUMKIT_COVERAGE=ON)?" }
& "$llvm/llvm-profdata.exe" merge -sparse $raw -o $pdata
if ($LASTEXITCODE -ne 0) { throw "llvm-profdata merge failed" }

Write-Host "`n==== numkit source coverage (src/, third_party + tests excluded) ====`n"
& "$llvm/llvm-cov.exe" report $exe -instr-profile=$pdata -ignore-filename-regex=$ignore "src"

if ($Html) {
    & "$llvm/llvm-cov.exe" show $exe -instr-profile=$pdata `
        -format=html -output-dir="$build/html" -ignore-filename-regex=$ignore "src"
    Write-Host "`nHTML report: $build/html/index.html"
}
