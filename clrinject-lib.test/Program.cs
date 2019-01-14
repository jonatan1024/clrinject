
using System;
using System.Linq;

namespace InjectorApp
{
    class Program
    {
        static void Main(string[] args)
        {
            try
            {
                Injector injector = new Injector();
                var result = injector.EnumerateAppDomains("victim.exe");

                if (result.Any())
                    Console.WriteLine(result[0].AppDomains[0]);

                Console.ReadLine();

            }
            catch(Exception ex)
            {
                Console.WriteLine(ex.InnerException);
            }
        }

        public void SayHello()
        {
            Console.WriteLine("Hello World");
        }
    }
}
