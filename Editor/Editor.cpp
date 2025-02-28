#include <d3dcompiler.h>

#include "Editor.h"
#include "Shader.h"

#pragma comment(lib, "d3d12.lib") 
#pragma comment(lib, "d3dcompiler.lib") 
#pragma comment(lib, "dxgi.lib") 

#define LINE_STRING "================================"



static int const NUM_BACK_BUFFERS = 3;
static int const NUM_FRAMES_IN_FLIGHT = 3;
static FrameContext  gFrameContext[NUM_FRAMES_IN_FLIGHT] = {};
static UINT gFrameIndex = 0;
static ID3D12Device* gD3dDevice = nullptr;
static IDXGISwapChain3* gSwapChain = nullptr;
static HANDLE gSwapChainWaitableObject = nullptr;
static ID3D12DescriptorHeap* gD3dRtvDescHeap = nullptr;
static ID3D12DescriptorHeap* gD3dDsvDescHeap = nullptr;
static ID3D12DescriptorHeap* gD3dSrvDescHeap = nullptr;
static ID3D12CommandQueue* gD3dCommandQueue = nullptr;
static ID3D12GraphicsCommandList* gD3dCommandList = nullptr;
static ID3D12Fence* gFence = nullptr;
static HANDLE gFenceEvent = nullptr;
static UINT64 gFenceLastSignaledValue = 0;
static ID3D12Resource* gMainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static ID3D12Resource* gMainDepthBufferResource[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE  gMainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE  gMainDepthBufferDescriptor[NUM_BACK_BUFFERS] = {};







void WaitForLastSubmittedFrame()
{
    FrameContext* frameCtx = &gFrameContext[gFrameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtx->FenceValue = 0;
    if (gFence->GetCompletedValue() >= fenceValue)
        return;

    gFence->SetEventOnCompletion(fenceValue, gFenceEvent);
    WaitForSingleObject(gFenceEvent, INFINITE);
}

void WaitForCurrentFrame()
{
    FrameContext* frameCtx = &gFrameContext[gFrameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtx->FenceValue = 0;
    if (gFence->GetCompletedValue() >= fenceValue)
        return;

    gFence->SetEventOnCompletion(fenceValue, gFenceEvent);
    WaitForSingleObject(gFenceEvent, INFINITE);
}


FrameContext* WaitForNextFrameResources()
{
    UINT nextFrameIndex = gFrameIndex + 1;
    gFrameIndex = nextFrameIndex;

    HANDLE waitableObjects[] = { gSwapChainWaitableObject, nullptr };
    DWORD numWaitableObjects = 1;

    FrameContext* frameCtx = &gFrameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
    UINT64 fenceValue = frameCtx->FenceValue;
    if (fenceValue != 0) // means no fence was signaled
    {
        frameCtx->FenceValue = 0;
        gFence->SetEventOnCompletion(fenceValue, gFenceEvent);
        waitableObjects[1] = gFenceEvent;
        numWaitableObjects = 2;
    }

    WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

    return frameCtx;
}

void CreateRenderTarget(UINT Width, UINT Height)
{
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource* pBackBuffer = nullptr;
        gSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        gD3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, gMainRenderTargetDescriptor[i]);
        gMainRenderTargetResource[i] = pBackBuffer;


        D3D12_CLEAR_VALUE DepthOptimizedClearValue = {};
        DepthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        DepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        DepthOptimizedClearValue.DepthStencil.Stencil = 0;

        D3D12_HEAP_PROPERTIES HeapProp;
        memset(&HeapProp, 0, sizeof(D3D12_HEAP_PROPERTIES));
        HeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC DepthBufferDesc = {};
        DepthBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        DepthBufferDesc.Width = Width;
        DepthBufferDesc.Height = Height;
        DepthBufferDesc.DepthOrArraySize = 1;
        DepthBufferDesc.MipLevels = 1;
        DepthBufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
        DepthBufferDesc.SampleDesc.Count = 1;
        DepthBufferDesc.SampleDesc.Quality = 0;
        DepthBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        DepthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        DepthBufferDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

        ID3D12Resource* pDepthBuffer = nullptr;
        if (FAILED(gD3dDevice->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &DepthBufferDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &DepthOptimizedClearValue, IID_PPV_ARGS(&pDepthBuffer))))
        {
            std::cout << "Create Depth Buffer Failed." << std::endl;
        }
        gMainDepthBufferResource[i] = pDepthBuffer;

        D3D12_DEPTH_STENCIL_VIEW_DESC DepthStencilDesc = {};
        DepthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        DepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        DepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        gD3dDevice->CreateDepthStencilView(gMainDepthBufferResource[i], &DepthStencilDesc, gMainDepthBufferDescriptor[i]);
    }
}


void CleanupRenderTarget()
{
    WaitForLastSubmittedFrame();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
        if (gMainRenderTargetResource[i]) {
            gMainRenderTargetResource[i]->Release();
            gMainRenderTargetResource[i] = nullptr;
        }

        if (gMainDepthBufferResource[i]) {
            gMainDepthBufferResource[i]->Release();
            gMainDepthBufferResource[i] = nullptr;
        }
    }
}


bool CreateDeviceD3D(HWND hWnd)
{

#if defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug>DebugController0;
    Microsoft::WRL::ComPtr<ID3D12Debug1>DebugController1;
    if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController0))))
    {
        std::cout << "D3D12GetDebugInterface(IID_PPV_ARGS(&DebugController0))) FAILED" << std::endl;
        return true;
    }
    if (FAILED(DebugController0->QueryInterface(IID_PPV_ARGS(&DebugController1))))
    {
        std::cout << "DebugController0->QueryInterface(IID_PPV_ARGS(&DebugController1)) FAILED" << std::endl;
        return true;
    }
    if (DebugController1) {
        DebugController1->SetEnableGPUBasedValidation(true);
        DebugController1->EnableDebugLayer();
    }

