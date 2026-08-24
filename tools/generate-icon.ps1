param(
    [string]$IcoPath = (Join-Path $PSScriptRoot '..\assets\app-icon.ico'),
    [string]$PngPath = (Join-Path $PSScriptRoot '..\assets\app-icon.png')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-RoundedRectanglePath {
    param(
        [Drawing.RectangleF]$Rectangle,
        [single]$Radius
    )

    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $diameter = $Radius * 2
    $path.AddArc($Rectangle.X, $Rectangle.Y, $diameter, $diameter, 180, 90)
    $path.AddArc($Rectangle.Right - $diameter, $Rectangle.Y,
        $diameter, $diameter, 270, 90)
    $path.AddArc($Rectangle.Right - $diameter,
        $Rectangle.Bottom - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($Rectangle.X, $Rectangle.Bottom - $diameter,
        $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

function New-MicrophoneBitmap {
    param([int]$Size)

    $bitmap = [Drawing.Bitmap]::new(
        $Size, $Size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $scale = $Size / 256.0
    $graphics.ScaleTransform($scale, $scale)

    $charcoal = [Drawing.Color]::FromArgb(255, 38, 45, 54)
    $light = [Drawing.Color]::FromArgb(255, 246, 248, 250)
    $green = [Drawing.Color]::FromArgb(255, 34, 197, 94)
    $body = New-RoundedRectanglePath ([Drawing.RectangleF]::new(76, 22, 104, 154)) 51
    $lightBrush = [Drawing.SolidBrush]::new($light)
    $greenBrush = [Drawing.SolidBrush]::new($green)
    $outline = [Drawing.Pen]::new($charcoal, 14)
    $outline.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    $stand = [Drawing.Pen]::new($charcoal, 14)
    $stand.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $stand.EndCap = [Drawing.Drawing2D.LineCap]::Round

    $graphics.FillPath($lightBrush, $body)
    $graphics.SetClip($body)
    $level = [Drawing.Drawing2D.GraphicsPath]::new()
    $level.StartFigure()
    $level.AddLine(70, 108, 70, 190)
    $level.AddLine(186, 190, 186, 101)
    $level.AddBezier(186, 101, 150, 78, 121, 120, 70, 104)
    $level.CloseFigure()
    $graphics.FillPath($greenBrush, $level)
    $graphics.ResetClip()
    $graphics.DrawPath($outline, $body)

    $graphics.DrawArc($stand, 48, 64, 160, 142, 0, 180)
    $graphics.DrawLine($stand, 128, 206, 128, 231)
    $graphics.DrawLine($stand, 91, 231, 165, 231)

    $level.Dispose()
    $stand.Dispose()
    $outline.Dispose()
    $greenBrush.Dispose()
    $lightBrush.Dispose()
    $body.Dispose()
    $graphics.Dispose()
    return $bitmap
}

$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$images = [Collections.Generic.List[byte[]]]::new()
foreach ($size in $sizes) {
    $bitmap = New-MicrophoneBitmap $size
    $stream = [IO.MemoryStream]::new()
    $bitmap.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
    $images.Add($stream.ToArray())
    if ($size -eq 256) {
        $bitmap.Save($PngPath, [Drawing.Imaging.ImageFormat]::Png)
    }
    $stream.Dispose()
    $bitmap.Dispose()
}

$file = [IO.File]::Open($IcoPath, [IO.FileMode]::Create,
    [IO.FileAccess]::Write, [IO.FileShare]::None)
$writer = [IO.BinaryWriter]::new($file)
$writer.Write([uint16]0)
$writer.Write([uint16]1)
$writer.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($index = 0; $index -lt $sizes.Count; $index++) {
    $size = $sizes[$index]
    $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
    $writer.Write([byte]$(if ($size -eq 256) { 0 } else { $size }))
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]32)
    $writer.Write([uint32]$images[$index].Length)
    $writer.Write([uint32]$offset)
    $offset += $images[$index].Length
}
foreach ($image in $images) {
    $writer.Write($image)
}
$writer.Dispose()
$file.Dispose()

Write-Output "Generated $IcoPath and $PngPath"
