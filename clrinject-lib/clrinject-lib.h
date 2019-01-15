#pragma once
#include "clrinject-lib.h"
#include "../clrinject/clrinject.h"

using namespace System;

typedef public value struct EnumerationResult
{
public:
	String ^runtimeVersion;
	Boolean IsRuntimeStarted;
	System::Collections::Generic::List<String^>^ AppDomains;
};

public ref class Injector
{
public:
	System::Collections::Generic::List<EnumerationResult>^ EnumerateAppDomains(int processId);
	System::Collections::Generic::List<EnumerationResult>^ EnumerateAppDomains(String ^processName);
	void InjectIntoProcess(int processId, String^ pathOfAssemblyToBeInjected);
	void InjectIntoProcess(String ^processName, String^ pathOfAssemblyToBeInjected);
	void InjectIntoProcess(int processId, String^ pathOfAssemblyToBeInjected, int appDomainNo);
	void InjectIntoProcess(String ^processName, String^ pathOfAssemblyToBeInjected, int appDomainNo);
private:
	System::Collections::Generic::List<EnumerationResult> ^ EnumerateAppDomains(InjectionOptions &options);
};

InjectionResult InvokeInjection(InjectionOptions &options);
System::Collections::Generic::List<EnumerationResult>^ ExtractEnumerationInfo(InjectionResult &result);
void ResultGuard(int retVal, InjectionResult &result);
int GetProcessIdFromName(String ^ processName);
InjectionResult InvokeInjection(InjectionOptions &options);