#endif

    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = NUM_BACK_BUFFERS;
        sd.Width = 0;
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.Stereo = FALSE;
    }

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(nullptr, featureLevel, IID_PPV_ARGS(&gD3dDevice)) != S_OK)
        return false;

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = NUM_BACK_BUFFERS;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        if (gD3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&gD3dRtvDescHeap)) != S_OK)
            return false;

        SIZE_T rtvDescriptorSize = gD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = gD3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            gMainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
        Desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        Desc.NumDescriptors = NUM_BACK_BUFFERS;
        Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if (gD3dDevice->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&gD3dDsvDescHeap)) != S_OK)
            return false;

        SIZE_T DsvDescriptorSize = gD3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle = gD3dDsvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            gMainDepthBufferDescriptor[i] = DsvHandle;
            DsvHandle.ptr += DsvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 2;  //0 for ui rendering, 1 for constant buffer in model rendering
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (gD3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&gD3dSrvDescHeap)) != S_OK)
            return false;
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        if (gD3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&gD3dCommandQueue)) != S_OK)
            return false;
    }

    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (gD3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&gFrameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (gD3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, gFrameContext[0].CommandAllocator, nullptr, IID_PPV_ARGS(&gD3dCommandList)) != S_OK ||
        gD3dCommandList->Close() != S_OK)
        return false;

    if (gD3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gFence)) != S_OK)
        return false;

    gFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (gFenceEvent == nullptr)
        return false;

    {
        IDXGIFactory4* dxgiFactory = nullptr;
        IDXGISwapChain1* swapChain1 = nullptr;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
            return false;
        if (dxgiFactory->CreateSwapChainForHwnd(gD3dCommandQueue, hWnd, &sd, nullptr, nullptr, &swapChain1) != S_OK)
            return false;
        if (swapChain1->QueryInterface(IID_PPV_ARGS(&gSwapChain)) != S_OK)
            return false;
        swapChain1->Release();
        dxgiFactory->Release();
        gSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        gSwapChainWaitableObject = gSwapChain->GetFrameLatencyWaitableObject();
    }

    RECT Rect = { 0, 0, 0, 0 };
    ::GetClientRect(hWnd, &Rect);
    CreateRenderTarget(Rect.right - Rect.left, Rect.bottom - Rect.top);

    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (gSwapChain) { gSwapChain->SetFullscreenState(false, nullptr); gSwapChain->Release(); gSwapChain = nullptr; }
    if (gSwapChainWaitableObject != nullptr) { CloseHandle(gSwapChainWaitableObject); }
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (gFrameContext[i].CommandAllocator) { gFrameContext[i].CommandAllocator->Release(); gFrameContext[i].CommandAllocator = nullptr; }
    if (gD3dCommandQueue) { gD3dCommandQueue->Release(); gD3dCommandQueue = nullptr; }
    if (gD3dCommandList) { gD3dCommandList->Release(); gD3dCommandList = nullptr; }
    if (gD3dRtvDescHeap) { gD3dRtvDescHeap->Release(); gD3dRtvDescHeap = nullptr; }
    if (gD3dDsvDescHeap) { gD3dDsvDescHeap->Release();  gD3dDsvDescHeap = nullptr; }
    if (gD3dSrvDescHeap) { gD3dSrvDescHeap->Release(); gD3dSrvDescHeap = nullptr; }
    if (gFence) { gFence->Release(); gFence = nullptr; }
    if (gFenceEvent) { CloseHandle(gFenceEvent); gFenceEvent = nullptr; }
    if (gD3dDevice) { gD3dDevice->Release(); gD3dDevice = nullptr; }

}


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (gD3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            WaitForLastSubmittedFrame();
            CleanupRenderTarget();
            HRESULT result = gSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
            assert(SUCCEEDED(result) && "Failed to resize swapchain.");
            CreateRenderTarget((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}


Editor::Editor(Processer* InProcesser)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    ExternalProcesser = InProcesser;
 
    ImportFilePath = "NO File Loaded";
    ExportFilePath = "NO File Loaded";
    Progress = 0.0;

    NeedWaitLastFrame = false;
    Terminated = false;
    Working = false;
    Imported = false;
    CurrentPassIndex = 0;

    CallBackOnRender = nullptr;
    CallBackOnTick = nullptr;
    CallBackOnLoadModel = nullptr;
    CallBackOnLastFrameFinishedRender = nullptr;
    CallBackOnInspectorUI = nullptr;
    CallBackOnCustomUI = nullptr;
    CallBackOnAllProgressFinished = nullptr;

    Inited = false;
}

Editor::~Editor()
{
    Inited = false;
    ImGui::DestroyContext();
}



bool Editor::Init(const wchar_t* ApplicationName, int PosX, int PosY, UINT Width, UINT Height)
{
    if (Inited) return true;

    // Create application window
    WndClass = { sizeof(WndClass), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, ApplicationName, nullptr };
    ::RegisterClassExW(&WndClass);
    HWnd = ::CreateWindowW(WndClass.lpszClassName, ApplicationName, WS_OVERLAPPEDWINDOW, PosX, PosY, Width, Height, nullptr, nullptr, WndClass.hInstance, nullptr);
    
    // Initialize Direct3D
    if (!CreateDeviceD3D(HWnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(WndClass.lpszClassName, WndClass.hInstance);
        return false;
    }

    // Show the window
    ::ShowWindow(HWnd, SW_SHOWDEFAULT);
    ::UpdateWindow(HWnd);


    ImGui_ImplWin32_Init(HWnd);
    ImGui_ImplDX12_Init(gD3dDevice, NUM_FRAMES_IN_FLIGHT,
        DXGI_FORMAT_R8G8B8A8_UNORM, gD3dSrvDescHeap,
        gD3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        gD3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

    MeshViewer.reset(new MeshRenderer(gD3dDevice, gD3dSrvDescHeap));

    FileTypeList.clear();

    Inited = true;

    return true;
}


void Editor::Loop()
{
    if (!Inited) return;

    // Main loop
    ImVec4 clear_color = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;


        if (NeedWaitForSumittedFrame())
        {
            WaitForLastSubmittedFrame();
            OnLastFrameFinished();
        }

        if (CallBackOnTick != nullptr)
            CallBackOnTick(ExternalProcesser, &Hint);

        FrameContext* NextFrameCtx = WaitForNextFrameResources();
        UINT backBufferIdx = gSwapChain->GetCurrentBackBufferIndex();
        NextFrameCtx->CommandAllocator->Reset();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = gMainRenderTargetResource[backBufferIdx];
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        gD3dCommandList->Reset(NextFrameCtx->CommandAllocator, nullptr);
        gD3dCommandList->ResourceBarrier(1, &barrier);

        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        gD3dCommandList->ClearRenderTargetView(gMainRenderTargetDescriptor[backBufferIdx], clear_color_with_alpha, 0, nullptr);
        gD3dCommandList->ClearDepthStencilView(gMainDepthBufferDescriptor[backBufferIdx], D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        gD3dCommandList->OMSetRenderTargets(1, &gMainRenderTargetDescriptor[backBufferIdx], FALSE, &gMainDepthBufferDescriptor[backBufferIdx]);
        gD3dCommandList->SetDescriptorHeaps(1, &gD3dSrvDescHeap);

        // Rendering
        Render(gD3dCommandList);
        if (CallBackOnRender != nullptr)
            CallBackOnRender(ExternalProcesser, &Hint);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        gD3dCommandList->ResourceBarrier(1, &barrier);
        gD3dCommandList->Close();

        gD3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&gD3dCommandList);

        gSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync

        UINT64 fenceValue = gFenceLastSignaledValue + 1;
        gD3dCommandQueue->Signal(gFence, fenceValue);
        gFenceLastSignaledValue = fenceValue;
        NextFrameCtx->FenceValue = fenceValue;
    }

    WaitForLastSubmittedFrame();
}


void Editor::Close()
{
    if (Inited) {
        MeshViewer.release();

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();

        CleanupDeviceD3D();
        ::DestroyWindow(HWnd);
        ::UnregisterClassW(WndClass.lpszClassName, WndClass.hInstance);
    }
}





void Editor::Render(ID3D12GraphicsCommandList* CommandList)
{
    // Render 3d model
    RenderBackground(CommandList);

    // Render UI
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    {
        static float f = 0.0f;
        static int counter = 0;
        ImGuiIO& io = ImGui::GetIO();

        //ImGuiWindowFlags window_flags = 0;
        //window_flags |= ImGuiWindowFlags_NoResize;
        //window_flags |= ImGuiWindowFlags_NoCollapse;

        ImGui::SetNextWindowPos(ImVec2(4, 6), ImGuiCond_FirstUseEver);
        //ImGui::Begin("Inspector", NULL, window_flags);
        ImGui::Begin("Inspector");
        ImGui::SetWindowSize(ImVec2(700, 300), ImGuiCond_FirstUseEver);

        ImGui::Text("FPS: %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::Separator();
        ImGui::Separator();
        ImGui::Text("");

        ImGui::BeginDisabled(Working);
        if (ImGui::Button("Import"))
        {
            OnLoadButtonClicked();
        }
        ImGui::SameLine();
        if (ImGui::Button("Export"))
        {
            OnExportButtonClicked();
        }
        ImGui::EndDisabled();

        ImGui::Text(ImportFilePath.string().c_str());
        ImGui::Text("");
        ImGui::Separator();
        ImGui::Separator();

        ImGui::BeginDisabled(Working);
        if (ImGui::Button("Generate"))
        {
            OnGenerateButtonClicked();
        }
        ImGui::EndDisabled();

        ImGui::ProgressBar(GetProgress(), ImVec2(-1.0f, 0.0f));
        ImGui::TextColored(Hint.Color, Hint.Text.c_str());

        if (CallBackOnInspectorUI != nullptr)
            CallBackOnInspectorUI(ExternalProcesser, &Hint);

        ImGui::End();
    }

    if (MeshViewer.get())
        MeshViewer->ShowViewerSettingUI(Imported, Working);
   
    if (CallBackOnCustomUI != nullptr)
        CallBackOnCustomUI(ExternalProcesser, &Hint);

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), CommandList);
}


void Editor::BrowseFileOpen(std::filesystem::path* OutFilePath)
{
    IFileOpenDialog* pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pfd));

    COMDLG_FILTERSPEC* rgSpec = nullptr;
    if (FileTypeList.size() > 0)
    {
        rgSpec = new COMDLG_FILTERSPEC[FileTypeList.size()];
        for (int i = 0; i < FileTypeList.size(); i++)
        {
            rgSpec[i].pszName = FileTypeList[i].Name;
            rgSpec[i].pszSpec = FileTypeList[i].Type;
        }
    }

    std::string SeletedFile;
    if (SUCCEEDED(hr))
    {
        DWORD dwFlags;

        hr = pfd->GetOptions(&dwFlags);
        hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
        hr = pfd->SetFileTypes(FileTypeList.size(), rgSpec);

        hr = pfd->Show(NULL);
        if (SUCCEEDED(hr))
        {
            IShellItemArray* psiaResults;
            hr = pfd->GetResults(&psiaResults);
            if (SUCCEEDED(hr))
            {
                DWORD resultNum;
                hr = psiaResults->GetCount(&resultNum);
                if (resultNum >= 1)
                {
                    IShellItem* psiaResult;
                    hr = psiaResults->GetItemAt(0, &psiaResult);
                    PWSTR pszFilePath = NULL;
                    hr = psiaResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr))
                    {
                        //MessageBoxW(NULL, pszFilePath, L"File Path", MB_OK);
                        *OutFilePath = ToUtf8(std::wstring(pszFilePath));
                    }
                    CoTaskMemFree(pszFilePath);
                }
            }
            if (psiaResults)
                psiaResults->Release();
        }
        pfd->Release();
    }
    CoUninitialize();

    if (rgSpec != nullptr)
    {
        delete[] rgSpec;
    }

    return;
}

