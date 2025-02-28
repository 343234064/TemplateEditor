// TemplateEditor.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "Editor\Processer.h"
#include "Editor\Editor.h"

static Editor* gEditor = nullptr;
static Processer* gProcesser = nullptr;

int main()
{
    gProcesser = new Processer();
    gEditor = new Editor(gProcesser);
    
    if (gEditor->Init(L"Template Editor", 100, 100, 1690, 960))
    {
        gEditor->GetMeshRenderer()->Init();
        gEditor->FileTypeList.push_back(FileType(L"SomeFile", L".SomeFile"));

        gEditor->Loop();

    }

    gEditor->Close();
    delete gEditor;

    delete gProcesser;
}


