/* Test vector for the flowgraph rendering
*/
#include <stdlib.h>

int global;
void *foo(int x) {
void **block, *last = NULL;
    for(int i=0; i<5; i++) {
        block = malloc( sizeof(void*)+x*global );
        *block = last;
        last = block;
        block = malloc( sizeof(void*)+x );
        *block = last;
        last = block;
    }
    return block;
}

int main(int argc, char **argv) {
    global = 3;
    return foo(argc)==NULL ? 1 : 0;
}
