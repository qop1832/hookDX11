// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "DllMain.h"


typedef HRESULT(__fastcall* IDXGISwapChainPresent)(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
typedef HRESULT(__fastcall* IDXGISwapChainResizeBuffers)(IDXGISwapChain* pChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT Flags);
typedef void(__stdcall* ID3D11DrawIndexed)(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);


// 定义一些全局变量，用于存储 DirectX 设备、上下文和交换链
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;
static IDXGISwapChain* pSwapChain = NULL;

DWORD_PTR* pDeviceContextVTable = NULL; // 用于存储设备上下文的虚表
ID3D11DrawIndexed fnID3D11DrawIndexed;  // 定义 DrawIndexed 函数指针
HWND window = nullptr;                  // 保存窗口句柄
static WNDPROC OriginalWndProcHandler = nullptr; // 保存原始窗口过程
HMODULE ModuleInUse;
bool g_PresentHooked = false;  // 标记 Present 是否已钩住
BOOL g_bInitialised = false;   // 标记是否初始化完成
static bool renderview_lost = true;   // 渲染目标丢失...
IDXGISwapChainPresent fnIDXGISwapChainPresent; // Present 函数指针
IDXGISwapChainResizeBuffers fnIDXGISwapChainResizeBuffers;


static int count = 0;


enum IDXGISwapChainvTable //for dx10 / dx11
{
	QUERY_INTERFACE,
	ADD_REF,
	RELEASE,
	SET_PRIVATE_DATA,
	SET_PRIVATE_DATA_INTERFACE,
	GET_PRIVATE_DATA,
	GET_PARENT,
	GET_DEVICE,
	PRESENT,
	GET_BUFFER,
	SET_FULLSCREEN_STATE,
	GET_FULLSCREEN_STATE,
	GET_DESC,
	RESIZE_BUFFERS,
	RESIZE_TARGET,
	GET_CONTAINING_OUTPUT,
	GET_FRAME_STATISTICS,
	GET_LAST_PRESENT_COUNT
};

// 窗口消息处理回调函数
LRESULT CALLBACK hWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	POINT mPos;
	GetCursorPos(&mPos);
	ScreenToClient(window, &mPos);

	// 处理按键抬起事件
	if (uMsg == WM_KEYUP)
	{
		if (wParam == VK_INSERT)
		{
			std::cout << "WM_KEYUP -> VK_INSERT" << std::endl;
		}
	}

	// 处理自定义消息 0x1832
	if (uMsg == 0x1832)
	{
		auto base = GetModuleHandleW(NULL);
		char* code = (char*)"AccountLogin.UI.AccountEditBox:SetText('123@163.com')";
		OutputDebugStringA("WOW.1");
		try
		{
			reinterpret_cast<void(__fastcall*)(char*, char*, int)>(((uintptr_t)base + 0x63D300))(code, code, 0);
			OutputDebugStringA("WOW.2");
		}
		catch (...)
		{
			OutputDebugStringA("WOW.3");
		}


		std::cout << "[+]0x1832 消息...本地计数 -> " << count << "\t接收消息计数 ->" << lParam << std::endl;
		count++;
	}

	// 调用原始窗口过程处理其他消息
	return CallWindowProc(OriginalWndProcHandler, hWnd, uMsg, wParam, lParam);
}


// 绘制钩子函数（已注释掉自定义绘制逻辑）
void __stdcall hookD3D11DrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) {
	// 调用原始的 DrawIndexed 函数，不做任何自定义绘制
	fnID3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}

// 从交换链中获取设备和上下文
HRESULT GetDeviceAndCtxFromSwapchain(IDXGISwapChain* pSwapChain, ID3D11Device** ppDevice, ID3D11DeviceContext** ppContext) {
	HRESULT ret = pSwapChain->GetDevice(__uuidof(ID3D11Device), (PVOID*)ppDevice);
	if (SUCCEEDED(ret))
	{
		(*ppDevice)->GetImmediateContext(ppContext);
	}
	return ret;
}



HRESULT __fastcall ResizeBuffers_hooked(IDXGISwapChain* pChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT Flags)
{
	if (mainRenderTargetView) {
		mainRenderTargetView->Release();
		mainRenderTargetView = nullptr;
	}

	renderview_lost = true;

	return fnIDXGISwapChainResizeBuffers(pChain, BufferCount, Width, Height, NewFormat, Flags);
}

