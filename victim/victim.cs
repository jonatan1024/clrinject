using System;
using System.Collections.Generic;
using System.Configuration;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Victim
{
    class Victim
    {
        static void Main(string[] args)
        {
            AppDomain.CurrentDomain.SetData("dummy", "Original content");
            while (true)
            {
                Console.WriteLine(AppDomain.CurrentDomain.GetData("dummy"));
                Thread.Sleep(TimeSpan.FromSeconds(1));
            }
        }
    }
}
