. $PSScriptRoot\utils.ps1

$exePath = "$PSScriptRoot\..\bin\Release\GIFMirage.exe"


$returnCode = Execute-Program -ProgramName $exePath -ArgsList @(
    "$PSScriptRoot\..\images\气气.gif",
    "$PSScriptRoot\..\images\马达.gif",
    "-o",
    "$PSScriptRoot\mirage\gif-gif",
    "-p", "0"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $exePath -ArgsList @(
    "$PSScriptRoot\..\images\3632618f6239e2f08b3191c3f9399aa928ed7ea2.webp",
    "$PSScriptRoot\..\images\气气.gif",
    "-o",
    "$PSScriptRoot\mirage\webp-gif",
    "-x", "500",
    "-y", "400"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

$returnCode = Execute-Program -ProgramName $exePath -ArgsList @(
    "$PSScriptRoot\..\images\马达.gif",
    "$PSScriptRoot\..\images\7808ce7d382f950d32732a52c8dc972d3d27a9a8.png",
    "-o",
    "$PSScriptRoot\mirage\gif-png",
    "-f", "20",
    "-d", "100"
)

if ($returnCode -ne 0) {
    exit $returnCode
}

# $returnCode = Execute-Program -ProgramName $exePath -ArgsList @(
#     "$PSScriptRoot\..\images\气气.gif",
#     "$PSScriptRoot\..\images\马达.gif",
#     "-o",
#     "$PSScriptRoot\mirage\m0",
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
                "$PSScriptRoot\..\images\气气.gif",
                "$PSScriptRoot\..\images\马达.gif",
                # "C:\Users\cyani\OneDrive\桌面\25f11f527c4feb6a8102ec40400c5683884e7e84.gif",
                # "C:\Users\cyani\OneDrive\桌面\7faaa4547bd5ff5dcbaba7127f4324b26105c26b.gif",
                # "-x", "800",
                # "-y", "540",
                # "-f", "60",
                # "-d", "40",
                "-o",
                # "$PSScriptRoot\mirage\${mode}-mashiro",
                "$PSScriptRoot\mirage\${mode}",
                "-m", "$mode",
                "-p", "12"
            )

            if ($returnCode -ne 0) {
                exit $returnCode
            }
        }
    }
}