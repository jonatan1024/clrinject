
using System;
using System.IO;
using System.Linq;
using System.Reflection;

namespace InjectorApp
{
    class Program
    {
        static void Main(string[] args)
        {
            try
            {
                Injector injector = new Injector();

                //Test enumeration
                var result = injector.EnumerateAppDomains("victim.exe");

                if (result.Any())
                {
                    result.ForEach(r =>
                    {
                        var runtimeInfo = r.IsRuntimeStarted ? "started" : "stopped";
                        Console.WriteLine($"Runtime Version : {r.runtimeVersion} is {runtimeInfo}, and have following app domains:");
                        int index = 0;
                        r.AppDomains.ForEach(appd => Console.WriteLine($"{++index}. {appd}"));
                    });
                }
                else
                {
                    Console.WriteLine("Enumeration yielded no result");
                }

                Console.WriteLine();

                //Test injection in all app domains
                //injector.InjectIntoProcess("victim.exe", PathOfProcessToInject);

                //Test injection in the specified app domain
                injector.InjectIntoProcess("victim.exe", PathOfProcessToInject, 3);

            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.InnerException);
            }

            Console.ReadLine();
        }

        public static string PathOfProcessToInject
        {
            get
            {
                var assembly = Assembly.GetExecutingAssembly();
                string codeBase = assembly.CodeBase;
                UriBuilder uri = new UriBuilder(codeBase);
                var projectRootDirectory = Directory.GetParent(Path.GetDirectoryName(Uri.UnescapeDataString(uri.Path))).Parent.Parent.Parent;
                string buildMode = codeBase.Contains("Debug") ? "Debug" : "Release";
                var assemblyName = AssemblyName.GetAssemblyName(assembly.FullName.Split(',')[0] + ".exe");
                var assemblyArchitecture = assemblyName.ProcessorArchitecture == ProcessorArchitecture.Amd64 ? "x64" : "x86";
                return $"{projectRootDirectory.FullName}\\invader\\bin\\{assemblyArchitecture}\\{buildMode}\\invader.exe";
            }
        }
    }
}
