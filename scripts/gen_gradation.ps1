Add-Type -AssemblyName System.Drawing
$w = 256
$h = 4
$bmp = New-Object System.Drawing.Bitmap($w, $h)
for ($x = 0; $x -lt $w; $x++) {
    $t = [float]$x / ($w - 1)
    $alpha = [int](255 * (1.0 - $t))
    for ($y = 0; $y -lt $h; $y++) {
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($alpha, 255, 255, 255))
    }
}
$outPath = Join-Path $PSScriptRoot "..\project\Resources\Particle\gradationLine.png"
$bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "Created gradationLine.png at $outPath"