void Editor::BrowseFileSave(std::filesystem::path* OutFilePath)
{
    IFileSaveDialog* pfd = NULL;
    HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL, IID_IFileSaveDialog, reinterpret_cast<void**>(&pfd));

    COMDLG_FILTERSPEC* rgSpec = nullptr;
    if (FileTypeList.size() > 0)
    {
        rgSpec = new COMDLG_FILTERSPEC[FileTypeList.size()];
        for (int i = 0; i < FileTypeList.size(); i++)
        {
            rgSpec[i].pszName = FileTypeList[i].Name;
            rgSpec[i].pszSpec = FileTypeList[i].Type;
        }
    }

    std::string SeletedFile;
    if (SUCCEEDED(hr))
    {
        DWORD dwFlags;

        hr = pfd->GetOptions(&dwFlags);
        hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
        hr = pfd->SetFileTypes(FileTypeList.size(), rgSpec);

        hr = pfd->Show(NULL);
        if (SUCCEEDED(hr))
        {
            IShellItem* psiaResult;
            hr = pfd->GetResult(&psiaResult);
            if (SUCCEEDED(hr))
            {
                PWSTR pszFilePath = NULL;
                hr = psiaResult->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                if (SUCCEEDED(hr))
                {
                    //MessageBoxW(NULL, pszFilePath, L"File Path", MB_OK);
                    *OutFilePath = ToUtf8(std::wstring(pszFilePath));
                }
                CoTaskMemFree(pszFilePath);
                
            }
            if (psiaResult)
                psiaResult->Release();
        }
        pfd->Release();
    }
    CoUninitialize();

    if (rgSpec != nullptr)
    {
        delete[] rgSpec;
    }

    return;
}

void Editor::OnLoadButtonClicked()
{
   BrowseFileOpen(&ImportFilePath);

   Imported = false;
   bool Success = false;
   if (ExternalProcesser != nullptr)
       Success = ExternalProcesser->Import(&ImportFilePath);

   if (CallBackOnLoadModel != nullptr)
       CallBackOnLoadModel(ExternalProcesser, &ImportFilePath, &Hint);

   if (!Success)
   {
       Hint.Error("Import Failed, nothing imported");
   }
   else
       Imported = true;
}


void Editor::OnGenerateButtonClicked()
{
    bool Success = false;
    if (ExternalProcesser != nullptr)
        KickGenerateMission();

    if (!Success)
    {
        Hint.Error("NO File Loaded OR Failed to start process...");
    }
}

void Editor::OnExportButtonClicked()
{
    BrowseFileSave(&ExportFilePath);

    bool Success = false;
    if (ExternalProcesser != nullptr)
        Success = ExternalProcesser->Export(&ExportFilePath);

    if (!Success)
    {
        Hint.Error("Export Failed, nothing exported");
    }
    else
        Hint.Normal("Exported");
}

void Editor::OnOneProgressFinished()
{
    // flush working mission
    while (true)
    {
        if (ExternalProcesser->IsWorking())
            ExternalProcesser->GetProgress();
        else
            break;
    }

    std::string& ErrorString = ExternalProcesser->GetErrorString();
    if (ErrorString.size() > 0) {
        std::cout << LINE_STRING << std::endl;
        std::cout << "Something get error, please see error" + std::to_string(CurrentPassIndex) + ".log." << std::endl;
        std::cout << LINE_STRING << std::endl;

        ExternalProcesser->DumpErrorString(CurrentPassIndex);
    }
  
}

