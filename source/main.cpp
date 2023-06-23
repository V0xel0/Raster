#include "Utils.hpp"
#include "GameAsserts.hpp"
#include "GameService.hpp"
#include "Win32Platform.hpp"
#include "DxManagment.h"

#include "Allocators.hpp"
#include "Views.hpp"

#ifdef _DEBUG
#else

#endif


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	AlwaysAssert(sizeof(void*) == 8 && "This is not a 64-bit OS!");
	UINT schedulerGranularity = 1;
	b32 schedulerError = (timeBeginPeriod(schedulerGranularity) == TIMERR_NOERROR);

	Win32::RegisterMouseForRawInput();
	HWND win_handle = Win32::CreateMainWindow(1280, 720, "Raster");
	auto&& [width, height] = Win32::GetWindowClientDimensions(win_handle);
	
	Alloc_Arena global_memory
	{
		.max_size = GiB(2),
		.base = (byte*)VirtualAlloc(0, GiB(2), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
	};
	AlwaysAssert(global_memory.base && "Failed to allocate memory from Windows");
	
	DX_Machine machine{};
	DX_Context context{};
	ID3D11Texture2D *back_buffer = nullptr;
	ID3D11Texture2D *cpu_buffer = nullptr;
	HRESULT hr;
	
	if (!dx_init_resources(&machine, &context, win_handle))
		return 0;
	
	GameInput gameInputBuffer[2] = {};
	GameInput *newInputs = &gameInputBuffer[0];
	GameInput *oldInputs = &gameInputBuffer[1];
	
	while (Win32::isMainRunning)
	{
		GameController *oldKeyboardMouseController = getGameController(oldInputs, 0);
		GameController *newKeyboardMouseController = getGameController(newInputs, 0);
		*newKeyboardMouseController = {};
		newKeyboardMouseController->isConnected = true;
		for (u32 i = 0; i < array_count_32(newKeyboardMouseController->buttons); ++i)
		{
			newKeyboardMouseController->buttons[i].wasDown = oldKeyboardMouseController->buttons[i].wasDown;
			newKeyboardMouseController->mouse.x = oldKeyboardMouseController->mouse.x;
			newKeyboardMouseController->mouse.y = oldKeyboardMouseController->mouse.y;
		}

		MSG msg = {};
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			Win32::processKeyboardMouseMsgs(&msg, newKeyboardMouseController);
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				Win32::isMainRunning = false;
				break;
			}
		}
		
		auto&& [new_width, new_height] = Win32::GetWindowClientDimensions(win_handle);
		if ((width != new_width || height != new_height) || !cpu_buffer)
		{
			if (cpu_buffer)
			{
				back_buffer->Release();
				cpu_buffer->Release();
				back_buffer = nullptr;
				cpu_buffer = nullptr;
			}
			
			width = new_width;
			height = new_height;
			
			if (width && height)
			{
				hr = machine.swap_chain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
				AssertHR(hr);
				hr =  machine.swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&back_buffer);
				AssertHR(hr);
					
				D3D11_TEXTURE2D_DESC tex_desc
				{
					.Width = width,
					.Height = height,
					.MipLevels = 1,
					.ArraySize = 1,
					.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
					.SampleDesc = { 1, 0 },
					.Usage = D3D11_USAGE_DYNAMIC,
					.BindFlags = D3D11_BIND_SHADER_RESOURCE,
					.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
				};

				hr = machine.device->CreateTexture2D(&tex_desc, nullptr, &cpu_buffer);
				AssertHR(hr);
			}
		}
		
		if (width && height)
		{
			D3D11_MAPPED_SUBRESOURCE mapped;

			hr = context.imm_context->Map((ID3D11Resource *)cpu_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			AssertHR(hr);
			
			// Drawing pixels
			static u32 counter;
			u32 *pixel = (u32 *)mapped.pData;
			
			for (u32 y = 0; y < height; y++)
			{
				for (u32 x = 0; x < width; x++)
				{
					u32 r = (((x + counter) % width) * 255 / width);
					u32 g = (y * 255 / height);
					u32 b = 0;
					pixel[y * width + x] = (b << 16) | (g << 8) | (r << 0);
				}
			}
			counter++;
			
			context.imm_context->Unmap((ID3D11Resource *)cpu_buffer, 0);
			context.imm_context->CopyResource((ID3D11Resource *)back_buffer, (ID3D11Resource *)cpu_buffer);
		}
		
		hr = machine.swap_chain->Present(0,0);
		AssertHR(hr);
	}
	
	UnregisterClassA("Raster", GetModuleHandle(nullptr)); //? Do we need that?
	return 0;
}