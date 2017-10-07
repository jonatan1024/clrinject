#include <Windows.h>
#include <string.h>
#include <intrin.h>
#include "clrinject.h"

struct dataStruct {
	void * RemoteProc;
	HMODULE(__stdcall *GetModuleHandleA)(LPCSTR);
	FARPROC(__stdcall *GetProcAddress)(HMODULE, LPCSTR);
	HANDLE(__stdcall *GetCurrentProcess)();
	IID IID_IAppDomain;
	InjectionOptions options;
	InjectionResult result;
};

#pragma runtime_checks("", off)

#define REMOTE_CODE_SECTION "remCode"
#define REMOTE_CONST_SECTION "remConst"

#pragma section(REMOTE_CODE_SECTION,read,execute)
#pragma const_seg(REMOTE_CONST_SECTION)
#pragma comment(linker, "/merge:" REMOTE_CONST_SECTION "=" REMOTE_CODE_SECTION)
//if the string are not present in remConst section, use following pragma instead of the previous one
//#pragma comment(linker, "/merge:.rdata=" REMOTE_CODE_SECTION)

//late include to move all used constants into remConst
#include <metahost.h>
#import <mscorlib.tlb> raw_interfaces_only auto_rename

#define CT(constant, type) ((type)((const byte *)RUNTIME_BASE + (((const byte *)constant) - (const byte *)COMPILE_BASE)))
#define CA(constant) CT(constant, decltype(constant))
#define CC(constant) (*CA(&constant))
#define COMPILE_BASE RemoteProc
#define RUNTIME_BASE localData->RemoteProc

__declspec(code_seg("remCode")) __declspec(safebuffers)
static DWORD WINAPI RemoteProc(void * param) {
	dataStruct * localData = (dataStruct*)param;
		
	HMODULE hMSCorEE = localData->GetModuleHandleA(CC("MSCorEE.dll"));
	HMODULE hOleAut32 = localData->GetModuleHandleA(CC("OleAut32.dll"));
	auto _CLRCreateInstance = (HRESULT(__stdcall*)(REFCLSID, REFIID, LPVOID*))localData->GetProcAddress(hMSCorEE, CC("CLRCreateInstance"));
	auto _SysAllocString = (BSTR(__stdcall*)(const OLECHAR*))localData->GetProcAddress(hOleAut32, CC("SysAllocString"));
	auto _SysFreeString = (void(__stdcall*)(BSTR))localData->GetProcAddress(hOleAut32, CC("SysFreeString"));

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
	return 0;
}

#undef COMPILE_BASE
#undef RUNTIME_BASE

#pragma const_seg()

#pragma runtime_checks("", restore)

#define INJECT_ERROR(cleanupLevel, message, ...) do{\
	result->status = GetLastError();\
	sprintf(result->statusMessage, message, __VA_ARGS__);\
	retVal = -1;\
	goto cleanup ## cleanupLevel;\
}while(0)

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

int Inject(const InjectionOptions * options, InjectionResult * result) {
	int retVal = 0;
	memset(result, 0, sizeof(InjectionResult));

	HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, options->processId);
	if (!process)
		INJECT_ERROR(0, "OpenProcess on processId %d failed!", options->processId);

	//todo doc and errorcheck

	void * bingoBaseAddress;
	SIZE_T bingoRegionSize;

	IMAGE_NT_HEADERS * imageNtHeaders = (IMAGE_NT_HEADERS*)((const byte*)(&__ImageBase) + __ImageBase.e_lfanew);
	IMAGE_SECTION_HEADER * imageSectionHeader = (IMAGE_SECTION_HEADER*)(imageNtHeaders + 1);
	for (int i = 0; i < imageNtHeaders->FileHeader.NumberOfSections; i++) {
		if (!strncmp((const char *)imageSectionHeader[i].Name, REMOTE_CODE_SECTION, sizeof(REMOTE_CODE_SECTION))) {
			bingoBaseAddress = (void*)(imageSectionHeader[i].VirtualAddress + (const byte*)&__ImageBase);
			bingoRegionSize = imageSectionHeader[i].Misc.VirtualSize;
			break;
		}
	}
	
	LPVOID remoteProcAddr = VirtualAllocEx(process, NULL, bingoRegionSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!remoteProcAddr)
		INJECT_ERROR(1, "VirtualAllocEx for remote procedure code failed!");
	if (!WriteProcessMemory(process, remoteProcAddr, bingoBaseAddress, bingoRegionSize, NULL))
		INJECT_ERROR(2, "WriteProcessMemory for remote procedure code failed!");

	dataStruct localData;
	localData.RemoteProc = ((byte*)remoteProcAddr) + ((byte*)RemoteProc - (byte*)bingoBaseAddress);
	localData.GetModuleHandleA = GetModuleHandleA;
	localData.GetProcAddress = GetProcAddress;
	localData.GetCurrentProcess = GetCurrentProcess;
	memcpy(&localData.IID_IAppDomain, &__uuidof(mscorlib::_AppDomain), sizeof(IID));

	memcpy(&localData.options, options, sizeof(InjectionOptions));
	memset(&localData.result, 0, sizeof(InjectionResult));

	LPVOID remoteDataAddr = VirtualAllocEx(process, NULL, sizeof(dataStruct), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!remoteDataAddr)
		INJECT_ERROR(2, "VirtualAllocEx for remote data failed!");
	if (!WriteProcessMemory(process, remoteDataAddr, &localData, sizeof(dataStruct), NULL))
		INJECT_ERROR(3, "WriteProcessMemory for remote data failed!");

	HANDLE thread = CreateRemoteThread(process, NULL, 0, (LPTHREAD_START_ROUTINE)((byte*)remoteProcAddr + ((byte*)RemoteProc - (byte*)bingoBaseAddress)), remoteDataAddr, NULL, NULL);
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
	VirtualFreeEx(process, remoteProcAddr, 0, MEM_RELEASE);
cleanup1:
	CloseHandle(process);
cleanup0:
	return retVal;
}