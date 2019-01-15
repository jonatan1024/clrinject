#include <Windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <atlbase.h>
#include "msclr\marshal_cppstd.h"
#include "clrinject-lib.h"
#include "../clrinject/clrinject.h"

using namespace System::Runtime::InteropServices;

private class SZStringToOleString {
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

System::Collections::Generic::List<EnumerationResult>^ Injector::EnumerateAppDomains(int processId)
{
	InjectionOptions options = {};
	options.enumerate = true;
	options.processId = processId;
	return EnumerateAppDomains(options);
}

System::Collections::Generic::List<EnumerationResult>^ Injector::EnumerateAppDomains(String ^ processName)
{
	InjectionOptions options = {};
	int processId = GetProcessIdFromName(processName);
	options.enumerate = true;
	options.processId = processId;
	return EnumerateAppDomains(options);
}

void Injector::InjectIntoProcess(int processId, String^ pathOfAssemblyToBeInjected)
{
	InjectionOptions options = {};
	options.processId = processId;
	std::string pathOfAssemblyToBeInjectedString = msclr::interop::marshal_as<std::string>(pathOfAssemblyToBeInjected);
	lstrcpynW(options.assemblyFile, SZStringToOleString(pathOfAssemblyToBeInjectedString.c_str()), MAX_PATH);
	//options.appDomainIndex = 1;
	InjectionResult result = InvokeInjection(options);
}

void Injector::InjectIntoProcess(String ^ processName, String^ pathOfAssemblyToBeInjected)
{
	int processId = GetProcessIdFromName(processName);
	return Injector::InjectIntoProcess(processId, pathOfAssemblyToBeInjected);
}

void Injector::InjectIntoProcess(int processId, String^ pathOfAssemblyToBeInjected, int appDomainNo)
{
	InjectionOptions options = {};
	options.processId = processId;
	std::string pathOfAssemblyToBeInjectedString = msclr::interop::marshal_as<std::string>(pathOfAssemblyToBeInjected);
	lstrcpynW(options.assemblyFile, SZStringToOleString(pathOfAssemblyToBeInjectedString.c_str()), MAX_PATH);
	options.appDomainIndex = appDomainNo;
	InjectionResult result = InvokeInjection(options);
}

void Injector::InjectIntoProcess(String ^ processName, String^ pathOfAssemblyToBeInjected, int appDomainNo)
{
	int processId = GetProcessIdFromName(processName);
	return Injector::InjectIntoProcess(processId, pathOfAssemblyToBeInjected, appDomainNo);
}

System::Collections::Generic::List<EnumerationResult> ^ Injector::EnumerateAppDomains(InjectionOptions &options)
{
	InjectionResult result = InvokeInjection(options);
	return ExtractEnumerationInfo(result);
}

InjectionResult InvokeInjection(InjectionOptions &options)
{
	InjectionResult result;
	int retVal = Inject(&options, &result);
	ResultGuard(retVal, result);
	return result;
}

System::Collections::Generic::List<EnumerationResult>^ ExtractEnumerationInfo(InjectionResult &result)
{
	System::Collections::Generic::List<EnumerationResult> ^output = gcnew System::Collections::Generic::List<EnumerationResult>();
	for (int i = 0; i < result.numRuntimes; i++) {
		const RuntimeInfo& runtime = result.runtimes[i];

		EnumerationResult enumeration;
		enumeration.runtimeVersion = gcnew String(runtime.version);
		enumeration.IsRuntimeStarted = runtime.started;
		enumeration.AppDomains = gcnew System::Collections::Generic::List<String^>();

		for (int index = 0; index < runtime.numAppDomains; index++) {
			const AppDomainInfo& appDomain = runtime.appDomains[index];
			enumeration.AppDomains->Add(gcnew String(appDomain.friendlyName));
		}

		output->Add(enumeration);
	}

	return output;
}

int GetProcessIdFromName(String ^ processName)
{
	String ^ processNameWithoutExtension = processName->Substring(0, processName->LastIndexOf('.'));
	array<System::Diagnostics::Process^> ^processes = System::Diagnostics::Process::GetProcessesByName(processNameWithoutExtension);

	if (processes->Length == 0) {
		throw gcnew System::ApplicationException("Failed to get id for process name" + processNameWithoutExtension);;
	}

	return processes[0]->Id;
}

void ResultGuard(int retVal, InjectionResult &result)
{
	if (retVal) {
		if (result.status || result.statusMessage[0])
			throw gcnew System::ApplicationException(System::String::Format(L"Injection failed, return value: {0}, code: {1}, reason: {2}", gcnew String(retVal.ToString()), gcnew String(result.status.ToString()), gcnew String(result.statusMessage)));
		else
			throw gcnew System::ApplicationException(System::String::Format("Injection failed, return value: {0}", gcnew String(retVal.ToString())));
	}
}


