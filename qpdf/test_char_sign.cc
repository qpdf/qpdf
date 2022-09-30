#include <cstdio>
int
main()
{
    char ch = '\xf7';
    if (ch < 0) {
        printf("char is signed\n");
    } else {
        printf("char is unsigned\n");
    }
    return 0;
}
