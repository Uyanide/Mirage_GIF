. .\utils.ps1

$currentDirectory = Get-CurrentDirectory
$exePath = "$currentDirectory\..\bin\Release\GIFMirage.exe"

$returnCode = Execute-Program -ProgramName $exePath -ArgsList @(
    "$currentDirectory\..\images\气气.gif",
    "$currentDirectory\..\images\马达.gif",
    "-o",
    "$currentDirectory\mirage\gif-gif",
    "-p", "0"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $exePath -ArgsList @(
    "$currentDirectory\..\images\3632618f6239e2f08b3191c3f9399aa928ed7ea2.webp",
    "$currentDirectory\..\images\气气.gif",
    "-o",
    "$currentDirectory\mirage\webp-gif",
    "-x", "500",
    "-y", "400"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $exePath -ArgsList @(
    "$currentDirectory\..\images\马达.gif",
    "$currentDirectory\..\images\7808ce7d382f950d32732a52c8dc972d3d27a9a8.png",
    "-o",
    "$currentDirectory\mirage\gif-png",
    "-f", "20",
    "-d", "100"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

# $returnCode = Execute-Program -ProgramName $exePath -ArgsList @(
#     "$currentDirectory\..\images\气气.gif",
#     "$currentDirectory\..\images\马达.gif",
#     "-o",
#     "$currentDirectory\mirage\m0",
#     "-m", "0",
#     "-p", "12"
# )

# if ($returnCode -ne 0) {
#     exit $returnCode
# }

for ($s = 0; $s -le 4; $s++) {
    for ($w = 1; $w -le 4; $w++) {
        foreach ($suffix in @("C", "R")) {
            $mode = "S${s}W${w}${suffix}"
            $returnCode = Execute-Program -ProgramName $exePath -ArgsList @(
                "$currentDirectory\..\images\气气.gif",
                "$currentDirectory\..\images\马达.gif",
                "-o",
                "$currentDirectory\mirage\$mode",
                "-m", "$mode",
                "-p", "12"
            )

            if ($returnCode -ne 0) {
                Write-Host "处理模式 $mode 时出错，退出代码: $returnCode"
                exit $returnCode
            }
        }
    }
}