// Present 钩子函数
HRESULT __fastcall Present_hooked(IDXGISwapChain* pChain, UINT SyncInterval, UINT Flags)
{
	DXGI_SWAP_CHAIN_DESC sd;
	HRESULT hr = pChain->GetDesc(&sd);
	if (FAILED(hr)) {
		std::cerr << "Failed to get swap chain description, hr: " << std::hex << hr << std::endl;
		return fnIDXGISwapChainPresent(pChain, SyncInterval, Flags);
	}

	if (!g_bInitialised || renderview_lost) {
		g_PresentHooked = true;

		if (FAILED(GetDeviceAndCtxFromSwapchain(pChain, &pDevice, &pContext)))
			return fnIDXGISwapChainPresent(pChain, SyncInterval, Flags);

		pSwapChain = pChain;

		if (OriginalWndProcHandler == nullptr)
		{
			window = sd.OutputWindow;
			OriginalWndProcHandler = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)hWndProc);
		}

		ID3D11Texture2D* pBackBuffer = nullptr;
		hr = pChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
		if (SUCCEEDED(hr)) {
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
		}

		g_bInitialised = true;
		renderview_lost = false;
	}

	// 窗口大小变化处理
	RECT rect;
	GetClientRect(sd.OutputWindow, &rect);
	UINT width = rect.right - rect.left;
	UINT height = rect.bottom - rect.top;

	if (sd.BufferDesc.Width != width || sd.BufferDesc.Height != height) {
		if (mainRenderTargetView) {
			mainRenderTargetView->Release();
			mainRenderTargetView = nullptr;
		}

		hr = pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		if (FAILED(hr)) {
			std::cerr << "Failed to resize buffers, hr: " << std::hex << hr << std::endl;
			return hr;
		}

		ID3D11Texture2D* pBackBuffer = nullptr;
		hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
		if (SUCCEEDED(hr)) {
			pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
		}

		D3D11_VIEWPORT viewport = { 0 };
		viewport.Width = (FLOAT)width;
		viewport.Height = (FLOAT)height;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		pContext->RSSetViewports(1, &viewport);
	}

	// 设置渲染目标
	pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);

	return fnIDXGISwapChainPresent(pChain, SyncInterval, Flags);
}


// 钩住 DirectX 的 Present 函数
void detourDirectXPresent() {
	std::cout << "[+] 正在调用 fnIDXGISwapChainPresent Detour" << std::endl;
	DetourTransactionBegin();
	std::cout << "Detour 事务已开始\n";
	DetourUpdateThread(GetCurrentThread());
	// 替换原始的 Present 函数
	DetourAttach(&(LPVOID&)fnIDXGISwapChainPresent, (PBYTE)Present_hooked);
	std::cout << "函数替换已完成\n";
	DetourTransactionCommit();
}


// 钩住 DirectX 的 Present 函数
void detourDirectXResizeBuffers() {
	std::cout << "[+] 正在调用 fnIDXGISwapChainResizeBuffers Detour" << std::endl;
	DetourTransactionBegin();
	std::cout << "Detour 事务已开始\n";
	DetourUpdateThread(GetCurrentThread());
	// 替换原始的 Present 函数
	DetourAttach(&(LPVOID&)fnIDXGISwapChainResizeBuffers, (PBYTE)ResizeBuffers_hooked);
	std::cout << "函数替换已完成\n";
	DetourTransactionCommit();
}


//void detourDirectXDrawIndexed() {
//	std::cout << "[+] 正在调用 fnID3D11DrawIndexed Detour" << std::endl;
//	DetourTransactionBegin();
//	DetourUpdateThread(GetCurrentThread());
//	// 将原始的 fnID3D11DrawIndexed 替换为自定义的 hookD3D11DrawIndexed
//	DetourAttach(&(LPVOID&)fnID3D11DrawIndexed, (PBYTE)hookD3D11DrawIndexed);
//	DetourTransactionCommit();
//}


// 获取 Present 函数地址
void retrieveValues() {
	DWORD_PTR hDxgi = (DWORD_PTR)GetModuleHandle(L"dxgi.dll");
#if defined(ENV64BIT)
	fnIDXGISwapChainPresent = (IDXGISwapChainPresent)((DWORD_PTR)hDxgi + 0x5070);
#elif defined (ENV32BIT)
	fnIDXGISwapChainPresent = (IDXGISwapChainPresent)((DWORD_PTR)hDxgi + 0x10230);
#endif
	std::cout << "[+] Present 地址: " << std::hex << fnIDXGISwapChainPresent << std::endl;
}


// 打印设备和上下文的地址信息
void printValues() {
	std::cout << "[+] ID3D11DeviceContext 地址: " << std::hex << pContext << std::endl;
	std::cout << "[+] ID3D11Device 地址: " << std::hex << pDevice << std::endl;
	std::cout << "[+] ID3D11RenderTargetView 地址: " << std::hex << mainRenderTargetView << std::endl;
	std::cout << "[+] IDXGISwapChain 地址: " << std::hex << pSwapChain << std::endl;
}

LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { return DefWindowProc(hwnd, uMsg, wParam, lParam); }

// 创建并获取 Present 函数
void GetPresent()
{
	// 创建一个简单的窗口用于初始化交换链
	WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DXGIMsgProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
	RegisterClassExA(&wc);
	HWND hWnd = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

	// 配置交换链
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = 2;
	sd.BufferDesc.Height = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// 尝试创建设备和交换链
	D3D_FEATURE_LEVEL FeatureLevelsRequested = D3D_FEATURE_LEVEL_11_0;
	UINT numFeatureLevelsRequested = 1;
	D3D_FEATURE_LEVEL FeatureLevelsSupported;
	HRESULT hr;
	IDXGISwapChain* swapchain = 0;
	ID3D11Device* dev = 0;
	ID3D11DeviceContext* devcon = 0;

	// 如果设备和交换链创建失败，输出错误信息
	if (FAILED(hr = D3D11CreateDeviceAndSwapChain(NULL,
		D3D_DRIVER_TYPE_HARDWARE,
		NULL,
		0,
		&FeatureLevelsRequested,
		numFeatureLevelsRequested,
		D3D11_SDK_VERSION,
		&sd,
		&swapchain,
		&dev,
		&FeatureLevelsSupported,
		&devcon)))
	{
		std::cout << "[-] 无法使用 VT 方法挂钩 Present." << std::endl;
		return;
	}

	// 获取交换链的虚表并保存 Present 函数地址
	DWORD_PTR* pSwapChainVtable = NULL;
	pSwapChainVtable = (DWORD_PTR*)swapchain;
	pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];
	// fnIDXGISwapChainPresent = (IDXGISwapChainPresent)(DWORD_PTR)pSwapChainVtable[8];
	fnIDXGISwapChainPresent = (IDXGISwapChainPresent)(DWORD_PTR)(pSwapChainVtable[IDXGISwapChainvTable::PRESENT]);
	fnIDXGISwapChainResizeBuffers = (IDXGISwapChainResizeBuffers)(DWORD_PTR)(pSwapChainVtable[IDXGISwapChainvTable::RESIZE_BUFFERS]);
	
	std::cout << "[+] Present 地址: " << std::hex << fnIDXGISwapChainPresent << std::endl;
	std::cout << "[+] ResizeBuffers 地址: " << std::hex << fnIDXGISwapChainResizeBuffers << std::endl;



	// 释放设备和上下文
	dev->Release();
	devcon->Release();
	swapchain->Release();
	DestroyWindow(hWnd);
	std::cout << "[+] 设备创建并销毁成功!" << std::endl;
}


void PrintOffsets() {
	//Console
	FILE* pFile = nullptr;
	AllocConsole();
	freopen_s(&pFile, "CONOUT$", "w", stdout);
	printf("请稍候。这可能需要几分钟...\n");
	//Removed pattern scanner and dumper.
}


DWORD WINAPI MainThread(LPVOID lpReserved)
{

	//PrintOffsets();

//
//#ifndef NDEBUG
	GInterface::Init(ModuleInUse);
	GetPresent();


	// If GetPresent failed we have this backup method to get Present Address
	if (!g_PresentHooked) {
		std::cout << "等待...\n";
		retrieveValues();
	}

	// After this call, Present should be hooked and controlled by me.
	detourDirectXPresent();
	detourDirectXResizeBuffers();
	while (!g_bInitialised) {
		Sleep(1000);
	}

	printValues();


	std::cout << "Hook 结束.....\n";
	std::cout << "窗口句柄 -> " << window << std::endl;


	//std::cout << "[+] pDeviceContextVTable0 地址: " << std::hex << pContext << std::endl;
	//pDeviceContextVTable = (DWORD_PTR*)pContext;
	//std::cout << "[+] pDeviceContextVTable1 地址: " << std::hex << pDeviceContextVTable << std::endl;
	//pDeviceContextVTable = (DWORD_PTR*)pDeviceContextVTable[0];
	//std::cout << "[+] pDeviceContextVTable2 地址: " << std::hex << pDeviceContextVTable << std::endl;
	////fnID3D11DrawIndexed
	//fnID3D11DrawIndexed = (ID3D11DrawIndexed)pDeviceContextVTable[12];

	//std::cout << "[+] pDeviceContextVTable 地址: " << std::hex << pDeviceContextVTable << std::endl;

	//std::cout << "[+] fnID3D11DrawIndexed Addr: " << std::hex << fnID3D11DrawIndexed << std::endl;
	//detourDirectXDrawIndexed();
//#endif
	return TRUE;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		ModuleInUse = hModule;
		DisableThreadLibraryCalls(hModule); // 禁用线程库调用
		CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

