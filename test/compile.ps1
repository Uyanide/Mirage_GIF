function Log-Info {
    param ([string]$Message)
    Write-Host "[INFO] $Message" -ForegroundColor Green
}

function Log-Error {
    param ([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

function Execute-Program {
    param (
        [string]$ProgramName,
        [string[]]$ArgsList
    )

    try {
        Log-Info "Executing $ProgramName $ArgsList"

        $process = Start-Process -FilePath $ProgramName -ArgumentList $ArgsList -NoNewWindow -PassThru
        $process.WaitForExit()

        Log-Info "Program exited with return code: $($process.ExitCode)"
        return $process.ExitCode
    } catch {
        Log-Error "An error occurred while executing the program: $_"
        return -1
    }
}


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