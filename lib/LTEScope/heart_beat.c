#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <stdbool.h>
int main(){
    int count = 0;
    while (true){
        count++;
        if(count == 4){
            system("ping www.google.com -c 2 -w 20");
            count = 0;
        }
        sleep(1);
    }
}