void Editor::OnAllProgressFinished()
{
    if (CallBackOnAllProgressFinished != nullptr)
        CallBackOnAllProgressFinished(ExternalProcesser, &Hint);

    //Loaded = true;
    //MeshViewer->LoadMeshFromProcesser(Processer.get());
}


double Editor::GetProgress()
{
    if (ExternalProcesser == nullptr) return 0.0;

    Progress = ExternalProcesser->GetProgress();

    if (!ExternalProcesser->IsWorking() && Working)
    {
        if (!PassPool.empty()) 
        {
            if (Progress == 1.0)
                OnOneProgressFinished();

            PassType NextPass = PassPool.front();
            if (NextPass != nullptr) {
                ExternalProcesser->Clear();
                CurrentPassIndex++;

                bool Success = NextPass(ExternalProcesser, Hint.Text);
                if (!Success)
                {
                    Hint.ErrorColor();
                    ClearPassPool();
                    Terminated = true;
                }
                else {
                    Hint.NormalColor();
                    Progress = 0.0;
                }
            }

            if (!PassPool.empty())
                PassPool.pop();
        }
        else
        {
            if (!Terminated) {
                Hint.Normal("Completed.");
                OnAllProgressFinished();
            }
            else
            {
                Hint.Error("Something Error, Terminated.");
            }
            Working = false;
        }
    }

    return Progress;
}

void Editor::ClearPassPool()
{
    if (!PassPool.empty())
    {
        std::queue<PassType> Empty;
        swap(Empty, PassPool);
    }
}


void Editor::KickGenerateMission()
{
    if (ExternalProcesser == nullptr) return;

    ClearPassPool();

    Progress = 0.0;
    Working = true;
    Terminated = false;
    Hint.NormalColor();
    CurrentPassIndex = 0;

    for(int i = 0; i< ExternalProcesser->PassPool.size(); i++)
        PassPool.push(ExternalProcesser->PassPool[i]);
  

}



void Editor::RenderBackground(ID3D12GraphicsCommandList* CommandList)
{

    if (MeshViewer.get()) {
     
        Float3 DisplaySize = Float3(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0);
        MeshViewer->RenderModel(DisplaySize, CommandList);

    }

}



void MeshRenderer::ShowViewerSettingUI(bool ModelIsLoaded, bool GeneratorIsWorking)
{

    ImGui::SetNextWindowPos(ImVec2(4, 306), ImGuiCond_FirstUseEver);
    ImGui::Begin("Debug Viewer");
    ImGui::SetWindowSize(ImVec2(255, 350), ImGuiCond_FirstUseEver);
    ImGui::Separator();
    ImGui::BeginDisabled(GeneratorIsWorking);
    if (ImGui::Button("Render Model"))
    {
        if (!ModelIsLoaded)
        {
            Hint.Error("Please Import  First.");
        }
        else {
            KickShowModel();
        }
    }
    ImGui::TextColored(Hint.Color, Hint.Text.c_str());
    ImGui::EndDisabled();

    ImGui::SeparatorText("Camera Setting");

    if (ImGui::Button("Reset Camera"))
    {
        RenderCamera.Reset(TotalBounding);
    }
    ImGui::InputFloat("Move Speed", &RenderCamera.MoveSpeed, 0.1f, 1.0f, "%.3f");
    ImGui::InputFloat("FOV(Vertical)", &RenderCamera.FovY, 0.01f, 1.0f, "%.3f");
    ImGui::InputFloat("NearZ", &RenderCamera.NearZ, 0.01f, 1.0f, "%.5f");
    ImGui::InputFloat("FarZ", &RenderCamera.FarZ, 0.01f, 1.0f, "%.5f");

    ImGui::SeparatorText("View Setting");

    static int Val = 0;
    ImGui::Checkbox("Show Wire Frame", &ShowWireFrame);
    ImGui::Checkbox("Show Face Normal", &ShowFaceNormal);
    ImGui::Checkbox("Show Vertex Normal", &ShowVertexNormal);

    ImGui::Separator();

    ImGui::End();


    Float3 MoveDirection;
    Float3 MoveOffset;
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
    {
        MoveDirection.x += io.MouseDelta.x * 0.05f;//(io.MouseDelta.x > 0.0f ? 1.0f : (io.MouseDelta.x < 0.0f ? -1.0f : 0.0f));
        MoveDirection.y -= io.MouseDelta.y * 0.05f;//(io.MouseDelta.y > 0.0f ? 1.0f : (io.MouseDelta.y < 0.0f ? -1.0f : 0.0f));
    }
    MoveDirection.z += io.MouseWheel;
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
    {
        MoveOffset.x += io.MouseDelta.x * 0.05f;
        MoveOffset.y -= io.MouseDelta.y * 0.05f;
    }

    if (ImGui::IsKeyDown(ImGuiKey_W))
    {
        MoveDirection.z += 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_S))
    {
        MoveDirection.z -= 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_A))
    {
        MoveDirection.x += 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_D))
    {
        MoveDirection.x -= 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_Q))
    {
        MoveDirection.y += 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_E))
    {
        MoveDirection.y -= 1.0f;
    }

    if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
    {
        MoveOffset.y += 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
    {
        MoveOffset.y -= 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
    {
        MoveOffset.x += 1.0f;
    }
    if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
    {
        MoveOffset.x -= 1.0f;
    }

    RenderCamera.RotateAround(MoveDirection);
    RenderCamera.Move(MoveOffset);
    RenderCamera.UpdateDirection();

    if (ImGui::IsKeyDown(ImGuiKey_F))
    {
        RenderCamera.Reset(TotalBounding);
    }
}



bool CreateCommittedResource(UINT64 Size, ID3D12Device* Device, ID3D12Resource** DestBuffer)
{
    D3D12_HEAP_PROPERTIES HeapProp;
    memset(&HeapProp, 0, sizeof(D3D12_HEAP_PROPERTIES));
    HeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC Desc;
    memset(&Desc, 0, sizeof(D3D12_RESOURCE_DESC));
    Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    Desc.Width = Size;
    Desc.Height = 1;
    Desc.DepthOrArraySize = 1;
    Desc.MipLevels = 1;
    Desc.Format = DXGI_FORMAT_UNKNOWN;
    Desc.SampleDesc.Count = 1;
    Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (Device->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &Desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(DestBuffer)) < 0)
        return false;

    return true;
}

