#pragma once

// Windows 10
#define _WIN32_WINNT 0x0A00
#define NOMINMAX
#define NOMCX
#define NOSERVICE
#define NOHELP
#define WIN32_LEAN_AND_MEAN

#pragma warning( push )
#pragma warning(disable : 4365)
#pragma warning(disable : 4668)
#pragma warning(disable : 5039)
#include <windows.h>
#include <windowsx.h>
#ifdef _DEBUG
	#include <dxgidebug.h>
#endif
#include <DirectXMath.h>
#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <timeapi.h>
#pragma warning( pop )

//? Win32 Platform layer implementations, intended to be used with "WINAPI WinMain" only!
//? In order to provide distinction from Microsoft's WinApi functions "Win32" namespace is
//? provided for all custom platform layer functions and structures
//! DO NOT INCLUDE IT ENYWHERE ELSE THAN IN WIN32 ENTRY POINT COMPILATION UNIT FILE!
namespace Win32
{
	struct PlatformState
	{
		u64 totalSize;
		void *gameMemory;
	};

	//? Internal globals are never exposed to application layer directly, they are mostly data that
	//? needs to be shared with window CALLBACK function in Windows

	global_variable b32 isMainRunning = true;
	global_variable LARGE_INTEGER clockFrequency;

	internal auto GetWindowClientDimensions(HWND window)
	{
		struct Output
		{
			u32 w; u32 h;
		} out;
		RECT rect;
		GetClientRect(window, &rect);
		out.w = rect.right - rect.left;
		out.h = rect.bottom - rect.top;
		return out;
	}

	LRESULT CALLBACK mainWindowCallback(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
	{
		LRESULT output = 0;

		switch (message)
		{

		case WM_MENUCHAR:
		{
			output = MAKELRESULT(0, MNC_CLOSE);
		}
		break;

		case WM_DESTROY:
		{
			OutputDebugStringA("Window Destroyed\n");
			PostQuitMessage(0);
		}
		break;

		default:
		{
			output = DefWindowProc(window, message, wParam, lParam);
		}
		break;
		}
		return (output);
	}

	// Creates window for current process, cause of CS_OWNDC device context is assumed to not be shared with anyone
	internal HWND CreateMainWindow(const s32 w, const s32 h, const char *name)
	{
		HINSTANCE instance = nullptr;
		HWND mainWindow = nullptr;

		instance = GetModuleHandleA(nullptr);
		WNDCLASSEXA windowClass = {};

		windowClass.cbSize = sizeof(WNDCLASSEXA);
		windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		windowClass.lpfnWndProc = Win32::mainWindowCallback;
		windowClass.hInstance = instance;
		//windowClass.hIcon = LoadIcon(instance, "IDI_WINLOGO");
		windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
		windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 6);
		windowClass.lpszClassName = name;
		//windowClass.hIconSm = LoadIcon(windowClass.hInstance, "IDI_ICON");

		const s32 error = RegisterClassExA(&windowClass);
		GameAssert(error != 0 && "Class registration failed");

		RECT rc = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };
		AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW);
		const s32 winStyle = WS_EX_NOREDIRECTIONBITMAP | WS_SIZEBOX | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED | WS_SYSMENU;

		mainWindow = CreateWindowExA(
			WS_EX_APPWINDOW,
			name,
			name,
			winStyle,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			rc.right - rc.left,
			rc.bottom - rc.top,
			nullptr,
			nullptr,
			instance,
			nullptr);

		GameAssert(mainWindow != nullptr && "Window creation failed");

		ShowWindow(mainWindow, SW_SHOW);
		SetForegroundWindow(mainWindow);
		SetFocus(mainWindow);
		ShowCursor(true);
		ShowScrollBar(mainWindow, SB_BOTH, FALSE);
		return mainWindow;
	}

	// =========================================== MOUSE & KEYBOARD EVENTS HANDLING ==================================================
	internal void processKeyboardMouseEvent(GameKeyState *newState, const b32 isDown)
	{
		if (newState->wasDown != isDown)
		{
			newState->wasDown = isDown;
			++newState->halfTransCount;
		}
	}

	internal void processKeyboardMouseMsgs(MSG *msg, GameController *keyboardMouse)
	{
		LPARAM lParam = msg->lParam;
		WPARAM wParam = msg->wParam;

		switch (msg->message)
		{
			// Used for obtaining relative mouse movement and wheel without acceleration
		case WM_INPUT:
		{
			u32 size = sizeof(RAWINPUT);
			RAWINPUT raw[sizeof(RAWINPUT)];
			u32 copied = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, raw, &size, sizeof(RAWINPUTHEADER));

			if (copied != size)
			{
				//TODO: Fail handling from GetRawInputData()
				MessageBoxA(NULL, "Incorrect raw input data size!", "error", 0);
				break;
			}
			if (raw->header.dwType == RIM_TYPEMOUSE)
			{
				keyboardMouse->mouse.deltaX = raw->data.mouse.lLastX;
				keyboardMouse->mouse.deltaY = raw->data.mouse.lLastY;
				if (raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
				{
					keyboardMouse->mouse.deltaWheel = (*(s16 *)&raw->data.mouse.usButtonData);
				}
			}
		}
		break;

		case WM_MOUSEMOVE:
		{
			keyboardMouse->mouse.x = (s32)(GET_X_LPARAM(lParam) );
			keyboardMouse->mouse.y = (s32)(GET_Y_LPARAM(lParam) );
		}
		break;

		// LMB RMB MMB down messages
		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
		{
		}
		break;

		// LMB RMB MMB up messages
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
		{
		}
		break;

		// LMB RMB MMB double click messages
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		{
		}
		break;

		// Keyboard input messages
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			u32 vkCode = (u32)wParam;
			b32 wasDown = TestBit(lParam, 30) != 0;
			b32 isDown = TestBit(lParam, 31) == 0;
			//TODO: Consider binding from file?
			if (wasDown != isDown)
			{
				if (vkCode == 'W')
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->moveUp, isDown);
				}
				else if (vkCode == 'S')
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->moveDown, isDown);
				}
				else if (vkCode == 'A')
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->moveLeft, isDown);
				}
				else if (vkCode == 'D')
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->moveRight, isDown);
				}
				else if (vkCode == 'Q')
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->action1, isDown);
				}
				else if (vkCode == 'E')
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->action2, isDown);
				}
				else if (vkCode == 'Z')
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->action3, isDown);
				}
				else if (vkCode == 'X')
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->action4, isDown);
				}
				else if (vkCode == VK_ESCAPE)
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->back, isDown);
				}
				else if (vkCode == VK_RETURN)
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->start, isDown);
				}
				else if (vkCode == VK_SPACE)
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->actionFire, isDown);
				}
				else if (vkCode == VK_UP)
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->action1, isDown);
				}
				else if (vkCode == VK_DOWN)
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->action2, isDown);
				}
				else if (vkCode == VK_LEFT)
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->moveLeft, isDown);
				}
				else if (vkCode == VK_RIGHT)
				{
					Win32::processKeyboardMouseEvent(&keyboardMouse->moveRight, isDown);
				}
			}
			b32 altWasDown = TestBit(lParam, 29);
			if ((vkCode == VK_F4) && altWasDown)
			{
				Win32::isMainRunning = false;
			}
			break;
		}
		default:
		{
		}
		break;
		}
	}

	internal void RegisterMouseForRawInput(HWND window = nullptr)
	{
		RAWINPUTDEVICE rawDevices[1];
		// Mouse registering info
		rawDevices[0].usUsagePage = 0x01;
		rawDevices[0].usUsage = 0x02;
		rawDevices[0].dwFlags = 0;
		rawDevices[0].hwndTarget = window;

		if (RegisterRawInputDevices(rawDevices, 1, sizeof(rawDevices[0])) == FALSE)
		{
			//TODO: Proper handling in case of failure to register mouse and/or keyboard
			MessageBoxA(NULL, "Could not register mouse and/or keyboard for raw input", "error", 0);
			GameAssert(0);
		}
	}

	internal s32 getMonitorFrequency()
	{
		DEVMODEA devInfo = {};
		devInfo.dmSize = sizeof(DEVMODE);
		EnumDisplaySettingsA(0, ENUM_CURRENT_SETTINGS, &devInfo);
		return (s32)devInfo.dmDisplayFrequency;
	}
	
	// ================================================= DEBUG INTERNAL FUNCTIONS ==========================================================

