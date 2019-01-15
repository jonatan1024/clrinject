using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Invader
{
    public class Invader
    {
        static int Main(string[] args) { 
            Console.WriteLine("Current data is: "+ AppDomain.CurrentDomain.GetData("dummy"));
            AppDomain.CurrentDomain.SetData("dummy", "You have been invaded !!");

            return 0;
        }

        public Invader()
        {
            AppDomain.CurrentDomain.SetData("dummy", "Data set from Invader.Invader.Invader()!");
        }
    }
}
