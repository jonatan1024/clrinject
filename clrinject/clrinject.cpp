#include <Windows.h>
#include <string.h>
#include "clrinject.h"

#define STRCPY(destination, source) for (int i = 0; ; i++) {\
	destination[i] = source[i];\
	if (!source[i]) break;\
}

struct RemoteDataStruct {
#ifndef _WIN64
	INT relocation;
#endif
	HMODULE(WINAPI *GetModuleHandleA)(LPCSTR);
	FARPROC(WINAPI *GetProcAddress)(HMODULE, LPCSTR);
	HANDLE(WINAPI *GetCurrentProcess)();
	DWORD(WINAPI *GetLastError)();
	InjectionOptions options;
	InjectionResult result;
};

#pragma runtime_checks("", off)

#define REMOTE_CODE_SECTION "remCode"
#define REMOTE_CONST_SECTION "remConst"

#pragma section(REMOTE_CODE_SECTION,read,execute)
#pragma const_seg(REMOTE_CONST_SECTION)
#pragma warning(disable:4102 4254)
#pragma comment(linker, "/merge:" REMOTE_CONST_SECTION "=" REMOTE_CODE_SECTION)
//if the string are not present in remConst section, use following pragma instead of the previous one
//#pragma comment(linker, "/merge:.rdata=" REMOTE_CODE_SECTION)
#pragma warning(default:4102 4254)

//late include to move all used constants into remConst
#include <metahost.h>
#import <mscorlib.tlb> raw_interfaces_only auto_rename

#ifndef _WIN64
#define CT(constant, type) ((type)((const byte *)constant + (RELOCATION)))
#define CA(constant) CT(constant, decltype(constant))
#define CC(constant) (*CA(&constant))
#define RELOCATION localData->relocation
#else
#define CC(constant) (constant)
#endif

#define REMOTE_ERROR_RET(cleanupLevel, message, returnValue) do {\
	result.status = localData->GetLastError();\
	STRCPY(result.statusMessage, CC(message));\
	retVal = returnValue;\
	goto cleanup ## cleanupLevel;\
} while(0)

#define REMOTE_ERROR(cleanupLevel, message) REMOTE_ERROR_RET(cleanupLevel, message, 1)
#define REMOTE_ERROR_STATUS(cleanupLevel, message) if(status != S_OK){\
	REMOTE_ERROR_RET(cleanupLevel, message, status);\
}

