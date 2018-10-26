#include <iostream>
#include "mwcas.h"

int main(void)
{
    uint64_t a = 0;
    uint64_t b = 1;
    MwcasDescriptor desc;
    desc.add_word(&a, 0, 1);
    desc.add_word(&b, 1, 0);
    std::cout << a << ", " << b << std::endl;
    bool succeeded = mwcas(&desc);
    std::cout << succeeded << std::endl;
    std::cout << a << ", " << b << std::endl;
    return 0;
}