bool MeshRenderer::LoadMeshFromProcesser(Processer* InProcesser)
{
    if (D3dDevice == nullptr || InProcesser == nullptr) return false;
    ClearResource();
    
    std::cout << LINE_STRING << std::endl;
    std::cout << "Begin Loading Mesh Data..." << std::endl;

    std::vector<SourceContext*>& SrcList = InProcesser->GetContextList();
    for (int i = 0; i < SrcList.size(); i++)
    {

        Mesh NewMesh;
        NewMesh.Name = SrcList[i]->Name;
        NewMesh.Triangle.IndexNum = SrcList[i]->GetTriangleNum() * 3;
        NewMesh.Triangle.VertexNum = SrcList[i]->GetVertexNum();
        NewMesh.FaceNormal.IndexNum = SrcList[i]->GetTriangleNum() * 2;
        NewMesh.FaceNormal.VertexNum = SrcList[i]->GetTriangleNum() * 2;
        NewMesh.VertexNormal.IndexNum = SrcList[i]->GetVertexNum() * 2;
        NewMesh.VertexNormal.VertexNum = SrcList[i]->GetVertexNum() * 2;
        NewMesh.Bounding = SrcList[i]->Bounding;

        std::cout << "Loading Mesh : " << NewMesh.Name << std::endl;
        std::cout << "Index Num  : " << NewMesh.Triangle.IndexNum << std::endl;
        std::cout << "Vertex Num : " << NewMesh.Triangle.VertexNum << std::endl;


        D3D12_RANGE Range;
        {
            if (!CreateCommittedResource(NewMesh.Triangle.VertexNum * sizeof(DrawRawVertex), D3dDevice, &NewMesh.Triangle.VertexBuffer))
                return false;
            if (!CreateCommittedResource(NewMesh.Triangle.IndexNum * sizeof(DrawRawIndex), D3dDevice, &NewMesh.Triangle.IndexBuffer))
                return false;

            void* VertexResource = nullptr;
            void* IndexResource = nullptr;
            memset(&Range, 0, sizeof(D3D12_RANGE));

            if (NewMesh.Triangle.VertexBuffer->Map(0, &Range, &VertexResource) != S_OK)
                return false;
            DrawRawVertex* VertexDest = (DrawRawVertex*)VertexResource;
            memcpy(VertexDest, SrcList[i]->DrawVertexList, NewMesh.Triangle.VertexNum * sizeof(DrawRawVertex));
            NewMesh.Triangle.VertexBuffer->Unmap(0, &Range);

            if (NewMesh.Triangle.IndexBuffer->Map(0, &Range, &IndexResource) != S_OK)
                return false;
            DrawRawIndex* IndexDest = (DrawRawIndex*)IndexResource;
            memcpy(IndexDest, SrcList[i]->DrawIndexList, NewMesh.Triangle.IndexNum * sizeof(DrawRawIndex));
            NewMesh.Triangle.IndexBuffer->Unmap(0, &Range);

            NewMesh.Triangle.VertexBufferView.BufferLocation = NewMesh.Triangle.VertexBuffer->GetGPUVirtualAddress();
            NewMesh.Triangle.VertexBufferView.StrideInBytes = sizeof(DrawRawVertex);
            NewMesh.Triangle.VertexBufferView.SizeInBytes = NewMesh.Triangle.VertexNum * sizeof(DrawRawVertex);

            NewMesh.Triangle.IndexBufferView.BufferLocation = NewMesh.Triangle.IndexBuffer->GetGPUVirtualAddress();
            NewMesh.Triangle.IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            NewMesh.Triangle.IndexBufferView.SizeInBytes = NewMesh.Triangle.IndexNum * sizeof(DrawRawIndex);
        }

        {
            if (!CreateCommittedResource(NewMesh.FaceNormal.VertexNum * sizeof(DrawRawVertex), D3dDevice, &NewMesh.FaceNormal.VertexBuffer))
                return false;
            if (!CreateCommittedResource(NewMesh.FaceNormal.IndexNum * sizeof(DrawRawIndex), D3dDevice, &NewMesh.FaceNormal.IndexBuffer))
                return false;

            void* VertexResource = nullptr;
            void* IndexResource = nullptr;
            memset(&Range, 0, sizeof(D3D12_RANGE));

            if (NewMesh.FaceNormal.VertexBuffer->Map(0, &Range, &VertexResource) != S_OK)
                return false;
            DrawRawVertex* VertexDest = (DrawRawVertex*)VertexResource;
            memcpy(VertexDest, SrcList[i]->DrawFaceNormalVertexList, NewMesh.FaceNormal.VertexNum * sizeof(DrawRawVertex));
            NewMesh.FaceNormal.VertexBuffer->Unmap(0, &Range);

            if (NewMesh.FaceNormal.IndexBuffer->Map(0, &Range, &IndexResource) != S_OK)
                return false;
            DrawRawIndex* IndexDest = (DrawRawIndex*)IndexResource;
            memcpy(IndexDest, SrcList[i]->DrawFaceNormalIndexList, NewMesh.FaceNormal.IndexNum * sizeof(DrawRawIndex));
            NewMesh.FaceNormal.IndexBuffer->Unmap(0, &Range);

            NewMesh.FaceNormal.VertexBufferView.BufferLocation = NewMesh.FaceNormal.VertexBuffer->GetGPUVirtualAddress();
            NewMesh.FaceNormal.VertexBufferView.StrideInBytes = sizeof(DrawRawVertex);
            NewMesh.FaceNormal.VertexBufferView.SizeInBytes = NewMesh.FaceNormal.VertexNum * sizeof(DrawRawVertex);

            NewMesh.FaceNormal.IndexBufferView.BufferLocation = NewMesh.FaceNormal.IndexBuffer->GetGPUVirtualAddress();
            NewMesh.FaceNormal.IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            NewMesh.FaceNormal.IndexBufferView.SizeInBytes = NewMesh.FaceNormal.IndexNum * sizeof(DrawRawIndex);
        }

        {
            if (!CreateCommittedResource(NewMesh.VertexNormal.VertexNum * sizeof(DrawRawVertex), D3dDevice, &NewMesh.VertexNormal.VertexBuffer))
                return false;
            if (!CreateCommittedResource(NewMesh.VertexNormal.IndexNum * sizeof(DrawRawIndex), D3dDevice, &NewMesh.VertexNormal.IndexBuffer))
                return false;

            void* VertexResource = nullptr;
            void* IndexResource = nullptr;
            memset(&Range, 0, sizeof(D3D12_RANGE));

            if (NewMesh.VertexNormal.VertexBuffer->Map(0, &Range, &VertexResource) != S_OK)
                return false;
            DrawRawVertex* VertexDest = (DrawRawVertex*)VertexResource;
            memcpy(VertexDest, SrcList[i]->DrawVertexNormalVertexList, NewMesh.VertexNormal.VertexNum * sizeof(DrawRawVertex));
            NewMesh.VertexNormal.VertexBuffer->Unmap(0, &Range);

            if (NewMesh.VertexNormal.IndexBuffer->Map(0, &Range, &IndexResource) != S_OK)
                return false;
            DrawRawIndex* IndexDest = (DrawRawIndex*)IndexResource;
            memcpy(IndexDest, SrcList[i]->DrawVertexNormalIndexList, NewMesh.VertexNormal.IndexNum * sizeof(DrawRawIndex));
            NewMesh.VertexNormal.IndexBuffer->Unmap(0, &Range);

            NewMesh.VertexNormal.VertexBufferView.BufferLocation = NewMesh.VertexNormal.VertexBuffer->GetGPUVirtualAddress();
            NewMesh.VertexNormal.VertexBufferView.StrideInBytes = sizeof(DrawRawVertex);
            NewMesh.VertexNormal.VertexBufferView.SizeInBytes = NewMesh.VertexNormal.VertexNum * sizeof(DrawRawVertex);

            NewMesh.VertexNormal.IndexBufferView.BufferLocation = NewMesh.VertexNormal.IndexBuffer->GetGPUVirtualAddress();
            NewMesh.VertexNormal.IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            NewMesh.VertexNormal.IndexBufferView.SizeInBytes = NewMesh.VertexNormal.IndexNum * sizeof(DrawRawIndex);
        }




        std::cout << "Loading Mesh Finished : " << NewMesh.Name << std::endl;

        MeshList.push_back(NewMesh);
        TotalBounding.Resize(NewMesh.Bounding);
    }

    RenderCamera.Reset(TotalBounding);
    std::cout << "Loading Mesh Finished." << std::endl;
    std::cout << LINE_STRING << std::endl;

    return true;
}