#if GAME_INTERNAL
	DebugFileOutput debugReadFile(const char *fileName)
	{
		HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
		auto d = defer([&]
		{
			CloseHandle(fileHandle);
		});
		DebugFileOutput out = {};

		if (fileHandle != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER fileSize;

			if (GetFileSizeEx(fileHandle, &fileSize))
			{
				u32 fileSize32 = trunc_u64_to_u32((u64)fileSize.QuadPart);
				out.data = VirtualAlloc(0, fileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

				if (out.data)
				{
					DWORD bytesRead;
					if (ReadFile(fileHandle, out.data, fileSize32, &bytesRead, 0) && (fileSize32 == bytesRead))
					{
						out.dataSize = fileSize32;
					}
					else
					{
						VirtualFree(out.data, 0, MEM_RELEASE);
						out.data = nullptr;
						//TODO: Log
					}
				}
			}
		}
		else
		{
			//TODO: Log
		}

		return (out);
	}

	b32 debugWriteFile(const char *fileName, void *memory, u32 memSize)
	{
		b32 isSuccessful = false;
		HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
		auto d = defer([&]
		{
			CloseHandle(fileHandle);
		});

		if (fileHandle != INVALID_HANDLE_VALUE)
		{
			DWORD bytesWritten;
			if (WriteFile(fileHandle, memory, memSize, &bytesWritten, 0))
			{
				isSuccessful = (bytesWritten == memSize);
			}
			else
			{
				//TODO: Log
			}
		}
		else
		{
			//TODO: Log
		}

		return isSuccessful;
	}

	void debugFreeFileMemory(void *memory)
	{
		if (memory != nullptr)
		{
			VirtualFree(memory, 0, MEM_RELEASE);
		}
	}
#endif


} // namespace Win32