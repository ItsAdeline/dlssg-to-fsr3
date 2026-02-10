$ErrorActionPreference = "Stop"

# Set up powershell equivalent of vcvarsall.bat when compiler/tools aren't in PATH
if ($env:VCPKG_ROOT -eq $null -or $env:VCPKG_ROOT -eq "") {
    if (Test-Path "C:\vcpkg") {
        $env:VCPKG_ROOT = "C:\vcpkg"
    }
}

if ((Get-Command "cl" -ErrorAction SilentlyContinue) -eq $null) {
    $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationpath

    if ($vsPath) {
        $modulePath = (Get-ChildItem $vsPath -Recurse -File -Filter Microsoft.VisualStudio.DevShell.dll).FullName
        if ($modulePath) {
            Import-Module $modulePath
            Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64'
        }
    }
}

# Then build with VS
# & cmake --preset final-dtwrapper
# & cmake --build --preset final-dtwrapper-release
# & cpack --preset final-dtwrapper

# & cmake --preset final-nvngxwrapper
# & cmake --build --preset final-nvngxwrapper-release
# & cpack --preset final-nvngxwrapper

# & cmake --preset final-universal
# & cmake --build --preset final-universal-release
# & cpack --preset final-universal

& cmake --preset final-noloader
& cmake --build --preset final-noloader-release
& cpack --preset final-noloader