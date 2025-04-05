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

function Get-CurrentDirectory {
    $currentDirectory = $PSScriptRoot
    Log-Info "Current directory: $currentDirectory"
    return $currentDirectory
}