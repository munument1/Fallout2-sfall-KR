/*
 *    sfall
 *    Korean CP949 text renderer experiment
 */

#pragma once

#include "Module.h"

namespace sfall
{

class KoreanText : public Module {
public:
	const char* name() { return "KoreanText"; }
	static bool IsConfigured();
	void init();
	void exit() override;
};

}
