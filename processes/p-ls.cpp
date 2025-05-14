
extern "C" {

#include "process.h"
#include "lib.h"

void process_main(void);

}


class MyClass {       // The class
  public:             // Access specifier
    int myNum;        // Attribute (int variable)
    int myMethod() {  // Method/function defined inside the class
        return myNum;
    }

};

void process_main() {
    pid_t p = sys_getpid();
    
    app_printf(3, "Hello, from process %d\n", p);

    MyClass myObj;
    myObj.myNum = 15;
    int r = myObj.myMethod();

    app_printf(3, "num : %d \n", r);

    sys_exit(0);
}

