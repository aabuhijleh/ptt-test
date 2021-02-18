#include <Windows.h>
#include <napi.h>

#include <iostream>
#include <sstream>
#include <string>
#include <thread>

// The TSFN is used to bridge the C++ world and the JS world
Napi::ThreadSafeFunction tsfn;

// A native thread with its own message loop is needed to attach low level
// keyboard hooks in order not to block the main thread
std::thread nativeThread;

// custom message sent to the native thread to signal it to quit
const UINT STOP_MESSAGE = WM_USER + 1;

// variable to store the HANDLE to the hook. Don't declare it anywhere else then
// globally or you will get problems since every function uses this variable.
HHOOK _hook;

// This struct contains the data received by the hook callback. As you see in
// the callback function it contains the thing you will need: vkCode = virtual
// key code.
KBDLLHOOKSTRUCT kbdStruct;

void ReleaseTSFN();
std::string ConvertKeyCodeToString(int key_stroke);

// Called from JS with a callback as an argument. It should call the JS callback
// from inside the native thread when reciving a keyboard input event
// jsCallback: (key: string, isKeyUp: boolean) => void
void Start(const Napi::CallbackInfo &info) {
  Napi::Env env = info.Env();

  std::cout << "Start!!!" << std::endl;

  // Stop if already running
  ReleaseTSFN();

  // Create a ThreadSafeFunction
  tsfn = Napi::ThreadSafeFunction::New(
      env,
      info[0].As<Napi::Function>(), // JavaScript function called asynchronously
      "Keyboard Events",            // Name
      0,                            // Unlimited queue
      1,                            // Only one thread will use this initially
      [](Napi::Env) {               // Finalizer used to clean threads up
        std::cout << "Clean up function called" << std::endl;

        // send a message to the native thread to quit
        PostThreadMessageA(GetThreadId(nativeThread.native_handle()),
                           STOP_MESSAGE, NULL, NULL);

        std::cout << "nativeThread.joinable() returned: "
                  << nativeThread.joinable() << std::endl;

        nativeThread.join();
      });

  nativeThread = std::thread([] {
    // This is the callback function. Consider it the event that is raised when,
    // in this case, a key is pressed or released.
    static auto HookCallback = [](int nCode, WPARAM wParam,
                                  LPARAM lParam) -> LRESULT {
      try {
        if (nCode >= 0 && tsfn) {
          // the action is valid: HC_ACTION and tsfn is not released.

          // lParam is the pointer to the struct containing the data needed,
          // so cast and assign it to kdbStruct.
          kbdStruct = *((KBDLLHOOKSTRUCT *)lParam);

          // call the JS callback with the key input value and type
          napi_status status =
              tsfn.BlockingCall([=](Napi::Env env, Napi::Function jsCallback) {
                jsCallback.Call(
                    {Napi::String::New(
                         env, ConvertKeyCodeToString(kbdStruct.vkCode)),
                     Napi::Boolean::New(env, wParam == WM_KEYUP ||
                                                 wParam == WM_SYSKEYUP)});
              });
          if (status != napi_ok) {
            std::cout << "Failed to execute BlockingCall!" << std::endl;
          }
        }
      } catch (...) {
        std::cerr << "something went wrong while handling the key event"
                  << std::endl;
      }

      // call the next hook in the hook chain. This is nessecary or your hook
      // chain will break and the hook stops
      return CallNextHookEx(_hook, nCode, wParam, lParam);
    };

    // Set the hook and set it to use the callback function above
    // WH_KEYBOARD_LL means it will set a low level keyboard hook. More
    // information about it at MSDN. The last 2 parameters are NULL, 0 because
    // the callback function is in the same thread and window as the function
    // that sets and releases the hook.
    if (!(_hook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0))) {
      std::cout << "Failed to install hook!" << std::endl;
    }

    // Create a message loop
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
      if (bRet == -1) {
        // handle the error and possibly exit
        std::cout << "some error occurred in the message loop" << std::endl;
      } else if (msg.message == STOP_MESSAGE) {
        PostQuitMessage(0);
      } else {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  });
}

// Called from JS to release the TSFN and stop listening to keyboard events
void Stop(const Napi::CallbackInfo &info) { ReleaseTSFN(); }

// Release the TSFN
void ReleaseTSFN() {
  if (tsfn) {
    napi_status status = tsfn.Release();
    if (status != napi_ok) {
      std::cout << "Failed to release the TSFN!" << std::endl;
    }
    tsfn = NULL;
  }
}

// Convert vkeyCode to string that matches these browser values
// https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/key/Key_Values
std::string ConvertKeyCodeToString(int key_stroke) {
  if ((key_stroke == 1) || (key_stroke == 2)) {
    return ""; // ignore mouse clicks
  }

  std::stringstream output;

  switch (key_stroke) {
  case VK_MENU:
  case VK_LMENU:
  case VK_RMENU:
    output << "Alt";
    break;
  case VK_LWIN:
  case VK_RWIN:
    output << "Meta";
    break;
  case VK_BACK:
    output << "Backspace";
    break;
  case VK_RETURN:
    output << "Enter";
    break;
  case VK_SPACE:
    output << "Spacebar";
    break;
  case VK_TAB:
    output << "Tab";
    break;
  case VK_SHIFT:
  case VK_LSHIFT:
  case VK_RSHIFT:
    output << "Shift";
    break;
  case VK_CONTROL:
  case VK_LCONTROL:
  case VK_RCONTROL:
    output << "Control";
    break;
  case VK_ESCAPE:
    output << "Escape";
    break;
  case VK_END:
    output << "End";
    break;
  case VK_HOME:
    output << "Home";
    break;
  case VK_LEFT:
    output << "ArrowLeft";
    break;
  case VK_UP:
    output << "ArrowUp";
    break;
  case VK_RIGHT:
    output << "ArrowRight";
    break;
  case VK_DOWN:
    output << "ArrowDown";
    break;
  case VK_CAPITAL:
    output << "CapsLock";
    break;
  case VK_PRIOR:
    output << "PageUp";
    break;
  case VK_NEXT:
    output << "PageDown";
    break;
  case VK_DELETE:
    output << "Delete";
    break;
  case VK_INSERT:
    output << "Insert";
    break;
  case VK_SNAPSHOT:
    output << "PrintScreen";
    break;
  case 190:
  case 110:
    output << ".";
    break;
  case 189:
  case 109:
    output << "-";
    break;
  default:
    if (key_stroke >= VK_F1 && key_stroke <= VK_F20) {
      output << "F" << (key_stroke - VK_F1 + 1);
    } else {
      // map virtual key according to keyboard layout
      char key =
          MapVirtualKeyExA(key_stroke, MAPVK_VK_TO_CHAR, GetKeyboardLayout(0));
      output << char(key);
    }
  }

  return output.str();
}

// Declare JS functions and map them to native functions
Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(Napi::String::New(env, "start"), Napi::Function::New(env, Start));
  exports.Set(Napi::String::New(env, "stop"), Napi::Function::New(env, Stop));
  return exports;
}

NODE_API_MODULE(push_to_talk, Init)
