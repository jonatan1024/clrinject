# clrinject
Injects C# EXE or DLL Assembly into every CLR runtime and AppDomain of another process. 

## Usage
`clrinject-cli.exe -p <processId/processName> -a <assemblyFile>`

Opens process with id `<processId>` or name `<processName>`, inject `<assemblyFile>` EXE and execute Main method.

### Additional options
* `-e` Enumerates all loaded CLR Runtimes and created AppDomains.
* `-d <#>` Inject only into `<#>`-th AppDomain. If no number or zero is specified, assembly is injected into every AppDomain.
* `-i <namespace>.<className>`  Create an instance of class `<className>` from namespace `<namespace>`.

## Examples
* `clrinject-cli.exe -p victim.exe -e` (Enumerate Runtimes and AppDomains from victim.exe)
* `clrinject-cli.exe -p 1234 -a "C:\Path\To\invader.exe" -d 2` (Inject invader.exe into second AppDomain from process with id 1234)
* `clrinject-cli.exe -p victim.exe -a "C:\Path\To\invader.dll" -i "Invader.Invader"` (Create instance of Invader inside every AppDomain in victim.exe)
* `clrinject-cli64.exe -p victim64.exe -a "C:\Path\To\invader64.exe"` (Inject x64 assembly into x64 process)