__declspec(code_seg("remCode")) __declspec(safebuffers)
static DWORD WINAPI RemoteProc(void * param) {
	RemoteDataStruct * localData = (RemoteDataStruct*)param;
	const InjectionOptions& options = localData->options;
	InjectionResult& result = localData->result;
	DWORD retVal = 0;

	HMODULE hMSCorEE = localData->GetModuleHandleA(CC("MSCorEE.dll"));
	if (!hMSCorEE)
		REMOTE_ERROR(0, "GetModuleHandleA of MSCorEE.dll failed!");
	HMODULE hOleAut32 = localData->GetModuleHandleA(CC("OleAut32.dll"));
	if (!hOleAut32)
		REMOTE_ERROR(0, "GetModuleHandleA of OleAut32.dll failed!");
	auto _CLRCreateInstance = (HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*))localData->GetProcAddress(hMSCorEE, CC("CLRCreateInstance"));
	if (!_CLRCreateInstance)
		REMOTE_ERROR(0, "GetProcAddress of MSCorEE/CLRCreateInstance failed!");
	auto _SysAllocString = (BSTR(WINAPI*)(const OLECHAR*))localData->GetProcAddress(hOleAut32, CC("SysAllocString"));
	if (!_SysAllocString)
		REMOTE_ERROR(0, "GetProcAddress of OleAut32/SysAllocString failed!");
	auto _SysFreeString = (void(WINAPI*)(BSTR))localData->GetProcAddress(hOleAut32, CC("SysFreeString"));
	if (!_SysFreeString)
		REMOTE_ERROR(0, "GetProcAddress of OleAut32/SysFreeString failed!");

	ICLRMetaHost* pCLRMetaHost = NULL;

	HRESULT status;

	status = _CLRCreateInstance(CC(CLSID_CLRMetaHost), CC(IID_ICLRMetaHost), (LPVOID*)&pCLRMetaHost);
	REMOTE_ERROR_STATUS(0, "CLRCreateInstance failed!");

	IEnumUnknown *pEnumUnknown = NULL;
	status = pCLRMetaHost->EnumerateLoadedRuntimes(localData->GetCurrentProcess(), &pEnumUnknown);
	REMOTE_ERROR_STATUS(1, "ICLRMetaHost->EnumerateLoadedRuntimes failed!");

	IUnknown* pCLRRuntimeInfoThunk = NULL;
	ULONG fetched = 0;

	int appDomainIndex = 1;

	while ((status = pEnumUnknown->Next(1, &pCLRRuntimeInfoThunk, &fetched)) == S_OK && fetched == 1) {
		if (result.numRuntimes >= MAX_RUNTIMES) {
			STRCPY(result.statusMessage, CC("Too many Runtimes!"));
			result.status = MAX_RUNTIMES;
			retVal = 2;
			goto cleanup2;
		}
		RuntimeInfo& resultRuntime = result.runtimes[result.numRuntimes++];

		ICLRRuntimeInfo* pCLRRuntimeInfo = NULL;
		status = pCLRRuntimeInfoThunk->QueryInterface(CC(IID_ICLRRuntimeInfo), (LPVOID*)&pCLRRuntimeInfo);
		REMOTE_ERROR_STATUS(2, "IUnknown->QueryInterface(ICLRRuntimeInfo) failed!");

		DWORD versionLength = sizeof(resultRuntime.version) / sizeof(wchar_t);
		status = pCLRRuntimeInfo->GetVersionString(resultRuntime.version, &versionLength);
		REMOTE_ERROR_STATUS(3, "ICLRRuntimeInfo->GetVersionString failed!");

		status = pCLRRuntimeInfo->IsStarted(&resultRuntime.started, &resultRuntime.startedFlags);
		REMOTE_ERROR_STATUS(3, "ICLRRuntimeInfo->IsStarted failed!");
		if (!resultRuntime.started)
			goto cleanup2;

		ICorRuntimeHost* pCorRuntimeHost = NULL;
		status = pCLRRuntimeInfo->GetInterface(CC(CLSID_CorRuntimeHost), CC(IID_ICorRuntimeHost), (LPVOID*)&pCorRuntimeHost);
		REMOTE_ERROR_STATUS(3, "ICLRRuntimeInfo->GetInterface(ICorRuntimeHost) failed!");

		HDOMAINENUM domainEnum;
		status = pCorRuntimeHost->EnumDomains(&domainEnum);
		REMOTE_ERROR_STATUS(4, "ICorRuntimeHost->EnumDomains failed!");

		IUnknown * pAppDomainThunk = NULL;
		while ((status = pCorRuntimeHost->NextDomain(domainEnum, &pAppDomainThunk)) == S_OK) {
			if (resultRuntime.numAppDomains >= MAX_APPDOMAINS) {
				STRCPY(result.statusMessage, CC("Too many AppDomains!"));
				result.status = MAX_APPDOMAINS;
				retVal = 2;
				goto cleanup5;
			}
			AppDomainInfo& resultAppDomain = resultRuntime.appDomains[resultRuntime.numAppDomains++];

			mscorlib::_AppDomain * pAppDomain = NULL;
			status = pAppDomainThunk->QueryInterface(CC(__uuidof(mscorlib::_AppDomain)), (LPVOID*)&pAppDomain);
			REMOTE_ERROR_STATUS(5, "IUnknown->QueryInterface(_AppDomain) failed!");

			BSTR friendlyName;
			status = pAppDomain->get_FriendlyName(&friendlyName);
			REMOTE_ERROR_STATUS(6, "_AppDomain->get_FriendlyName failed!");
			STRCPY(resultAppDomain.friendlyName, friendlyName);
			_SysFreeString(friendlyName);

			if (!options.enumerate && (!options.appDomainIndex || options.appDomainIndex == appDomainIndex)) {
				BSTR assemblyFile = _SysAllocString(options.assemblyFile);

				if (!options.typeName[0]) {
					status = pAppDomain->ExecuteAssembly_2(assemblyFile, &result.retVal);
					REMOTE_ERROR_STATUS(7, "_AppDomain->ExecuteAssembly failed!");
					resultAppDomain.injected = true;
				}
				else {
					BSTR typeName = _SysAllocString(options.typeName);
					mscorlib::_ObjectHandle * instance;
					status = pAppDomain->CreateInstanceFrom(assemblyFile, typeName, &instance);
					REMOTE_ERROR_STATUS(8, "_AppDomain->CreateInstanceFrom failed!");

					instance->Release();
					resultAppDomain.injected = true;

					cleanup8:
					_SysFreeString(typeName);
				}

			cleanup7:
				_SysFreeString(assemblyFile);
			}
		cleanup6:
			pAppDomain->Release();
		cleanup5:
			pAppDomainThunk->Release();

			appDomainIndex++;
		}
		if (status != S_FALSE) {
			REMOTE_ERROR_STATUS(4, "ICorRuntimeHost->NextDomain failed!");
		}
	cleanup4:
		pCorRuntimeHost->Release();
	cleanup3:
		pCLRRuntimeInfo->Release();
	cleanup2:
		pCLRRuntimeInfoThunk->Release();
	}
	pEnumUnknown->Release();
cleanup1:
	pCLRMetaHost->Release();
cleanup0:
	return retVal;
}

#define SANITY_CHECK(pointer) if(pointer < remoteSectionStart || pointer > remoteSectionEnd) {\
	sprintf(result->statusMessage, "Sanity check failed, pointer '" #pointer "' (%p) outside of remote section (%p - %p)", pointer, remoteSectionStart, remoteSectionEnd);\
	return false;\
}

