// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "UniSequence.h"
using namespace EuroScopePlugIn;

UniSequence* pMyPlugIn = nullptr;

void __declspec (dllexport)
EuroScopePlugInInit(CPlugIn** ppPluginInstance)
{
	*ppPluginInstance = pMyPlugIn = new UniSequence;
}

void __declspec (dllexport)
EuroScopePlugInExit(void)
{
	pMyPlugIn->endLog();
	delete pMyPlugIn;
}