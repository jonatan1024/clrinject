using System;
using System.Collections.Generic;
using System.Configuration;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Victim
{
    class Victim
    {
        static void Main(string[] args)
        {
            AppDomain.CreateDomain("NewApplicationDomain").CreateInstanceFrom(Assembly.GetEntryAssembly().Location, "Victim.Victim");
            AppDomain.CreateDomain("Another AppDomain").CreateInstanceFrom(Assembly.GetEntryAssembly().Location, "Victim.Victim");
            new Victim();
            Thread.Sleep(-1);
        }

        public Victim()
        {
            Task.Run(() =>
            {
                AppDomain.CurrentDomain.SetData("dummy", "Original content");
                while (true)
                {
                    Console.WriteLine("{0}: {1}",AppDomain.CurrentDomain.FriendlyName, AppDomain.CurrentDomain.GetData("dummy"));
                    Thread.Sleep(TimeSpan.FromSeconds(5));
                }
            });
        }
    }
}
