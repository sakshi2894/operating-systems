#include "types.h"
#include "stat.h"
#include "user.h"
#include "pstat.h"

int
main(int argc, char *argv[])
{
    struct pstat st;

    if(argc != 2){
        printf(1, "usage: mytest counter");
        exit();
    }

    int i, x, j;
    //int mypid = getpid();

    for(i = 1; i < atoi(argv[1]); i++){
        x = x + i;
    }

    getprocinfo(&st);
    for (j = 0; j < 64; j++) {
	    printf(1, "printing j: %d\n", j);
        //if (st.inuse[j] && st.pid[j] >= 3 && st.pid[j] == mypid) {
        //    for (l = 3; l >= 0; l--) {
                printf(1, "pid:%d\n", st.pid[j]);
        //    }
        ///}
    }
    
    exit();
    return 0;
 } 
