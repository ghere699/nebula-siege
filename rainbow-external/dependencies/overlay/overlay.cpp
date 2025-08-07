#include "includes.hpp"
#include <chrono>

// Chrono literals
using namespace std::chrono_literals;

bool c_overlay::setup_overlay()
{
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	game_window = FindWindowA("R6Game", nullptr);

	if (!game_window)
		return false;

	if (!create_imgui_backend())
		return false;

	if (!create_imgui())
		return false;

	return true;
}


void CleanupRenderTarget()
{
	if (c_overlay::target_view) { c_overlay::target_view->Release(); c_overlay::target_view = NULL; }
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (c_overlay::swap_chain) { c_overlay::swap_chain->Release(); c_overlay::swap_chain = NULL; }
	if (c_overlay::device_context) { c_overlay::device_context->Release(); c_overlay::device_context = NULL; }
	if (c_overlay::device) { c_overlay::device->Release(); c_overlay::device = NULL; }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	c_overlay::swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	c_overlay::device->CreateRenderTargetView(pBackBuffer, NULL, &c_overlay::target_view);
	pBackBuffer->Release();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (c_overlay::device != NULL && wParam != SIZE_MINIMIZED)
		{
			CleanupRenderTarget();
			c_overlay::swap_chain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
			CreateRenderTarget();
		}
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

bool CreateDeviceD3D(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &c_overlay::swap_chain, &c_overlay::device, &featureLevel, &c_overlay::device_context) != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

bool c_overlay::create_imgui_backend()
{
	// Create application window
	wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "ImGui Overlay", NULL };
	::RegisterClassEx(&wc);

	int screenWidth = GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// Create a borderless, topmost, layered window.
	// WS_EX_TRANSPARENT is initially set based on menu state later.
	HWND hwnd = ::CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT, // Use WS_EX_LAYERED for transparency
		wc.lpszClassName,
		"Dear ImGui DirectX11 Overlay",
		WS_POPUP,
		0, 0, screenWidth, screenHeight,
		NULL, NULL, wc.hInstance, NULL
	);

	if (hwnd == NULL) {
		MessageBox(NULL, "Window Creation Failed!", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	overlay_window = hwnd;

	// *** THE FIX IS HERE ***
	// We don't use DwmExtendFrameIntoClientArea. Instead, we use a color key.
	// This tells Windows that any pixel with the color RGB(0,0,0) will be transparent.
	SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);

	// Initialize Direct3D
	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return false;
	}

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	LONG style = GetWindowLong(overlay_window, GWL_EXSTYLE);
	style |= WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
	SetWindowLong(hwnd, GWL_EXSTYLE, style);
	return true;
}


bool c_overlay::create_imgui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(overlay_window);
	ImGui_ImplDX11_Init(device, device_context);

	return true;
}

void c_overlay::start_scene()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void c_overlay::end_scene()
{
	ImGui::Render();

	const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black background

	device_context->OMSetRenderTargets(1, &target_view, NULL);
	device_context->ClearRenderTargetView(target_view, clear_color);

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	swap_chain->Present(1, 0);
}

bool c_overlay::clean_context()
{
	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow(overlay_window);
	::UnregisterClass(wc.lpszClassName, wc.hInstance);

	// 2. Uninitialize COM
	CoUninitialize();
	return true;
}

bool c_overlay::setup_render()
{
	setup_overlay();

	bool done = false;

	while (!done)
	{
		while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				done = true;
		}

		if (done)
			break;

		start_scene();

		ImDrawList* draw = ImGui::GetBackgroundDrawList();

		if (!cache.camera)
			goto IMGUI_END;

		{
			std::lock_guard lock(actor_list_mutex);

			for (c_actor* actor : cache.actor_list)
			{
				
				if (!actor)
					continue;

				if (!actor->is_actor_player())
					continue;

				c_actor::actor_status status = actor->get_actor_status();
				if (status != c_actor::actor_status::VALID)
					continue;

				vector3_t origin = actor->get_actor_position();
				if (origin.empty())
					continue;

				auto [min, max] = actor->get_actor_bounds();
				if (min.empty() || max.empty())
					continue;

				// we clamp the bounds due to an issue where the player would walk and the bounds for extend very far
				const float max_extent = 1.0f;

				min.x = std::clamp(min.x, -max_extent, max_extent);
				min.y = std::clamp(min.y, -max_extent, max_extent);
				max.x = std::clamp(max.x, -max_extent, max_extent);
				max.y = std::clamp(max.y, -max_extent, max_extent);

				const float extent_x = std::abs(max.x - min.x);
				const float extent_y = std::abs(max.y - min.y);
				const float extent_z = std::abs(max.z - min.z);
				const float volume = extent_x * extent_y * extent_z;

				// my current actor list draws every rigid body on the player, we have these checks in place to ensure we only draw the player box only
			const float min_extent_threshold = 0.1f;
				if (extent_x < min_extent_threshold || extent_y < min_extent_threshold || extent_z < min_extent_threshold ||
					extent_x > 3.0f || extent_y > 3.0f || extent_z > 3.0f ||
					extent_x != extent_x || extent_y != extent_y || extent_z != extent_z ||
					extent_x == 0.0f || extent_y == 0.0f || extent_z == 0.0f ||
					volume <= 1.0f || volume != volume || volume == 8.0f)
				{
					continue;
				}

				vector3_t corners[8] = {
					origin + vector3_t(min.x, min.y, min.z),
					origin + vector3_t(min.x, max.y, min.z),
					origin + vector3_t(max.x, max.y, min.z),
					origin + vector3_t(max.x, min.y, min.z),
					origin + vector3_t(min.x, min.y, max.z),
					origin + vector3_t(min.x, max.y, max.z),
					origin + vector3_t(max.x, max.y, max.z),
					origin + vector3_t(max.x, min.y, max.z),
				};

				vector2_t screen[8];
				bool valid[8];
				int valid_count = 0;

				for (int i = 0; i < 8; ++i)
				{
					screen[i] = cache.camera->get_screen_position(corners[i]);
					valid[i] = (screen[i].x != 0.f && screen[i].y != 0.f);

					if (valid[i])
						valid_count++;
				}

				if (valid_count == 0)
					continue;

				ImU32 color = IM_COL32(0, 255, 0, 255);

				for (int i = 0; i < 4; ++i)
				{
					if (valid[i] && valid[(i + 1) % 4])
						draw->AddLine(ImVec2(screen[i].x, screen[i].y), ImVec2(screen[(i + 1) % 4].x, screen[(i + 1) % 4].y), color);

					if (valid[i + 4] && valid[((i + 1) % 4) + 4])
						draw->AddLine(ImVec2(screen[i + 4].x, screen[i + 4].y), ImVec2(screen[((i + 1) % 4) + 4].x, screen[((i + 1) % 4) + 4].y), color);

					if (valid[i] && valid[i + 4])
						draw->AddLine(ImVec2(screen[i].x, screen[i].y), ImVec2(screen[i + 4].x, screen[i + 4].y), color);
				}
			}
			Sleep(5);

		}
		IMGUI_END:
		end_scene();
	}

	clean_context();
	return true;
}

c_overlay overlay;