__declspec(code_seg("remCode"))
static bool WINAPI SanityCheck(InjectionResult * result, const void * remoteSectionStart, SIZE_T remoteSectionLen) {
	const void * remoteSectionEnd = (const byte *)remoteSectionStart + remoteSectionLen;
	//functions
	SANITY_CHECK(RemoteProc);
	SANITY_CHECK(SanityCheck);
	//some constants
	SANITY_CHECK(&CLSID_CLRMetaHost);
	SANITY_CHECK(&__uuidof(mscorlib::_AppDomain));
	//some string literals
	SANITY_CHECK(&"sanity check");
	SANITY_CHECK(&__func__);
	return true;
}

#undef RELOCATION

#pragma const_seg()

#pragma runtime_checks("", restore)

#define INJECT_ERROR(cleanupLevel, message, ...) do {\
	result->status = GetLastError();\
	sprintf(result->statusMessage, message, __VA_ARGS__);\
	retVal = -1;\
	goto cleanup ## cleanupLevel;\
} while(0)

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

int Inject(const InjectionOptions * options, InjectionResult * result) {
	int retVal = 0;
	memset(result, 0, sizeof(InjectionResult));

	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, options->processId);
	if (!process)
		INJECT_ERROR(0, "OpenProcess on processId %d failed!", options->processId);

	void * remoteSection = NULL;
	SIZE_T remoteSectionLen = 0;

	IMAGE_NT_HEADERS * imageNtHeaders = (IMAGE_NT_HEADERS*)((const byte*)(&__ImageBase) + __ImageBase.e_lfanew);
	IMAGE_SECTION_HEADER * imageSectionHeader = (IMAGE_SECTION_HEADER*)(imageNtHeaders + 1);
	for (int i = 0; i < imageNtHeaders->FileHeader.NumberOfSections; i++) {
		if (!strncmp((const char *)imageSectionHeader[i].Name, REMOTE_CODE_SECTION, sizeof(REMOTE_CODE_SECTION))) {
			remoteSection = (void*)(imageSectionHeader[i].VirtualAddress + (const byte*)&__ImageBase);
			remoteSectionLen = imageSectionHeader[i].Misc.VirtualSize;
			break;
		}
	}
	if (!remoteSection)
		INJECT_ERROR(1, "Couldn't find '" REMOTE_CODE_SECTION "' section!");

	if (!SanityCheck(result, remoteSection, remoteSectionLen)) {
		retVal = -1;
		goto cleanup1;
	}

	LPVOID remoteSectionAddr = VirtualAllocEx(process, NULL, remoteSectionLen, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!remoteSectionAddr)
		INJECT_ERROR(1, "VirtualAllocEx for remote procedure code failed!");
	if (!WriteProcessMemory(process, remoteSectionAddr, remoteSection, remoteSectionLen, NULL))
		INJECT_ERROR(2, "WriteProcessMemory for remote procedure code failed!");

	RemoteDataStruct localData;
#ifndef _WIN64
	localData.relocation = (byte*)remoteSectionAddr - (byte*)remoteSection;
#endif
	localData.GetModuleHandleA = GetModuleHandleA;
	localData.GetProcAddress = GetProcAddress;
	localData.GetCurrentProcess = GetCurrentProcess;
	localData.GetLastError = GetLastError;

	memcpy(&localData.options, options, sizeof(InjectionOptions));
	memset(&localData.result, 0, sizeof(InjectionResult));

	LPVOID remoteDataAddr = VirtualAllocEx(process, NULL, sizeof(RemoteDataStruct), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!remoteDataAddr)
		INJECT_ERROR(2, "VirtualAllocEx for remote data failed!");
	if (!WriteProcessMemory(process, remoteDataAddr, &localData, sizeof(RemoteDataStruct), NULL))
		INJECT_ERROR(3, "WriteProcessMemory for remote data failed!");

	HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)((byte*)remoteSectionAddr + ((byte*)RemoteProc - (byte*)remoteSection)), remoteDataAddr, NULL, NULL);
	if (!thread)
		INJECT_ERROR(3, "CreateRemoteThread with remote procedure failed!");
	WaitForSingleObject(thread, INFINITE);
	DWORD remoteRetVal = 0;
	if (!GetExitCodeThread(thread, &remoteRetVal))
		INJECT_ERROR(4, "GetExitCodeThread of remote procedure failed!");

	retVal = remoteRetVal;
	if (!ReadProcessMemory(process, &((RemoteDataStruct*)remoteDataAddr)->result, result, sizeof(InjectionResult), NULL))
		INJECT_ERROR(4, "ReadProcessMemory for injection result failed!");

cleanup4:
	CloseHandle(thread);
cleanup3:
	VirtualFreeEx(process, remoteDataAddr, 0, MEM_RELEASE);
cleanup2:
	VirtualFreeEx(process, remoteSectionAddr, 0, MEM_RELEASE);
cleanup1:
	CloseHandle(process);
cleanup0:
	return retVal;
}