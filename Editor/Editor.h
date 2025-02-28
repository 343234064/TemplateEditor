#pragma once

#include <stdio.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <stdexcept>
#include <filesystem>


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <dxgi1_4.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

#include "Utils.h"
#include "Processer.h"







struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator;
    UINT64 FenceValue;
};



class HintText
{
public:
    HintText():
        Text(""), Color(ImVec4(1.0,1.0,1.0,1.0))
    {}
    
    void Error(const char* T)
    {
        Text = T;
        Color = ImVec4(1.0, 0.0, 0.0, 1.0);
    }

    void Normal(const char* T)
    {
        Text = T;
        Color = ImVec4(1.0, 1.0, 1.0, 1.0);
    }

    void ErrorColor()
    {
        Color = ImVec4(1.0, 0.0, 0.0, 1.0);
    }

    void NormalColor()
    {
        Color = ImVec4(1.0, 1.0, 1.0, 1.0);
    }

public:
    std::string Text;
    ImVec4 Color;
};


class Camera
{
public:
    Camera():
        Position(0.0),
        FovY(30.8),
        AspectRatio(1.0),
        NearZ(0.1),
        FarZ(10000.0),
        MoveSpeed(0.2),
        RotateSpeed(0.2),
        LookAt(0.0),
        Up(0.0,1.0,0.0)
    {
        UpdateDirection();
    }

    void Reset(BoundingBox& TotalModelBounding)
    {
        Position = TotalModelBounding.Center - Float3(TotalModelBounding.HalfLength.x, 0.0, TotalModelBounding.HalfLength.z) * 2.0f;
        LookAt = TotalModelBounding.Center;
        Up = Float3(0.0, 1.0, 0.0);
        UpdateDirection();
    }

    void UpdateDirection()
    {
        Forward = Normalize(LookAt - Position);
        Left = Normalize(Cross(Up, Forward));
        Up = Normalize(Cross(Forward, Left));
    }

    //void Rotate(Float3 Rotation)
    //{
    //    Float3 SinAngle;
    //    Float3 CosAngle;
    //    DirectX::XMScalarSinCos(&SinAngle.x, &CosAngle.x, Rotation.x * RotateSpeed);
    //    DirectX::XMScalarSinCos(&SinAngle.y, &CosAngle.y, Rotation.y * RotateSpeed);
    //    DirectX::XMScalarSinCos(&SinAngle.z, &CosAngle.z, Rotation.z * RotateSpeed);

    //    Float3 R1 = Float3(CosAngle.z * CosAngle.y + SinAngle.z * SinAngle.x * SinAngle.y, CosAngle.z * SinAngle.x * SinAngle.y - SinAngle.z * CosAngle.y, CosAngle.x * SinAngle.y);
    //    Float3 R2 = Float3(SinAngle.z * CosAngle.x, CosAngle.z * CosAngle.x, -SinAngle.x);
    //    Float3 R3 = Float3(SinAngle.z * SinAngle.x * CosAngle.y - CosAngle.z * SinAngle.y, SinAngle.z * SinAngle.y + CosAngle.z * SinAngle.x * CosAngle.y, CosAngle.x * CosAngle.y);

    //    Position = Float3(Dot(Position, R1), Dot(Position, R2), Dot(Position, R3));

    //}

    void Move(Float3 MoveOffset)
    {
        Float3 Origin = LookAt - Position;
        Float3 Trans = Float3(Dot(Origin, Left), Dot(Origin, Up), Dot(Origin, Forward));

        Trans = Trans + MoveOffset * MoveSpeed;

        LookAt = Float3(Dot(Trans, Float3(Left.x, Up.x, Forward.x)), Dot(Trans, Float3(Left.y, Up.y, Forward.y)), Dot(Trans, Float3(Left.z, Up.z, Forward.z))) + Position;
        Position = Position + (Left * MoveOffset.x + Up * MoveOffset.y + Forward * MoveOffset.z) * MoveSpeed;
    }

    void RotateAround(Float3 Direction)
    {
        Position = Position + Forward * Direction.z * MoveSpeed;
        float ViewLengthPrev = Length(Position - LookAt);
        Position = Position + (Left * Direction.x + Up * Direction.y) * MoveSpeed;

        Position = LookAt + Normalize(Position - LookAt) * ViewLengthPrev;
    }



public:
    Float3 Position;
    float FovY;
    float AspectRatio;
    float NearZ;
    float FarZ;
    float MoveSpeed;
    float RotateSpeed;

