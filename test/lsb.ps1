. $PSScriptRoot\utils.ps1

function Compare-File {
    param (
        [string]$file1,
        [string]$file2
    )

    $hash1 = Get-FileHash -Path $file1 -Algorithm SHA256
    $hash2 = Get-FileHash -Path $file2 -Algorithm SHA256

    if ($hash1.Hash -eq $hash2.Hash) {
        return $true
    } else {
        return $false
    }
}

$encPath = "$PSScriptRoot\..\bin\Release\GIFLsb-enc.exe"
$decPath = "$PSScriptRoot\..\bin\Release\GIFLsb-dec.exe"

$returnCode = Execute-Program -ProgramName $encPath -ArgsList @(
    "$PSScriptRoot\..\images\气气.gif",
    "$PSScriptRoot\..\images\马达.gif",
    "-o",
    "$PSScriptRoot\lsb\gif",
    "-p", "0",
    "-m", "whatever"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $decPath -ArgsList @(
    "$PSScriptRoot\lsb\gif.gif",
    "-o",
    "gif-dec.gif",
    "-d",
    "$PSScriptRoot\lsb\"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$isSame = Compare-File -file1 "$PSScriptRoot\..\images\马达.gif" -file2 "$PSScriptRoot\lsb\gif-dec.gif"

if ($isSame -ne $true) {
    Write-Host "Failed."
    exit 1
}

$returnCode = Execute-Program -ProgramName $encPath -ArgsList @(
    "$PSScriptRoot\..\images\3632618f6239e2f08b3191c3f9399aa928ed7ea2.webp",
    "$PSScriptRoot\..\images\马达.gif",
    "-o",
    "$PSScriptRoot\lsb\webp",
    "-p", "12",
    "-c", "2",
    "-g",
    "-m", "none"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $decPath -ArgsList @(
    "$PSScriptRoot\lsb\webp.gif",
    "-o",
    "webp-dec.gif",
    "-d",
    "$PSScriptRoot\lsb\"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$isSame = Compare-File -file1 "$PSScriptRoot\..\images\马达.gif" -file2 "$PSScriptRoot\lsb\webp-dec.gif"

if ($isSame -ne $true) {
    Write-Host "Failed."
    exit 1
}

$returnCode = Execute-Program -ProgramName $encPath -ArgsList @(
    "$PSScriptRoot\..\images\7808ce7d382f950d32732a52c8dc972d3d27a9a8.png",
    "$PSScriptRoot\..\images\slime.jpg",
    "-o",
    "$PSScriptRoot\lsb\png",
    "-l",
    "-c", "31",
    "-d"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $decPath -ArgsList @(
    "$PSScriptRoot\lsb\png.gif",
    "-o",
    "png-dec.jpg",
    "-d",
    "$PSScriptRoot\lsb\"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$isSame = Compare-File -file1 "$PSScriptRoot\..\images\slime.jpg" -file2 "$PSScriptRoot\lsb\png-dec.jpg"

if ($isSame -ne $true) {
    Write-Host "Failed."
    exit 1
}