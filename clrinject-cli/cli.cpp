#include <Windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN64
#pragma comment(lib, "clrinject.lib")
#else
#pragma comment(lib, "clrinject64.lib")
#endif
#include <clrinject.h>

DWORD GetProcessIdByName(const char * processName) {
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (_stricmp(entry.szExeFile, processName) == 0)
			{
				CloseHandle(snapshot);
				return entry.th32ProcessID;
			}
		}
	}

	CloseHandle(snapshot);
	return 0;
}

void usage() {
	fprintf(stderr,
		"Usage:\n"
		"\tclrinject-cli.exe -p <processId/processName> -a <assemblyFile>\n\n"
		"Addition options:\n"
		"\t-e\tEnumerate Runtimes and AppDomains.\n"
		"\t-d #\tInject only into #-th AppDomain. Default = 0, inject into every AppDomain.\n"
		"\t-i X.Y\tCreate an instance of class Y from namespace X.\n"
	);
	exit(1);
}

class SZStringToOleString {
	OLECHAR * data;
public:
	SZStringToOleString(const char * in_data) {
		data = new OLECHAR[strlen(in_data) + 1];
		for (size_t i = 0; i <= strlen(in_data); i++) {
			data[i] = in_data[i];
		}
	}
	operator OLECHAR*() {
		return data;
	}
	~SZStringToOleString() {
		delete[] data;
	}
};

int main(int argc, char** argv) {
	InjectionOptions options = {};
	char * processName = NULL;
	char * assemblyFile = NULL;
	char * typeName = NULL;

	//handle command line arguments
	int argi = 0;
	while (++argi < argc) {
		if (argv[argi][0] == '-') {
			switch (argv[argi][1])
			{
			case 'p':
				if (argi + 1 < argc) {
					argi++;
					processName = argv[argi];
					continue;
				}
				break;
			case 'a':
				if (argi + 1 < argc) {
					argi++;
					assemblyFile = argv[argi];
					continue;
				}
				break;
			case 'e':
				options.enumerate = true;
				continue;
			case 'd':
				if (argi + 1 < argc) {
					argi++;
					options.appDomainIndex = strtol(argv[argi], NULL, 10);
					continue;
				}
			case 'i':
				if (argi + 1 < argc) {
					argi++;
					typeName = argv[argi];
					continue;
				}
				break;
			}
		}
		fprintf(stderr, "Unexpected argument '%s'!\n\n", argv[argi]);
		usage();
	}

	//check arguments validity
	if (!processName || (!assemblyFile && !options.enumerate)) {
		fprintf(stderr, "Process or assembly file was not specified!\n\n");
		usage();
	}

	//find processId
	DWORD processId = GetProcessIdByName(processName);
	if (!processId)
		processId = strtol(processName, NULL, 10);

	if (!processId) {
		fprintf(stderr, "Failed to get id for process name '%s'!\n", processName);
		return 1;
	}

	options.processId = processId;
	if(assemblyFile)
		lstrcpynW(options.assemblyFile, SZStringToOleString(assemblyFile), MAX_PATH);
	if(typeName)
		lstrcpynW(options.typeName, SZStringToOleString(typeName), 256);

	InjectionResult result;
	int retVal = Inject(&options, &result);
	if (options.enumerate) {
		printf("Enumeration:\n");
		int index = 1;
		for (int i = 0; i < result.numRuntimes; i++) {
			const RuntimeInfo& runtime = result.runtimes[i];
			printf("#\tRuntime Version: '%ls', %s\n", runtime.version, runtime.started ? "is started" : "is not started");
			for (int j = 0; j < runtime.numAppDomains; j++) {
				const AppDomainInfo& appDomain = runtime.appDomains[j];
				printf("#%d\t\tAppDomain Name: '%ls'\n", index++, appDomain.friendlyName);
			}
		}
		printf("\n");
	}
	else {
		printf("Injection succeeded for following Runtimes and AppDomains:\n");
		for (int i = 0; i < result.numRuntimes; i++) {
			const RuntimeInfo& runtime = result.runtimes[i];
			for (int j = 0; j < runtime.numAppDomains; j++) {
				const AppDomainInfo& appDomain = runtime.appDomains[j];
				if(appDomain.injected)
					printf("\tRuntime '%ls', AppDomain '%ls'\n", runtime.version, appDomain.friendlyName);
			}
		}
		printf("Injection failed for following Runtimes and AppDomains:\n");
		int index = 1;
		for (int i = 0; i < result.numRuntimes; i++) {
			const RuntimeInfo& runtime = result.runtimes[i];
			for (int j = 0; j < runtime.numAppDomains; j++) {
				const AppDomainInfo& appDomain = runtime.appDomains[j];
				if (!appDomain.injected && (!options.appDomainIndex || options.appDomainIndex == index))
					printf("\tRuntime '%ls', AppDomain '%ls'\n", runtime.version, appDomain.friendlyName);
				index++;
			}
		}
		printf("\n");
	}

	if (retVal) {
		if (result.status || result.statusMessage[0])
			fprintf(stderr, "Injection failed, return value: 0x%08X, code: 0x%08X, reason: '%s'!\n", retVal, result.status, result.statusMessage);
		else
			fprintf(stderr, "Injection failed, return value: 0x%08X!\n", retVal);
		return 1;
	}

	if (!options.enumerate)
		printf("Injection successful, return value: 0x%08X\n", result.retVal);

	return 0;
}
