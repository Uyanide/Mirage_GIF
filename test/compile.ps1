. $PSScriptRoot\utils.ps1


if ($args -contains "--configure") {
    Remove-Item -Path "$PSScriptRoot\..\build" -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -Path "$PSScriptRoot\..\build" -ItemType Directory -Force | Out-Null
    $ret = Execute-Program -ProgramName "cmake" -ArgsList @(
        "-S",
        "$PSScriptRoot\..",
        "-B",
        "$PSScriptRoot\..\build",
        "-DVCPKG_TARGET_TRIPLET=x64-windows-static",
        "-DCMAKE_TOOLCHAIN_FILE=C:/Users/cyani/code/vcpkg/scripts/buildsystems/vcpkg.cmake"
    )
    if ($ret -ne 0) {
        exit $ret
    }
}

Execute-Program -ProgramName "cmake" -ArgsList @(
    "--build",
    "$PSScriptRoot\..\build",
    "--config",
    "RELEASE"
)