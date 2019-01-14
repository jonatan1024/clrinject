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
private:
	System::Collections::Generic::List<EnumerationResult>^ ExtractEnumerationInfo(InjectionResult &result, System::Collections::Generic::List<EnumerationResult> ^output);
	System::Collections::Generic::List<EnumerationResult> ^ EnumerateAppDomains(InjectionOptions &options);
	void ResultGuard(int retVal, InjectionResult &result);
};


