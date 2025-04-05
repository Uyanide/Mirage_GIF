. .\utils.ps1

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

$currentDirectory = Get-CurrentDirectory
$encPath = "$currentDirectory\..\bin\Release\GIFLsb-enc.exe"
$decPath = "$currentDirectory\..\bin\Release\GIFLsb-dec.exe"

$returnCode = Execute-Program -ProgramName $encPath -ArgsList @(
    "$currentDirectory\..\images\气气.gif",
    "$currentDirectory\..\images\马达.gif",
    "-o",
    "$currentDirectory\lsb\gif",
    "-p", "0",
    "-m", "whatever"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $decPath -ArgsList @(
    "$currentDirectory\lsb\gif.gif",
    "-o",
    "$currentDirectory\lsb\gif-dec.gif"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$isSame = Compare-File -file1 "$currentDirectory\..\images\马达.gif" -file2 "$currentDirectory\lsb\gif-dec.gif"

if ($isSame -ne $true) {
    Write-Host "Failed."
    exit 1
}

$returnCode = Execute-Program -ProgramName $encPath -ArgsList @(
    "$currentDirectory\..\images\3632618f6239e2f08b3191c3f9399aa928ed7ea2.webp",
    "$currentDirectory\..\images\马达.gif",
    "-o",
    "$currentDirectory\lsb\webp",
    "-p", "12",
    "-c", "2",
    "-g",
    "-m", "none"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $decPath -ArgsList @(
    "$currentDirectory\lsb\webp.gif",
    "-o",
    "$currentDirectory\lsb\webp-dec.gif"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$isSame = Compare-File -file1 "$currentDirectory\..\images\马达.gif" -file2 "$currentDirectory\lsb\webp-dec.gif"

if ($isSame -ne $true) {
    Write-Host "Failed."
    exit 1
}

$returnCode = Execute-Program -ProgramName $encPath -ArgsList @(
    "$currentDirectory\..\images\7808ce7d382f950d32732a52c8dc972d3d27a9a8.png",
    "$currentDirectory\..\images\slime.jpg",
    "-o",
    "$currentDirectory\lsb\png",
    "-l",
    "-c", "31",
    "-d"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $decPath -ArgsList @(
    "$currentDirectory\lsb\png.gif",
    "-o",
    "$currentDirectory\lsb\png-dec.jpg"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$isSame = Compare-File -file1 "$currentDirectory\..\images\slime.jpg" -file2 "$currentDirectory\lsb\png-dec.jpg"

if ($isSame -ne $true) {
    Write-Host "Failed."
    exit 1
}