void MeshRenderer::ClearResource()
{
    std::vector<Mesh>::iterator it;
    for (it = MeshList.begin(); it != MeshList.end(); it++)
    {
        it->Clear();
    }
    MeshList.clear();

    TotalBounding.Clear();

}


bool MeshRenderer::InitRenderPipeline(D3D12_RASTERIZER_DESC* InRasterizerDesc, D3D12_BLEND_DESC* InBlendDesc)
{
    if (D3dDevice == nullptr || D3dSrcDescriptorHeap == nullptr) return false;

    {
        D3D12_DESCRIPTOR_RANGE DescriptorRange[1] = {};
        //CBV
        DescriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        DescriptorRange[0].NumDescriptors = 1;
        DescriptorRange[0].BaseShaderRegister = 0;
        DescriptorRange[0].RegisterSpace = 0;
        DescriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER RootParameters[1] = {};
        //CBV
        RootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        RootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        RootParameters[0].DescriptorTable.NumDescriptorRanges = _countof(DescriptorRange);
        RootParameters[0].DescriptorTable.pDescriptorRanges = &DescriptorRange[0];

        D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
        RootSignatureDesc.NumParameters = _countof(RootParameters);
        RootSignatureDesc.pParameters = RootParameters;
        RootSignatureDesc.NumStaticSamplers = 0;
        RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ID3DBlob* Blob = nullptr;
        if (FAILED(D3D12SerializeRootSignature(&RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &Blob, nullptr)))
        {
            std::cout << "Create RootSignatureBlob Failed." << std::endl;
            RootSignature = nullptr;
        }

        if (FAILED(D3dDevice->CreateRootSignature(0, Blob->GetBufferPointer(), Blob->GetBufferSize(), IID_PPV_ARGS(&RootSignature))))
        {
            std::cout << "Create RootSignature Failed." << std::endl;
            RootSignature = nullptr;
        }

        if(Blob) Blob->Release();
    }
    if (!RootSignature)
    {
        Release();
        return false;
    }

    {
        ID3DBlob* BlobVertexShader1 = nullptr;
        ID3DBlob* BlobVertexShader2 = nullptr;
        ID3DBlob* BlobVertexShader3 = nullptr;
        ID3DBlob* BlobPixelShader1 = nullptr;
        ID3DBlob* BlobPixelShader2 = nullptr;
        ID3DBlob* BlobPixelShader3 = nullptr;
        ID3DBlob* BlobVertexCompileMsg = nullptr;
        ID3DBlob* BlobPixelCompileMsg = nullptr;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT CompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT CompileFlags = 0;
#endif
        CompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

        
        bool Success = true;
        D3D_SHADER_MACRO Defines1[] = { {"DRAW_WIREFRAME", "0"} , {"DRAW_NORMAL", "0"} , {NULL, NULL} };
        D3D_SHADER_MACRO Defines2[] = { {"DRAW_WIREFRAME", "1"} , {"DRAW_NORMAL", "0"} , {NULL, NULL} };
        D3D_SHADER_MACRO Defines3[] = { {"DRAW_WIREFRAME", "0"} , {"DRAW_NORMAL", "1"} , {NULL, NULL} };

        //std::cout << "Compile Shader Path : \n" << InternalShader << std::endl;
        if(FAILED(D3DCompile(InternalShader, strlen(InternalShader), nullptr, Defines1, nullptr, "VSMain", "vs_5_0", CompileFlags, 0, &BlobVertexShader1, &BlobVertexCompileMsg)))
        {
            if(BlobVertexCompileMsg)
                std::cout << (const char*)(BlobVertexCompileMsg->GetBufferPointer()) << std::endl;
            Success = false;
        }
        if (FAILED(D3DCompile(InternalShader, strlen(InternalShader), nullptr, Defines2, nullptr, "VSMain", "vs_5_0", CompileFlags, 0, &BlobVertexShader2, &BlobVertexCompileMsg)))
        {
            if (BlobVertexCompileMsg)
                std::cout << (const char*)(BlobVertexCompileMsg->GetBufferPointer()) << std::endl;
            Success = false;
        }
        if (FAILED(D3DCompile(InternalShader, strlen(InternalShader), nullptr, Defines3, nullptr, "VSMain", "vs_5_0", CompileFlags, 0, &BlobVertexShader3, &BlobVertexCompileMsg)))
        {
            if (BlobVertexCompileMsg)
                std::cout << (const char*)(BlobVertexCompileMsg->GetBufferPointer()) << std::endl;
            Success = false;
        }
        if (BlobVertexCompileMsg)
            BlobVertexCompileMsg->Release();

        if (FAILED(D3DCompile(InternalShader, strlen(InternalShader), nullptr, Defines1, nullptr, "PSMain", "ps_5_0", CompileFlags, 0, &BlobPixelShader1, &BlobPixelCompileMsg)))
        {
            if (BlobPixelCompileMsg)
                std::cout << (const char*)(BlobPixelCompileMsg->GetBufferPointer()) << std::endl;
            Success = false;
        }
        if (FAILED(D3DCompile(InternalShader, strlen(InternalShader), nullptr, Defines2, nullptr, "PSMain", "ps_5_0", CompileFlags, 0, &BlobPixelShader2, &BlobPixelCompileMsg)))
        {
            if (BlobPixelCompileMsg)
                std::cout << (const char*)(BlobPixelCompileMsg->GetBufferPointer()) << std::endl;
            Success = false;
        }
        if (FAILED(D3DCompile(InternalShader, strlen(InternalShader), nullptr, Defines3, nullptr, "PSMain", "ps_5_0", CompileFlags, 0, &BlobPixelShader3, &BlobPixelCompileMsg)))
        {
            if (BlobPixelCompileMsg)
                std::cout << (const char*)(BlobPixelCompileMsg->GetBufferPointer()) << std::endl;
            Success = false;
        }
        if (BlobPixelCompileMsg)
            BlobPixelCompileMsg->Release();

        if (!Success)
        {
            if (BlobVertexShader1)
                BlobVertexShader1->Release();
            if (BlobVertexShader2)
                BlobVertexShader2->Release();
            if (BlobVertexShader3)
                BlobVertexShader3->Release();
            if (BlobPixelShader1)
                BlobPixelShader1->Release();
            if (BlobPixelShader2)
                BlobPixelShader2->Release();
            if (BlobPixelShader3)
                BlobPixelShader3->Release();
            Release();
            std::cout << "Compile Shader Failed." << std::endl;
            return false;
        }

        D3D12_INPUT_ELEMENT_DESC InputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateDesc = {};
        memset(&PipelineStateDesc, 0, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        PipelineStateDesc.InputLayout = { InputElementDesc, _countof(InputElementDesc) };
        PipelineStateDesc.pRootSignature = RootSignature;

        PipelineStateDesc.VS.pShaderBytecode = BlobVertexShader1->GetBufferPointer();
        PipelineStateDesc.VS.BytecodeLength = BlobVertexShader1->GetBufferSize();

        PipelineStateDesc.PS.pShaderBytecode = BlobPixelShader1->GetBufferPointer();
        PipelineStateDesc.PS.BytecodeLength = BlobPixelShader1->GetBufferSize();

        if (InRasterizerDesc != nullptr)
        {
            PipelineStateDesc.RasterizerState = *InRasterizerDesc;
        }
        else
        {
            PipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            PipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        }

        if (InBlendDesc != nullptr) 
        {
            PipelineStateDesc.BlendState = *InBlendDesc;
        }
        else {
            PipelineStateDesc.BlendState.AlphaToCoverageEnable = FALSE;
            PipelineStateDesc.BlendState.IndependentBlendEnable = FALSE;
            PipelineStateDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }

        PipelineStateDesc.DepthStencilState.DepthEnable = TRUE;
        PipelineStateDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        PipelineStateDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        PipelineStateDesc.DepthStencilState.StencilEnable = FALSE;
        
        PipelineStateDesc.SampleMask = UINT_MAX;
        PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        PipelineStateDesc.NumRenderTargets = 1;
        PipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        PipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        PipelineStateDesc.SampleDesc.Count = 1;
        PipelineStateDesc.SampleDesc.Quality = 0;

        if (FAILED(D3dDevice->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&SolidPipelineState))))
        {
            if (BlobVertexShader1)
                BlobVertexShader1->Release();
            if (BlobVertexShader2)
                BlobVertexShader2->Release();
            if (BlobVertexShader3)
                BlobVertexShader3->Release();
            if (BlobPixelShader1)
                BlobPixelShader1->Release();
            if (BlobPixelShader2)
                BlobPixelShader2->Release();
            if (BlobPixelShader3)
                BlobPixelShader3->Release();
            Release();
            std::cout << "Create Solid Pipeline State Failed." << std::endl;
            return false;
        }

        PipelineStateDesc.VS.pShaderBytecode = BlobVertexShader2->GetBufferPointer();
        PipelineStateDesc.VS.BytecodeLength = BlobVertexShader2->GetBufferSize();
        PipelineStateDesc.PS.pShaderBytecode = BlobPixelShader2->GetBufferPointer();
        PipelineStateDesc.PS.BytecodeLength = BlobPixelShader2->GetBufferSize();
        PipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        PipelineStateDesc.RasterizerState.AntialiasedLineEnable = true;
        PipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = false;
        if (FAILED(D3dDevice->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&WireFramePipelineState))))
        {
            if (BlobVertexShader1)
                BlobVertexShader1->Release();
            if (BlobVertexShader2)
                BlobVertexShader2->Release();
            if (BlobVertexShader3)
                BlobVertexShader3->Release();
            if (BlobPixelShader1)
                BlobPixelShader1->Release();
            if (BlobPixelShader2)
                BlobPixelShader2->Release();
            if (BlobPixelShader3)
                BlobPixelShader3->Release();
            Release();
            std::cout << "Create WireFrame Pipeline State Failed." << std::endl;
            return false;
        }

        PipelineStateDesc.VS.pShaderBytecode = BlobVertexShader3->GetBufferPointer();
        PipelineStateDesc.VS.BytecodeLength = BlobVertexShader3->GetBufferSize();
        PipelineStateDesc.PS.pShaderBytecode = BlobPixelShader3->GetBufferPointer();
        PipelineStateDesc.PS.BytecodeLength = BlobPixelShader3->GetBufferSize();
        PipelineStateDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        PipelineStateDesc.RasterizerState.AntialiasedLineEnable = true;
        PipelineStateDesc.BlendState.RenderTarget[0].BlendEnable = false;
        PipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        if (FAILED(D3dDevice->CreateGraphicsPipelineState(&PipelineStateDesc, IID_PPV_ARGS(&LinePipelineState))))
        {
            if (BlobVertexShader1)
                BlobVertexShader1->Release();
            if (BlobVertexShader2)
                BlobVertexShader2->Release();
            if (BlobVertexShader3)
                BlobVertexShader3->Release();
            if (BlobPixelShader1)
                BlobPixelShader1->Release();
            if (BlobPixelShader2)
                BlobPixelShader2->Release();
            if (BlobPixelShader3)
                BlobPixelShader3->Release();
            Release();
            std::cout << "Create Line Pipeline State Failed." << std::endl;
            return false;
        }
    }


    {
        D3D12_HEAP_PROPERTIES HeapProp;
        memset(&HeapProp, 0, sizeof(D3D12_HEAP_PROPERTIES));
        HeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC ConstantBufferDesc = {};
        ConstantBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ConstantBufferDesc.Width = (ConstantParams.Size() + 255) & ~255; //Constant buffer must be aligned with 256
        ConstantBufferDesc.Height = 1;
        ConstantBufferDesc.DepthOrArraySize = 1;
        ConstantBufferDesc.MipLevels = 1;
        ConstantBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        ConstantBufferDesc.SampleDesc.Count = 1;
        ConstantBufferDesc.SampleDesc.Quality = 0;
        ConstantBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ConstantBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        ConstantBufferDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

        if (FAILED(D3dDevice->CreateCommittedResource(&HeapProp, D3D12_HEAP_FLAG_NONE, &ConstantBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ConstantsBuffer))))
        {
            Release();
            std::cout << "Create Constant Buffer Failed." << std::endl;
            return false;
        }
        


        D3D12_CONSTANT_BUFFER_VIEW_DESC ConstantBufferViewDesc = {};
        ConstantBufferViewDesc.BufferLocation = ConstantsBuffer->GetGPUVirtualAddress();
        ConstantBufferViewDesc.SizeInBytes = (ConstantParams.Size() + 255) & ~255; //Constant buffer must be aligned with 256
        D3D12_CPU_DESCRIPTOR_HANDLE CPUCBVHandle = D3dSrcDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        CPUCBVHandle.ptr += D3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3dDevice->CreateConstantBufferView(&ConstantBufferViewDesc, CPUCBVHandle);

        D3D12_RANGE Range = {};
        memset(&Range, 0, sizeof(D3D12_RANGE));
        if (FAILED(ConstantsBuffer->Map(0, &Range, reinterpret_cast<void**>(&ConstantParams.GPUAddress))))
        {
            std::cout << "ConstantsBuffer Map Failed." << std::endl;
            return false;
        }
        ConstantParams.Copy();

     }


    return true;
}