    Float3 LookAt;
    Float3 Forward;
    Float3 Left;
    Float3 Up;

};







class MeshData
{
public:
    MeshData() :
        VertexNum(0),
        VertexBuffer(nullptr)
    {
        IndexNum = 0;
        IndexBuffer = nullptr;
        IndexBufferView = {};
        VertexBufferView = {};
    }


    void Clear()
    {
        if (VertexBuffer) {
            VertexBuffer->Release();
            VertexBuffer = nullptr;
        }

        if (IndexBuffer) {
            IndexBuffer->Release();
            IndexBuffer = nullptr;
        }

        IndexNum = 0;
        VertexNum = 0;
    }

public:
    int IndexNum;
    int VertexNum;
    ID3D12Resource* IndexBuffer;
    ID3D12Resource* VertexBuffer;
    D3D12_INDEX_BUFFER_VIEW IndexBufferView;
    D3D12_VERTEX_BUFFER_VIEW VertexBufferView;

};
class Mesh
{
public:
    Mesh():
        Name("")
    {
    }


    void Clear()
    {
        Triangle.Clear();
        FaceNormal.Clear();
        VertexNormal.Clear();
    }

public:
    std::string Name;
    MeshData Triangle;
    MeshData FaceNormal;
    MeshData VertexNormal;

    BoundingBox Bounding;
};



class Constants
{
public:
    Constants() :
        GPUAddress(nullptr)
    {
        memset(WorldViewProjection, 0, sizeof(float) * 4 * 4);
    }

    size_t Size()
    {
        return sizeof(float) * 4 * 4;
    }

    void Copy()
    {
        if (GPUAddress)
            memcpy(GPUAddress, WorldViewProjection, Size());
    }

public:
    void* GPUAddress;
    float  WorldViewProjection[4][4];
};



class MeshRenderer
{
public:
    MeshRenderer(ID3D12Device* device, ID3D12DescriptorHeap* d3dSrcDescHeap):
        D3dDevice(device),
        D3dSrcDescriptorHeap(d3dSrcDescHeap),
        RootSignature(nullptr),
        SolidPipelineState(nullptr),
        WireFramePipelineState(nullptr),
        LinePipelineState(nullptr),
        ConstantsBuffer(nullptr),
        Signal(false)
    {
        ShowWireFrame = false;
        ShowFaceNormal = false;
        ShowVertexNormal = false;

 
    }
    ~MeshRenderer()
    {
        ClearResource();
        D3dDevice = nullptr;

        Release();
    }

    bool Init(D3D12_RASTERIZER_DESC* InRasterizerDesc = nullptr, D3D12_BLEND_DESC* InBlendDesc = nullptr)
    {
        if (!InitRenderPipeline(InRasterizerDesc, InBlendDesc))
        {
            Hint.Error("Init Renderer Error");
            return false;
        }
        return true;
    }


    void LoadMesh(Processer* InProcesser)
    {
        bool Success = LoadMeshFromProcesser(InProcesser);
        if (!Success)
        {
            Hint.Error("Load Mesh Failed.");
        }
        Signal = false;
    }
    bool NeedWaitForLastSumittedFrame()
    {
        return Signal;
    }
    void KickShowModel()
    {
        Signal = true;
    }
    
    void RenderModel(const Float3& DisplaySize, ID3D12GraphicsCommandList* CommandList);
    void ShowViewerSettingUI(bool ModelIsLoaded, bool GeneratorIsWorking);
    
    Camera* GetCamera()
    {
        return &RenderCamera;
    }

private:
    bool InitRenderPipeline(D3D12_RASTERIZER_DESC* InRasterizerDesc = nullptr, D3D12_BLEND_DESC* InBlendDesc = nullptr);
    void ClearResource();
    void Release()
    {
        if (ConstantsBuffer)
        {
            ConstantsBuffer->Release();
            ConstantsBuffer = nullptr;
        }
        if (SolidPipelineState) {
            SolidPipelineState->Release();
            SolidPipelineState = nullptr;
        }
        if (WireFramePipelineState) {
            WireFramePipelineState->Release();
            WireFramePipelineState = nullptr;
        }
        if (LinePipelineState) {
            LinePipelineState->Release();
            LinePipelineState = nullptr;
        }
        if (RootSignature) {
            RootSignature->Release();
            RootSignature = nullptr;
        }
    }

