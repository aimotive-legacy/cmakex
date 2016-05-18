#include <cstdio>

#define STR2(X) #X
#define STR(X) STR2(X)

int main(int argc, char* argv[])
{
    fprintf(stdout, "%s\n", STR(DEFINE_THIS));
    fprintf(stderr, "%s\n", STR(CONFIG));
    return 0;
}