void MeshRenderer::UpdateEveryFrameState(const Float3& DisplaySize)
{
    
    RenderCamera.AspectRatio = DisplaySize.x / DisplaySize.y;

    DirectX::XMVECTOR Forward = DirectX::XMVectorSet(RenderCamera.Forward.x, RenderCamera.Forward.y, RenderCamera.Forward.z, 0.0f);
    DirectX::XMVECTOR Left = DirectX::XMVectorSet(RenderCamera.Left.x, RenderCamera.Left.y, RenderCamera.Left.z, 0.0f);
    DirectX::XMVECTOR UpDir= DirectX::XMVectorSet(RenderCamera.Up.x, RenderCamera.Up.y, RenderCamera.Up.z, 0.0f);
    DirectX::XMVECTOR NegEyePosition = DirectX::XMVectorSet(-RenderCamera.Position.x, -RenderCamera.Position.y, -RenderCamera.Position.z, 1.0f);

    DirectX::XMVECTOR D0 = DirectX::XMVector3Dot(Left, NegEyePosition); 
    DirectX::XMVECTOR D1 = DirectX::XMVector3Dot(UpDir, NegEyePosition);
    DirectX::XMVECTOR D2 = DirectX::XMVector3Dot(Forward, NegEyePosition);
    DirectX::XMMATRIX ViewMatrix;
    DirectX::XMVECTORU32 Mask = { { { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 } } };
    ViewMatrix.r[0] = DirectX::XMVectorSelect(D0, Left, Mask);
    ViewMatrix.r[1] = DirectX::XMVectorSelect(D1, UpDir, Mask);
    ViewMatrix.r[2] = DirectX::XMVectorSelect(D2, Forward, Mask);
    ViewMatrix.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    ViewMatrix = DirectX::XMMatrixTranspose(ViewMatrix);

    //DirectX::XMVECTOR EyePos = DirectX::XMVectorSet(RenderCamera.Position.x, RenderCamera.Position.y, RenderCamera.Position.z, 1.0);
    //DirectX::XMVECTOR LookAt = DirectX::XMVectorSet(TotalBounding.Center.x, TotalBounding.Center.y, TotalBounding.Center.z, 1.0);
    //UpDir = DirectX::XMVectorSet(0.0, 1.0, 0.0, 0.0);
    //ViewMatrix = DirectX::XMMatrixLookAtLH(EyePos, LookAt, UpDir);

    DirectX::XMMATRIX ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(RenderCamera.FovY, RenderCamera.AspectRatio, MAX(0.0000001f, RenderCamera.NearZ), MAX(0.0000001f, RenderCamera.FarZ));

    DirectX::XMMATRIX MVP = XMMatrixMultiply(ViewMatrix, ProjectionMatrix);

    _mm_storeu_ps(ConstantParams.WorldViewProjection[0], MVP.r[0]);
    _mm_storeu_ps(ConstantParams.WorldViewProjection[1], MVP.r[1]);
    _mm_storeu_ps(ConstantParams.WorldViewProjection[2], MVP.r[2]);
    _mm_storeu_ps(ConstantParams.WorldViewProjection[3], MVP.r[3]);


}