    void UpdateEveryFrameState(const Float3& DisplaySize);
    bool LoadMeshFromProcesser(Processer* InProcesser);




public:
    HintText Hint;
    bool ShowWireFrame;
    bool ShowFaceNormal;
    bool ShowVertexNormal;


private:
    std::vector<Mesh> MeshList;
    Constants ConstantParams;
    Camera RenderCamera;
    BoundingBox TotalBounding;
    bool Signal;

private:
    ID3D12Device* D3dDevice;
    ID3D12DescriptorHeap* D3dSrcDescriptorHeap;

    ID3D12RootSignature* RootSignature;
    ID3D12PipelineState* SolidPipelineState;
    ID3D12PipelineState* WireFramePipelineState;
    ID3D12PipelineState* LinePipelineState;
    ID3D12Resource* ConstantsBuffer;



};




struct FileType
{
    FileType(LPCWSTR InName, LPCWSTR InType):
        Name(InName), Type(InType)
    {}

    LPCWSTR Name;
    LPCWSTR Type;
};

class Editor
{
public:
    Editor(Processer* InProcesser);
    ~Editor();


    bool Init(const wchar_t* ApplicationName, int PosX, int PosY, UINT Width, UINT Height);
    void Loop();
    void Close();

    void SetNeedWaitForSumittedFrame(bool v) { NeedWaitLastFrame = v; }

    MeshRenderer* GetMeshRenderer() const
    {
        return MeshViewer.get();
    }

public:

    std::function<void(Processer* InProcesser, HintText* State)> CallBackOnRender;
    std::function<void(Processer* InProcesser, HintText* State)> CallBackOnTick;
    std::function<void(Processer* InProcesser, std::filesystem::path* FilePath, HintText* State)> CallBackOnLoadModel;
    std::function<void(Processer* InProcesser, HintText* State)> CallBackOnLastFrameFinishedRender;
    std::function<void(Processer* InProcesser, HintText* State)> CallBackOnInspectorUI;
    std::function<void(Processer* InProcesser, HintText* State)> CallBackOnCustomUI;
    std::function<void(Processer* InProcesser, HintText* State)> CallBackOnAllProgressFinished;

private:
    void OnLoadButtonClicked();
    void OnGenerateButtonClicked();
    void OnOneProgressFinished();
    void OnAllProgressFinished();
    void OnExportButtonClicked();

private:
    void Render(ID3D12GraphicsCommandList* CommandList);
    void RenderBackground(ID3D12GraphicsCommandList* CommandList);
    double GetProgress();

    bool NeedWaitForSumittedFrame()
    {
        bool MeshViewerWait = MeshViewer.get() && MeshViewer->NeedWaitForLastSumittedFrame();
        return MeshViewerWait || NeedWaitLastFrame;
    }

    void OnLastFrameFinished()
    {
        if (MeshViewer.get() && MeshViewer->NeedWaitForLastSumittedFrame())
            MeshViewer->LoadMesh(ExternalProcesser);

        if (CallBackOnLastFrameFinishedRender != nullptr)
            CallBackOnLastFrameFinishedRender(ExternalProcesser, &Hint);
    }


public:
    std::vector<FileType> FileTypeList;

private:
    void BrowseFileOpen(std::filesystem::path* OutFilePath);
    void BrowseFileSave(std::filesystem::path* OutFilePath);
    void KickGenerateMission();
    void ClearPassPool();


private:
    WNDCLASSEXW WndClass;
    HWND HWnd;

private:
    Processer* ExternalProcesser;

private:
    std::unique_ptr<MeshRenderer> MeshViewer;
    std::queue<PassType> PassPool;
    double Progress;

    std::filesystem::path ImportFilePath;
    std::filesystem::path ExportFilePath;
    HintText Hint;

    bool NeedWaitLastFrame;
    bool Terminated;
    bool Working;
    bool Imported;
    bool PreLoad;
    int CurrentPassIndex;

    bool Inited;

};




