# Draws the sandbox's textures into assets/textures/. They're procedural (lines and rectangles,
# no third-party art), so they can be regenerated at any time:
#
#   powershell -ExecutionPolicy Bypass -File scripts\make-textures.ps1
#
# Windows only: it uses .NET's System.Drawing. The PNGs it writes are committed, so other
# platforms never need to run it.

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$outDir = Join-Path $PSScriptRoot "..\assets\textures"
New-Item -ItemType Directory -Force $outDir | Out-Null

function New-Color([int]$r, [int]$g, [int]$b) { [System.Drawing.Color]::FromArgb(255, $r, $g, $b) }

# crate.png: a wooden crate side, 256x256. Vertical planks with a little grain, a darker frame,
# and a diagonal brace.
$size = 256
$bmp = New-Object System.Drawing.Bitmap $size, $size
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear((New-Color 170 118 66))

# Planks: alternate two shades, with darker seams between them.
$plank = 32
for ($x = 0; $x -lt $size; $x += $plank) {
    $shade = if (($x / $plank) % 2 -eq 0) { New-Color 176 122 68 } else { New-Color 160 110 60 }
    $g.FillRectangle((New-Object System.Drawing.SolidBrush $shade), $x, 0, $plank, $size)
    $g.DrawLine((New-Object System.Drawing.Pen (New-Color 96 62 30), 2), $x, 0, $x, $size)
}

# Grain: thin wavy lines, the same every run (fixed random seed).
$random = New-Object System.Random 7
$grainPen = New-Object System.Drawing.Pen (New-Color 140 94 50), 1
for ($i = 0; $i -lt 60; $i++) {
    $x = $random.Next(0, $size)
    $y0 = $random.Next(0, $size - 40)
    $length = $random.Next(20, 90)
    $points = @()
    for ($t = 0; $t -le $length; $t += 6) { $points += New-Object System.Drawing.PointF ($x + 1.5 * [Math]::Sin($t / 7.0)), ($y0 + $t) }
    $g.DrawLines($grainPen, [System.Drawing.PointF[]]$points)
}

# The frame and the diagonal brace, with a dark outline so they stand out.
$frame = 28
$framePen = New-Object System.Drawing.Pen (New-Color 120 80 40), $frame
$g.DrawRectangle($framePen, $frame / 2, $frame / 2, $size - $frame, $size - $frame)
$bracePen = New-Object System.Drawing.Pen (New-Color 128 86 44), 26
$g.DrawLine($bracePen, $frame, $size - $frame, $size - $frame, $frame)
$outlinePen = New-Object System.Drawing.Pen (New-Color 70 44 20), 3
$g.DrawRectangle($outlinePen, 1, 1, $size - 3, $size - 3)
$g.DrawRectangle($outlinePen, $frame, $frame, $size - 2 * $frame, $size - 2 * $frame)

# Nails in the frame's corners.
$nailBrush = New-Object System.Drawing.SolidBrush (New-Color 60 60 64)
foreach ($p in @(@(14, 14), @(242, 14), @(14, 242), @(242, 242))) { $g.FillEllipse($nailBrush, $p[0] - 4, $p[1] - 4, 8, 8) }

$g.Dispose()
$bmp.Save((Join-Path $outDir "crate.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

# checker.png: the floor, 64x64. Two by two light and dark squares, each with a thin dark line
# along two edges. Fine lines like these shimmer at a distance without mipmaps, which makes the
# floor a good test of them.
$size = 64
$bmp = New-Object System.Drawing.Bitmap $size, $size
$g = [System.Drawing.Graphics]::FromImage($bmp)
$light = New-Object System.Drawing.SolidBrush (New-Color 190 190 196)
$dark = New-Object System.Drawing.SolidBrush (New-Color 92 92 100)
$g.FillRectangle($light, 0, 0, 32, 32)
$g.FillRectangle($dark, 32, 0, 32, 32)
$g.FillRectangle($dark, 0, 32, 32, 32)
$g.FillRectangle($light, 32, 32, 32, 32)
$linePen = New-Object System.Drawing.Pen (New-Color 40 40 46), 1
foreach ($c in @(0, 32)) {
    $g.DrawLine($linePen, $c, 0, $c, $size)
    $g.DrawLine($linePen, 0, $c, $size, $c)
}
$g.Dispose()
$bmp.Save((Join-Path $outDir "checker.png"), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Write-Host "Wrote crate.png and checker.png to $outDir"
