#pragma once
#include <Windows.h>

struct AppDomain {

};

struct InjectionResult {
	long retVal;
	DWORD status;
	char statusMessage[256];
	AppDomain enumeration;
};

struct InjectionOptions {
	DWORD processId;
	OLECHAR assemblyFile[MAX_PATH];
};

int Inject(const InjectionOptions * options, InjectionResult * result);