void MeshRenderer::RenderModel(const Float3& DisplaySize, ID3D12GraphicsCommandList* CommandList)
{
    if (MeshList.size() < 1 || RootSignature == nullptr) return;

    UpdateEveryFrameState(DisplaySize);
    
    ConstantParams.Copy();

    D3D12_VIEWPORT Viewport;
    memset(&Viewport, 0, sizeof(D3D12_VIEWPORT));
    Viewport.Width = DisplaySize.x;
    Viewport.Height = DisplaySize.y;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    Viewport.TopLeftX = Viewport.TopLeftY = 0.0f;
    CommandList->RSSetViewports(1, &Viewport);

    D3D12_RECT Scissor = {};
    Scissor.left = 0;
    Scissor.top = 0;
    Scissor.right = DisplaySize.x;
    Scissor.bottom = DisplaySize.y;
    CommandList->RSSetScissorRects(1, &Scissor);

    CommandList->SetGraphicsRootSignature(RootSignature);

    //Set Constant buffer
    D3D12_GPU_DESCRIPTOR_HANDLE GPUCBVHandle = D3dSrcDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    GPUCBVHandle.ptr += D3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    CommandList->SetGraphicsRootDescriptorTable(0, GPUCBVHandle);

    CommandList->SetPipelineState(SolidPipelineState);
    for (std::vector<Mesh>::iterator it = MeshList.begin(); it != MeshList.end(); it++)
    {
        CommandList->IASetIndexBuffer(&(it->Triangle.IndexBufferView));
        CommandList->IASetVertexBuffers(0, 1, &(it->Triangle.VertexBufferView));
        CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        CommandList->DrawIndexedInstanced(it->Triangle.IndexNum, 1, 0, 0, 0);

    }

    if (ShowWireFrame)
    {
        CommandList->SetPipelineState(WireFramePipelineState);
        for (std::vector<Mesh>::iterator it = MeshList.begin(); it != MeshList.end(); it++)
        {
            CommandList->IASetIndexBuffer(&(it->Triangle.IndexBufferView));
            CommandList->IASetVertexBuffers(0, 1, &(it->Triangle.VertexBufferView));
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            CommandList->DrawIndexedInstanced(it->Triangle.IndexNum, 1, 0, 0, 0);
        }
    }

    if (ShowFaceNormal)
    {
        CommandList->SetPipelineState(LinePipelineState);
        for (std::vector<Mesh>::iterator it = MeshList.begin(); it != MeshList.end(); it++)
        {
            CommandList->IASetIndexBuffer(&(it->FaceNormal.IndexBufferView));
            CommandList->IASetVertexBuffers(0, 1, &(it->FaceNormal.VertexBufferView));
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            CommandList->DrawIndexedInstanced(it->FaceNormal.IndexNum, 1, 0, 0, 0);
        }

    }

    if (ShowVertexNormal)
    {
        CommandList->SetPipelineState(LinePipelineState);
        for (std::vector<Mesh>::iterator it = MeshList.begin(); it != MeshList.end(); it++)
        {
            CommandList->IASetIndexBuffer(&(it->VertexNormal.IndexBufferView));
            CommandList->IASetVertexBuffers(0, 1, &(it->VertexNormal.VertexBufferView));
            CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            CommandList->DrawIndexedInstanced(it->VertexNormal.IndexNum, 1, 0, 0, 0);
        }

    }
}



