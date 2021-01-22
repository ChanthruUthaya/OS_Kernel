#include "libc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern void main_pchild();




void main_philosopher(){
    int pfd[16];
    bool write = true;
    bool writef = true;
    for(int x = 0; x < 1; x++){
        int pid = fork();
        pfd[x]= pid;
        if(pid == 0){
            pipc(pid, 0);
            exec(&main_pchild);
        }
        else if(pid > 0){
            pipc(pfd[x], 1);
            }
    }
    while(1){
        if(write){
        writepipe(pfd[0], 1);
        write = false;
        }
        int x = pipeFull(pfd[0]);
        if(!x && writef){
            writepipe(pfd[0], -1);
            writef = false;
        }
    }


    exit(EXIT_SUCCESS);
    
}
   

