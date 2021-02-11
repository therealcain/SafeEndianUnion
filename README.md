# Safe Endian Union
## What is it? ( still incomplete )
`SafeEndianUnion`is a class written in C++20, that using in the underlying implementation a C `union` with endianness safety, that doesn't matter what you do, it will work on Little Endian and Big Endian machines.
Many times people avoid using `union` since their endianness problem, and using bitwise operations to not rely on the endianness of the targeted machine, but these bitwise operations are very ugly most of the case, and can make developers hard to understand it, `union` solves the bitwise ugliness, and now it solves also the endianness issue.

## Usage Example:

Converting an RGBA value to hex and backwards:
```cpp
#include "SafeEndianUnion.hpp"

struct RGBA { 
    unsigned char a, r, g, b;
};

int main()
{
    // First parameter: specify the order of the struct.
    // Second parameter: expanding the union to:
    // union { unsigned int; RGBA; };
    evi::SafeEndianUnion<evi::ByteOrder::Little, evi::Union<unsigned int /*hex*/, RGBA>> uni;
    uni = 0xAABBCCFF;
    
    auto as_struct = uni.get<RGBA>();
    std::cout << "R = " << (int)as_struct.r << "\nG = " << (int)as_struct.g << "\nB = " << (int)as_struct.b << "\nA = " << (int)as_struct.a << "\n";
}
```

And the output on both endianness is: 
```
R = 170
G = 187
B = 204
A = 255
```

## TODO 
* Overload for universal references.
* Follow the rule of 5.
* Add operator overload, for `operator=`.
* Make it faster by using intrinsic functions.
