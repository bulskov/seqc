#include "seqc/string_io.h"

size_t string_fwrite(string_t s, FILE *f)
{
    if (!f || !s.ptr || s.len == 0)
        return 0;
    return fwrite(s.ptr, 1, s.len, f);
}

size_t string_print(string_t s)
{
    return string_fwrite(s, stdout);
}

size_t string_println(string_t s)
{
    size_t n = string_fwrite(s, stdout);
    if (putchar('\n') != EOF)
        n++;
    return n;
}
