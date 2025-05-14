#include "lib.h"
int main(int argc, char** argv) {
    testmalloc(argc > 1 ? argv[1] : NULL);
    return 0;
}