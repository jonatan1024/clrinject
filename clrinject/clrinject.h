#pragma once
#include <Windows.h>

#define MAX_RUNTIMES 8
#define MAX_APPDOMAINS 32

struct AppDomainInfo {
	OLECHAR friendlyName[256];
	bool injected;
};

struct RuntimeInfo {
	WCHAR version[32];
	BOOL started;
	DWORD startedFlags;

	int numAppDomains;
	AppDomainInfo appDomains[MAX_APPDOMAINS];
};

struct InjectionResult {
	long retVal;
	DWORD status;
	char statusMessage[256];

	int numRuntimes;
	RuntimeInfo runtimes[MAX_RUNTIMES];
};

struct InjectionOptions {
	bool enumerate;
	int appDomainIndex;
	DWORD processId;
	OLECHAR assemblyFile[MAX_PATH];
	OLECHAR typeName[256];
};

int Inject(const InjectionOptions * options, InjectionResult * result);