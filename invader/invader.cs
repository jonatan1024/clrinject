using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace invader
{
    class invader
    {
        static int Main(string[] args) { 
            /*Console.WriteLine(AppDomain.CurrentDomain.GetData("dummy"));
            string x = Console.ReadLine();
            AppDomain.CurrentDomain.SetData("dummy", x);
            Console.ReadLine();*/
            Console.WriteLine("data = "+ AppDomain.CurrentDomain.GetData("dummy"));
            Console.Write("new data = ");
            AppDomain.CurrentDomain.SetData("dummy", Console.ReadLine());

            return 0x12345678;
        }
    }
}
