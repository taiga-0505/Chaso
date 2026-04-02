
$pathM = 'C:\Users\rea\source\repos\2025\Chaso\Chaso\project\Engine\Graphics\Model3D\ModelObject.cpp'
$contentM = [System.IO.File]::ReadAllText($pathM)
$newContentM = $contentM.Replace('ModelObject::~ModelObject() {', 'ModelObject::~ModelObject() {' + [Environment]::NewLine + '  if (!filePath_.empty()) {' + [Environment]::NewLine + '    std::cout << "[Model] Unloaded: " << filePath_ << std::endl;' + [Environment]::NewLine + '  }')
[System.IO.File]::WriteAllText($pathM, $newContentM)

$pathS = 'C:\Users\rea\source\repos\2025\Chaso\Chaso\project\Engine\Graphics\Sprite2D\Sprite2D.cpp'
$contentS = [System.IO.File]::ReadAllText($pathS)
$newContentS = $contentS.Replace('Sprite2D::~Sprite2D() { Release(); }', 'Sprite2D::~Sprite2D() {' + [Environment]::NewLine + '  if (!filePath_.empty()) {' + [Environment]::NewLine + '    std::cout << "[Sprite] Unloaded: " << filePath_ << std::endl;' + [Environment]::NewLine + '  }' + [Environment]::NewLine + '  Release();' + [Environment]::NewLine + '}')
[System.IO.File]::WriteAllText($pathS, $newContentS)
