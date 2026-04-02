
$pathS = 'C:\Users\rea\source\repos\2025\Chaso\Chaso\project\Engine\Graphics\Sound\Sound.cpp'
$contentS = [System.IO.File]::ReadAllText($pathS)

# Add include
if ($contentS -notlike '*Common/Log/Log.h*') {
    $contentS = $contentS.Replace('#include "Sound.h"', '#include "Sound.h"`r`n#include "Common/Log/Log.h"')
}

# Add logging to Initialize
$oldInit = '  soundData = SoundLoadAudio(filename);'
$newInit = '  soundData = SoundLoadAudio(filename);`r`n  filePath_ = filename;`r`n  Log::Print("[Sound] Loaded: " + filePath_);'
$contentS = $contentS.Replace($oldInit, $newInit)

# Add logging to Destructor
$oldDest = 'Sound::~Sound() {'
$newDest = 'Sound::~Sound() {`r`n  if (!filePath_.empty()) {`r`n    Log::Print("[Sound] Unloaded: " + filePath_);`r`n  }'
$contentS = $contentS.Replace($oldDest, $newDest)

[System.IO.File]::WriteAllText($pathS, $contentS)
