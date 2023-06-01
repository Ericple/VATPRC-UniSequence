#include "pch.h"
#include "UniSequence.h"
using namespace EuroScopePlugIn;

UniSequence* pPlugIn = nullptr;

void __declspec (dllexport)
EuroScopePlugInInit(CPlugIn** ppPluginInstance)
{
	*ppPluginInstance = pPlugIn = new UniSequence;
}

void __declspec (dllexport)
EuroScopePlugInExit(void)
{
	delete pPlugIn;
}