
$pathS = 'C:\Users\rea\source\repos\2025\Chaso\Chaso\project\Engine\Graphics\Sound\Sound.cpp'
$c = [System.IO.File]::ReadAllText($pathS)
$c = $c.Replace('`r`n', [Environment]::NewLine)
[System.IO.File]::WriteAllText($pathS, $c)
