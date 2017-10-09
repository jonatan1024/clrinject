#pragma once
#include <Windows.h>

#define MAX_RUNTIMES 8
#define MAX_APPDOMAINS 32

struct AppDomain {
	OLECHAR friendlyName[256];
	bool injected;
};

struct Runtime {
	WCHAR version[32];
	BOOL started;
	DWORD startedFlags;

	int numAppDomains;
	AppDomain appDomains[MAX_APPDOMAINS];
};

struct InjectionResult {
	long retVal;
	DWORD status;
	char statusMessage[256];

	int numRuntimes;
	Runtime runtimes[MAX_RUNTIMES];
};

struct InjectionOptions {
	bool enumerate;
	int appDomainIndex;
	DWORD processId;
	OLECHAR assemblyFile[MAX_PATH];
};

int Inject(const InjectionOptions * options, InjectionResult * result);