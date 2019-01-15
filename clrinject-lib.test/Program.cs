
using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;

namespace InjectorApp
{
    class Program
    {
        static void Main(string[] args)
        {
            Process victimProcess = null;

            try
            {
                victimProcess = Process.Start(PathOfVictimApp);
                if (!victimProcess.IsRunning())
                    return;

                System.Threading.Thread.Sleep(TimeSpan.FromSeconds(6));

                Injector injector = new Injector();

                //Test enumeration
                var result = injector.EnumerateAppDomains("victim.exe");

                if (result.Any())
                {
                    result.ForEach(runtime =>
                    {
                        var runtimeStatus = runtime.IsRuntimeStarted ? "started" : "stopped";
                        Console.WriteLine($"Runtime Version : {runtime.runtimeVersion} is {runtimeStatus}, and have following app domains:");
                        int index = 0;
                        runtime.AppDomains.ForEach(appd => Console.WriteLine($"{++index}. {appd}"));
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
           
            Console.WriteLine("Press any key to exit.");
            Console.ReadLine();

            victimProcess.Kill();

        }

        public static string PathOfProcessToInject
        {
            get
            {
                return GetPath("invader");
            }
        }

        public static string PathOfVictimApp
        {
            get
            {
                return GetPath("victim");
            }
        }

        private static string GetPath(string appName)
        {
            var assembly = Assembly.GetExecutingAssembly();
            string codeBase = assembly.CodeBase;
            UriBuilder uri = new UriBuilder(codeBase);
            var projectRootDirectory = Directory.GetParent(Path.GetDirectoryName(Uri.UnescapeDataString(uri.Path))).Parent.Parent.Parent;
            string buildMode = codeBase.Contains("Debug") ? "Debug" : "Release";
            var assemblyName = AssemblyName.GetAssemblyName(assembly.FullName.Split(',')[0] + ".exe");
            var assemblyArchitecture = assemblyName.ProcessorArchitecture == ProcessorArchitecture.Amd64 ? "x64" : "x86";
            return $"{projectRootDirectory.FullName}\\{appName}\\bin\\{assemblyArchitecture}\\{buildMode}\\{appName}.exe";
        }
    }

    public static class ProcessExtensions
    {
        public static bool IsRunning(this Process process)
        {
            if (process == null)
                throw new ArgumentNullException("process");
            try
            {
                Process.GetProcessById(process.Id);
            }
            catch (ArgumentException)
            {
                return false;
            }
            return true;
        }
    }
}
