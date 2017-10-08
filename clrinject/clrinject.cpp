#include <Windows.h>
#include <string.h>
#include <intrin.h>
#include "clrinject.h"

#define STRCPY(destination, source) for (int i = 0; ; i++) {\
	destination[i] = source[i];\
	if (!source[i]) break;\
}

struct dataStruct {
	int relocation;
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
#pragma warning(disable:4102)
#pragma comment(linker, "/merge:" REMOTE_CONST_SECTION "=" REMOTE_CODE_SECTION)
//if the string are not present in remConst section, use following pragma instead of the previous one
//#pragma comment(linker, "/merge:.rdata=" REMOTE_CODE_SECTION)
#pragma warning(default:4102)

//late include to move all used constants into remConst
#include <metahost.h>
#import <mscorlib.tlb> raw_interfaces_only auto_rename

#define CT(constant, type) ((type)((const byte *)constant + (RELOCATION)))
#define CA(constant) CT(constant, decltype(constant))
#define CC(constant) (*CA(&constant))
#define RELOCATION localData->relocation

#define REMOTE_ERROR(cleanupLevel, message) do {\
	localData->result.status = localData->GetLastError();\
	STRCPY(localData->result.statusMessage, message);\
	retVal = 1;\
	goto cleanup ## cleanupLevel;\
} while(0)

__declspec(code_seg("remCode")) __declspec(safebuffers)
static DWORD WINAPI RemoteProc(void * param) {
	dataStruct * localData = (dataStruct*)param;
	DWORD retVal = 0;

	HMODULE hMSCorEE = localData->GetModuleHandleA(CC("MSCorEE.dll"));
	HMODULE hOleAut32 = localData->GetModuleHandleA(CC("OleAut32.dll"));
	auto _CLRCreateInstance = (HRESULT(WINAPI*)(REFCLSID, REFIID, LPVOID*))localData->GetProcAddress(hMSCorEE, CC("CLRCreateInstance"));
	auto _SysAllocString = (BSTR(WINAPI*)(const OLECHAR*))localData->GetProcAddress(hOleAut32, CC("SysAllocString"));
	auto _SysFreeString = (void(WINAPI*)(BSTR))localData->GetProcAddress(hOleAut32, CC("SysFreeString"));

	ICLRMetaHost* pCLRMetaHost = NULL;

	HRESULT status;

	status = _CLRCreateInstance(CC(CLSID_CLRMetaHost), CC(IID_ICLRMetaHost), (LPVOID*)&pCLRMetaHost);
	if (status != S_OK)
		return status;

	IEnumUnknown *pEnumUnknown = NULL;
	status = pCLRMetaHost->EnumerateLoadedRuntimes(localData->GetCurrentProcess(), &pEnumUnknown);

	IUnknown* pCLRRuntimeInfoThunk = NULL;
	ULONG fetched = 0;

	while (pEnumUnknown->Next(1, &pCLRRuntimeInfoThunk, &fetched) == S_OK && fetched == 1) {
		ICLRRuntimeInfo* pCLRRuntimeInfo = NULL;
		status = pCLRRuntimeInfoThunk->QueryInterface(CC(IID_ICLRRuntimeInfo), (LPVOID*)&pCLRRuntimeInfo);

		BOOL started;
		DWORD startedFlags;
		status = pCLRRuntimeInfo->IsStarted(&started, &startedFlags);

		if (!started)
			continue;

		ICorRuntimeHost* pCorRuntimeHost = NULL;
		status = pCLRRuntimeInfo->GetInterface(CC(CLSID_CorRuntimeHost), CC(IID_ICorRuntimeHost), (LPVOID*)&pCorRuntimeHost);

		//status = pCorRuntimeHost->Start();

		HDOMAINENUM domainEnum;
		status = pCorRuntimeHost->EnumDomains(&domainEnum);
		IUnknown * pAppDomainThunk = NULL;
		while (pCorRuntimeHost->NextDomain(domainEnum, &pAppDomainThunk) == S_OK) {
			mscorlib::_AppDomain * pAppDomain = NULL;
			status = pAppDomainThunk->QueryInterface(CC(__uuidof(mscorlib::_AppDomain)), (LPVOID*)&pAppDomain);

			BSTR assemblyFile = _SysAllocString(localData->options.assemblyFile);
			pAppDomain->ExecuteAssembly_2(assemblyFile, &localData->result.retVal);

			_SysFreeString(assemblyFile);
			pAppDomain->Release();
			pAppDomainThunk->Release();
		}
		pCorRuntimeHost->Release();
		pCLRRuntimeInfo->Release();
		pCLRRuntimeInfoThunk->Release();
	}
	pEnumUnknown->Release();
	pCLRMetaHost->Release();
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

	dataStruct localData;
	localData.relocation = (byte*)remoteSectionAddr - (byte*)remoteSection;
	localData.GetModuleHandleA = GetModuleHandleA;
	localData.GetProcAddress = GetProcAddress;
	localData.GetCurrentProcess = GetCurrentProcess;
	localData.GetLastError = GetLastError;

	memcpy(&localData.options, options, sizeof(InjectionOptions));
	memset(&localData.result, 0, sizeof(InjectionResult));

	LPVOID remoteDataAddr = VirtualAllocEx(process, NULL, sizeof(dataStruct), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!remoteDataAddr)
		INJECT_ERROR(2, "VirtualAllocEx for remote data failed!");
	if (!WriteProcessMemory(process, remoteDataAddr, &localData, sizeof(dataStruct), NULL))
		INJECT_ERROR(3, "WriteProcessMemory for remote data failed!");

	HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)((byte*)remoteSectionAddr + ((byte*)RemoteProc - (byte*)remoteSection)), remoteDataAddr, NULL, NULL);
	if (!thread)
		INJECT_ERROR(3, "CreateRemoteThread with remote procedure failed!");
	WaitForSingleObject(thread, INFINITE);
	DWORD remoteRetVal = 0;
	if (!GetExitCodeThread(thread, &remoteRetVal))
		INJECT_ERROR(4, "GetExitCodeThread of remote procedure failed!");

	retVal = remoteRetVal;
	if (!ReadProcessMemory(process, &((dataStruct*)remoteDataAddr)->result, result, sizeof(InjectionResult), NULL))
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