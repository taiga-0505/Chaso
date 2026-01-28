#pragma warning(push)
#pragma warning(disable : 28251)
#include <Windows.h>
#pragma warning(pop)

#include "App.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  // COM 初期化
  HRESULT hrCom = CoInitializeEx(0, COINIT_MULTITHREADED);
  (void)hrCom;

  App app;
  if (!app.Init()) {
    CoUninitialize();
    return -1;
  }

  const int code = app.Run();

  app.Term();
  CoUninitialize();
  return code;
}
