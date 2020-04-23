#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[])
{
    char *nullptr = (char *)0;
    // After calling mprotect in the below line, any write to the address ranging from 0x0000 to 0x1000 will trigger a page fault and terminate the process.
    mprotect(nullptr, 1);

    int rc = fork();
    if (rc == 0)
    {
        printf(1, "Child\n");
        // child process will inherit the protection bit
        // This line will triger the page fault and terminate
        //munprotect(nullptr, 1);
        *nullptr = *nullptr;
        printf(1, "Child New: %p\n", (char)*nullptr);
        exit();
    }
    wait();
    printf(1, "Parent\n");

    // This line will unprotect the address ranging from 0x0000 to 0x1000
    //munprotect(nullptr, 1);

    printf(1, "Accessing\n");
    nullptr[1] = nullptr[1];
    printf(1, "Parent New: %p\n", (char)*nullptr);
    /*char *nullptr = (char *)0;
    printf(1, "Protecting\n");
    mprotect(nullptr, 1);
    printf(1, "Protected\n");
    *nullptr = 5;
    printf(1, "UnProtecting\n");
    munprotect(nullptr, 1);
    printf(1, "Accessing\n");
    *nullptr = 5;
    printf(1, "Accessed\n");
    printf(1, "New: %p\n", (char)*nullptr);*/
    exit();
    //return 0;
}