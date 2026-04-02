
$pathM = 'C:\Users\rea\source\repos\2025\Chaso\Chaso\project\Engine\Graphics\Model3D\ModelObject.cpp'
$contentM = [System.IO.File]::ReadAllText($pathM)
$newContentM = $contentM.Replace('std::cout << "[Model] Unloaded: " << filePath_ << std::endl;', 'Log::Print("[Model] Unloaded: " + filePath_);')
if ($newContentM -notlike '*Common/Log/Log.h*') {
    $newContentM = $newContentM.Replace('#include "ModelObject.h"', '#include "ModelObject.h"`r`n#include "Common/Log/Log.h"')
}
[System.IO.File]::WriteAllText($pathM, $newContentM)

$pathS = 'C:\Users\rea\source\repos\2025\Chaso\Chaso\project\Engine\Graphics\Sprite2D\Sprite2D.cpp'
$contentS = [System.IO.File]::ReadAllText($pathS)
$newContentS = $contentS.Replace('std::cout << "[Sprite] Unloaded: " << filePath_ << std::endl;', 'Log::Print("[Sprite] Unloaded: " + filePath_);')
if ($newContentS -notlike '*Common/Log/Log.h*') {
    $newContentS = $newContentS.Replace('#include "Sprite2D.h"', '#include "Sprite2D.h"`r`n#include "Common/Log/Log.h"')
}
[System.IO.File]::WriteAllText($pathS, $newContentS)
