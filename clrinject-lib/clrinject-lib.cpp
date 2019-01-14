#include <Windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <atlbase.h>
#include "clrinject-lib.h"
#include "../clrinject/clrinject.h"

using namespace System::Runtime::InteropServices;

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
	String ^ processNameWithoutExtension = processName->Substring(0, processName->LastIndexOf('.'));
	array<System::Diagnostics::Process^> ^processes = System::Diagnostics::Process::GetProcessesByName(processNameWithoutExtension);

	if (processes->Length == 0) {
		throw gcnew System::ApplicationException("Failed to get id for process name" + processNameWithoutExtension);;
	}

	int processId = processes[0]->Id;
	options.enumerate = true;
	options.processId = processId;

	return EnumerateAppDomains(options);

}

System::Collections::Generic::List<EnumerationResult> ^ Injector::EnumerateAppDomains(InjectionOptions &options)
{
	InjectionResult result;

	System::Collections::Generic::List<EnumerationResult> ^output = gcnew System::Collections::Generic::List<EnumerationResult>();

	int retVal = Inject(&options, &result);

	ResultGuard(retVal, result);

	return ExtractEnumerationInfo(result, output);
}

System::Collections::Generic::List<EnumerationResult>^ Injector::ExtractEnumerationInfo(InjectionResult &result, System::Collections::Generic::List<EnumerationResult> ^output)
{
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

void Injector::ResultGuard(int retVal, InjectionResult &result)
{
	if (retVal) {
		if (result.status || result.statusMessage[0])
			throw gcnew System::ApplicationException(System::String::Format(L"Injection failed, return value: {0}, code: {1}, reason: {2}", gcnew String(retVal.ToString()), gcnew String(result.status.ToString()), gcnew String(result.statusMessage)));
		else
			throw gcnew System::ApplicationException(System::String::Format("Injection failed, return value: {0}", gcnew String(retVal.ToString())));
	}
}


