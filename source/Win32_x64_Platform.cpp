//? This Translation Unit provides data and services (declared by application) from OS to a game/app layer
//? Architecture style is inspired from "Handmade Hero" series by Casey Muratori:
//? Application is treated as a service that OS fulfills,
//? instead of abstracting platform code and handling it as kind of "Virtual OS"
//? In case of porting to a different platform, this is the ONLY file you need to change

#include <stdio.h> //temporary
#include <omp.h>

#include "Utils.hpp"
#include "GameAsserts.hpp"
#include "Game_Services.hpp"
#include "Win32_x64_Platform.hpp"
#include "DxManagment.hpp"
#include "Allocators.hpp"
#include "Views.hpp"
#include "Math.hpp"

#ifdef _DEBUG
#else

#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	SYSTEM_INFO windows_info{};
	GetSystemInfo(&windows_info);
	auto cores_count = windows_info.dwNumberOfProcessors;
	AlwaysAssert(windows_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 
	             && "This is not a 64-bit OS!");
	
	Win32::Platform_Clock clock = Win32::clock_create();
	UINT schedulerGranularity = 1;
	b32 schedulerError = (timeBeginPeriod(schedulerGranularity) == TIMERR_NOERROR);
	
	Win32::register_mouse_raw_input();
	HWND win_handle = Win32::create_window(1280, 720, "Raster");
	auto&& [width, height] = Win32::get_window_client_dims(win_handle);
	
	Alloc_Arena global_memory
	{
		.max_size = GiB(2),
		.base = (byte*)VirtualAlloc(0, GiB(2), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)
	};
	AlwaysAssert(global_memory.base && "Failed to allocate memory from Windows");
	
	omp_set_max_active_levels(2);
	omp_set_num_threads(cores_count);
	
	DX_Machine machine{};
	DX_Context context{};
	ID3D11Texture2D *back_buffer = nullptr;
	ID3D11Texture2D *cpu_buffer = nullptr;
	HRESULT hr;
	
	if (!dx_init_resources(&machine, &context, win_handle))
		return 0;
	
	Game_Input gameInputBuffer[2] = {};
	Game_Input *newInputs = &gameInputBuffer[0];
	Game_Input *oldInputs = &gameInputBuffer[1];
	
	while (Win32::g_is_running)
	{
		u64 tick_start = Win32::get_performance_ticks();
		static u32 counter;
		
		Game_Controller *oldKeyboardMouseController = get_game_controller(oldInputs, 0);
		Game_Controller *newKeyboardMouseController = get_game_controller(newInputs, 0);
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
			Win32::process_keyboard_mouse_msg(&msg, newKeyboardMouseController);
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				Win32::g_is_running = false;
				break;
			}
		}
		
		auto&& [new_width, new_height] = Win32::get_window_client_dims(win_handle);
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
			// Stop GPU access to the CPU-pixel buffer data
			hr = context.imm_context->Map((ID3D11Resource *)cpu_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
			AssertHR(hr);
			
			// Drawing CPU-pixels
			//TODO: Move drawing code out to application layer
			//TODO: Consider memcpy from/to additional buffer instead of directly from/to mapped, basedo on
			//		https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map
			// 		To never actually read directly from mapped resource
			u32 *pixel = (u32 *)mapped.pData;
			#pragma omp parallel shared(counter, pixel) // seems to be "stuttering" sometimes
			{
				#pragma omp for
				for (u32 xy = 0; xy < width*height; ++xy) 
				{
//					u32 x = xy % width;
//					u32 y = xy / width;
//		
//					u32 r = (((x + counter) % width) * 255 / width);;
//					u32 g = (y * 255 / height);
//					u32 b = 0;
					
					
					pixel[xy] = (b << 16) | (g << 8) | (r << 0);
				}
			}

			counter++;
			// Allow GPU access to the CPU-pixel buffer data
			context.imm_context->Unmap((ID3D11Resource *)cpu_buffer, 0);
			context.imm_context->CopyResource((ID3D11Resource *)back_buffer, (ID3D11Resource *)cpu_buffer);
		}
		
		hr = machine.swap_chain->Present(0,0);
		AssertHR(hr);
		f64 frame_time_ms = Win32::get_elapsed_ms_here(clock, tick_start);
		
		if (counter % 100 == 0)
		{
			char time_buf[32];
			sprintf_s(time_buf, sizeof(time_buf), "%.3lf ms", frame_time_ms);
			SetWindowText(win_handle, time_buf);
		}
	}
	
	UnregisterClassA("Raster", GetModuleHandle(nullptr));
	return 0;
}