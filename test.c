#include <stdio.h>
#include <stdlib.h>

struct fam {
    int length;
    char test[];
}; 


int main(int argc, const char* argv[]) {
    struct fam foo;
    printf("%lu \n", sizeof(int));
    printf("%lu \n", sizeof(char*));
    printf("%lu \n", sizeof(foo));
    return 0;
}
