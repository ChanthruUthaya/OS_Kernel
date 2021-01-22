#include "pchild.h"
#include "libc.h"


void main_pchild(){
    bool waiting = true;
    bool eating  = false;
    int spoons = 1;
    while(1){
        while(waiting){
            int x = readpipe();
            if(x == 1){
                spoons++;
                waiting = false;
                eating = true;
            }
        }
       
        if(eating){
            write(STDOUT_FILENO, "EAT", 3);
                 eating = false;  
                
            }
        while(spoons == 2){
            int x = readpipe();
            if(x == -1){
                spoons--;
                waiting = true;
            }
        }
        
        
    }
    exit(EXIT_SUCCESS);
}