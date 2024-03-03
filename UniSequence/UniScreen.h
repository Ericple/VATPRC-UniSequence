#pragma once
#include "pch.h"

class UniScreen :
	public EuroScopePlugIn::CRadarScreen
{
public:
	bool m_Opened;

	UniScreen(void) {
		m_Opened = true;
	};

	~UniScreen(void) {

	};

	virtual auto OnAsrContentToBeClosed(void) -> void {
		m_Opened = false;
		// delete will be done when attempting to access
	};
};
