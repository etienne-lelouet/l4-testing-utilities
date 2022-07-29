#pragma once
#include <uv.h>
#include <stdio.h>
#include "argsparser.h"

class Sender
{
public:
	virtual void run() =0;
	virtual void stop() =0;
};
