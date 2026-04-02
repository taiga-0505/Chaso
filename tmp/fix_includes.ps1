
$files = @(
    'C:\Users\rea\source\repos\2025\Chaso\Chaso\project\Engine\Graphics\Model3D\ModelObject.cpp',
    'C:\Users\rea\source\repos\2025\Chaso\Chaso\project\Engine\Graphics\Sprite2D\Sprite2D.cpp'
)

foreach ($f in $files) {
    if (Test-Path $f) {
        $c = [System.IO.File]::ReadAllText($f)
        # PowerShell backtick character literal match
        $target = '`r`n'
        $c = $c.Replace($target, [Environment]::NewLine)
        [System.IO.File]::WriteAllText($f, $c)
    }
}
