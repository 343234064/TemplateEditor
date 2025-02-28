#pragma once

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <set>
#include <map>
#include <fstream>
#include <filesystem>
#include <functional>

#include "Utils.h"
#include "ThreadProcesser.h"

using namespace std;

class Processer;
typedef std::function<bool(Processer* InProcesser, std::string& State)> PassType;


struct DrawRawVertex
{
	DrawRawVertex() :
		pos(), normal(), color(), alpha(0.0f)
	{}
	DrawRawVertex(Float3 _pos, Float3 _nor, Float3 _col, float _alpha) :
		pos(_pos), normal(_nor), color(_col), alpha(_alpha)
	{}
	Float3 pos;
	Float3 normal;
	Float3 color;
	float alpha;
};
typedef unsigned int DrawRawIndex;



class SourceContext
{
public:
	SourceContext() :
		Name("Unknow"),
		DrawIndexList(nullptr),
		DrawFaceNormalIndexList(nullptr),
		DrawVertexNormalIndexList(nullptr),
		DrawVertexList(nullptr),
		DrawFaceNormalVertexList(nullptr),
		DrawVertexNormalVertexList(nullptr),
		CurrentPos1(0),
		CurrentPos2(0)
	{}
	virtual ~SourceContext()
	{
		Release();
	}


	virtual int GetTriangleNum() {
		return 0;
	}
	virtual int GetVertexNum() {
		return 0;
	}
	virtual bool Load(std::filesystem::path* InFilePath) {
		return true;
	}

	void Release()
	{
		if (DrawIndexList != nullptr)
			delete[] DrawIndexList;
		DrawIndexList = nullptr;

		if (DrawFaceNormalIndexList != nullptr)
			delete[] DrawFaceNormalIndexList;
		DrawFaceNormalIndexList = nullptr;

		if (DrawVertexNormalIndexList != nullptr)
			delete[] DrawVertexNormalIndexList;
		DrawVertexNormalIndexList = nullptr;

		if (DrawVertexList != nullptr)
			delete[] DrawVertexList;
		DrawVertexList = nullptr;

		if (DrawFaceNormalVertexList != nullptr)
			delete[] DrawFaceNormalVertexList;
		DrawFaceNormalVertexList = nullptr;

		if (DrawVertexNormalVertexList != nullptr)
			delete[] DrawVertexNormalVertexList;
		DrawVertexNormalVertexList = nullptr;
	}

public:
	std::string Name;
	BoundingBox Bounding;

	DrawRawIndex* DrawIndexList;
	DrawRawIndex* DrawFaceNormalIndexList;
	DrawRawIndex* DrawVertexNormalIndexList;
	DrawRawVertex* DrawVertexList;
	DrawRawVertex* DrawFaceNormalVertexList;
	DrawRawVertex* DrawVertexNormalVertexList;

	int CurrentPos1;
	int CurrentPos2;

};





class Processer
{
public:
	Processer() :
		AsyncProcesser(nullptr),
		ErrorString("")
	{
		AsyncProcesser = new ThreadProcesser();
	}
	virtual ~Processer()
	{

		if (AsyncProcesser != nullptr)
		{
			delete AsyncProcesser;
			AsyncProcesser = nullptr;
		}

		for (int i = 0; i < ContextList.size(); i++)
		{
			if(ContextList[i])
				delete ContextList[i];
		}
		ContextList.clear();

	}



public:

	virtual bool Import(std::filesystem::path* InFilePath)
	{
		std::cout << "Import:" << *InFilePath << std::endl;
		return true;
	}

	virtual bool Export(std::filesystem::path* InFilePath)
	{
		std::cout << "Export:" << *InFilePath << std::endl;
		return true;
	}

	

public:
	void AddData(void* Context)
	{
		if (AsyncProcesser && Context)
			AsyncProcesser->AddData(Context);
	}
	void BindRunFunc(void*(*RunFunc)(void*, double*), double IntervalTime)
	{
		if (!AsyncProcesser) return;

		std::function<void* (void*, double*)> Runnable = std::bind(RunFunc, std::placeholders::_1, std::placeholders::_2);
		AsyncProcesser->SetRunFunc(Runnable);
		AsyncProcesser->SetIntervalTime(IntervalTime);
	}
	bool Kick()
	{
		return (AsyncProcesser != nullptr) && AsyncProcesser->Kick();
	}

	double GetProgress()
	{
		if (AsyncProcesser == nullptr)
			return 0.0;

		double Progress = 0.0;
		SourceContext* Result = (SourceContext*)AsyncProcesser->GetResult(&Progress);
		//if (Result) ContextList.push_back(Result);

		return Progress;
	}

	std::string& GetErrorString()
	{
		return ErrorString;
	}

	void DumpErrorString(uint FileIndex)
	{
		std::string FileName = "error" + std::to_string(FileIndex);
		FileName += ".log";

		std::ofstream OutFile(FileName.c_str(), std::ios::out);
		OutFile << ErrorString;
		OutFile.close();
		ErrorString = "";
	}

	std::vector<SourceContext*>& GetContextList()
	{
		return ContextList;
	}

	bool IsWorking()
	{
		return (AsyncProcesser != nullptr && AsyncProcesser->IsWorking());
	}

	void Clear()
	{
		AsyncProcesser->Clear();

		ErrorString = "";
	}


public:
	std::vector<PassType> PassPool;

protected:
	ThreadProcesser* AsyncProcesser;
	std::string ErrorString;

	std::vector<SourceContext*> ContextList;

};
