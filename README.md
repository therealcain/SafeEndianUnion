# Safe Endian Union
## What is it?
`SafeEndianUnion` is a class written in C++20, that using in the underlying implementation a class similar to C `union` with endianness safety, that doesn't matter what you do, it will work on Little Endian and Big Endian machines.
Many times people are avoiding to use `union` since their endianness problem, and using bitwise operations to not rely on the endianness of the targeted machine, but these bitwise operations are very ugly most of the cases, and can make developers hard to understand it, `union` solves the bitwise ugliness, and make the code more elegant, and now it solves also the endianness issue.

## Usage Examples:

### Converting an RGBA value to hex and hex to RGBA:
```cpp
struct RGBA { 
    uint8_t r, g, b, a;
};

int main()
{
    // First parameter: specify the order of the struct.
    // Second parameter: expanding the union to:
    // union { unsigned int; RGBA; };
    evi::SafeEndianUnion<evi::ByteOrder::Big, evi::Union<uint32_t, RGBA>> uni;
    uni = 0xAABBCCFF;
    
    auto as_struct = uni.get<RGBA>();
    std::cout << "R = " << (int)as_struct.r << "\nG = " << (int)as_struct.g << "\nB = " << (int)as_struct.b << "\nA = " << (int)as_struct.a << "\n";

    auto as_hex = uni.get<0>();
    std::cout << "HEX = " << std::hex << +as_hex << "\n";
}
```

And the output on both endianness is: 
```
R = 170
G = 187
B = 204
A = 255
HEX = aabbccff
```
Test it yourself on [godbolt](https://godbolt.org/z/qheEde)!

### Status Flags
```cpp
// Bit fields are not allowed by the reflection system.
#undef EVI_ENABLE_REFLECTION_SYSTEM

struct StatusFlags
{
    bool N : 1; // Negative
    bool V : 1; // Overflow
    bool U : 1; // Unused
    bool B : 1; // Break
    bool D : 1; // Decimal
    bool I : 1; // Interrupt
    bool Z : 1; // Zero
    bool C : 1; // Carry
};

int main()
{
    evi::SafeEndianUnion<evi::ByteOrder::Big, evi::Union<uint8_t, StatusFlags>> uni;
    
    uni = StatusFlags{ 0, 0, 0, 1, 1, 1, 0, 1 };
    std::cout << std::bitset<8>(uni.get<uint8_t>()) << "\n";
    
    auto as_struct = uni.get<StatusFlags>();
    std::cout << "N = " << as_struct.N << "\n";
    std::cout << "V = " << as_struct.V << "\n";
    std::cout << "U = " << as_struct.U << "\n";
    std::cout << "B = " << as_struct.B << "\n";
    std::cout << "D = " << as_struct.D << "\n";
    std::cout << "I = " << as_struct.I << "\n";
    std::cout << "Z = " << as_struct.Z << "\n";
    std::cout << "C = " << as_struct.C << "\n";
}
```
And the output on both endianness is: 
```
00011101
N = 0
V = 0
U = 0
B = 1
D = 1
I = 1
Z = 0
C = 1
```

Try it yourself on [godbolt](https://godbolt.org/z/3sq3xT)!

### String Manipulation
```cpp
struct data
{
    char a[1];
    char b[2];
    char c[3];
    char d[4];
    char e[5];
};

int main()
{
    using array = std::array<char, sizeof(data)>;
    evi::SafeEndianUnion<evi::ByteOrder::Little, evi::Union<array, data>> uni;
    uni.set(array{"abcdefghi"});

    auto as_struct = uni.get<data>();
    std::cout << std::string(as_struct.a, sizeof(as_struct.a)) << "\n";
    std::cout << std::string(as_struct.b, sizeof(as_struct.b)) << "\n";
    std::cout << std::string(as_struct.c, sizeof(as_struct.c)) << "\n";
    std::cout << std::string(as_struct.d, sizeof(as_struct.d)) << "\n";
    std::cout << std::string(as_struct.e, sizeof(as_struct.e)) << "\n";
}
```

And the output on both endianness is: 

```
a
bc
def
ghi
```

Try it yourself on [godbolt](https://godbolt.org/z/M7GKsr)!

## Rules
* Do not use pointers or references in your `struct`s, this could break the union.
* Do not have different types in your struct, stick to only one type, this may change the size of the `struct` due to
[alignment and padding.](http://www.catb.org/esr/structure-packing/)
* The `struct` must be a [POD.](https://en.wikipedia.org/wiki/Passive_data_structure)
* `evi::Union<...>` accepts only arithmetic types and a stack allocated arrays ( either `std::array<T, N>` or `array[N]` ).
* `evi::Union<...>` will fail your compilation if you pass to it `volatile` or `const` types, it will also fail for references and pointers.
* If you having hard time debugging your program, and you believe it may be the `SafeEndianUnion` try to enable the the reflection system, it will check if your types in the `struct` are valid.
* You can use bit fields only if the reflection system is NOT enabled, because [bit fields are not recommended](https://stackoverflow.com/a/23458891/8298564).
* `std::bit_cast` might fail if you use bit fields.
* If the reflection system is ON then your `struct` is limited only up to 32 fields.

## Benchmark
### Info:
* CPU: Ryzen 7 3700x.
* Compiler: GNU g++-10.
* Compiler Flags: O3.
* Little Endian was tested on my machine ( Windows 10 ).
* Big Endian was tested with qemu and mips g++-10 compiler under WSL2.
### Results:
* RGBA to HEX:
* - Bitwise Operations: NOT YET.
* - C union, [`ntohl`](https://linux.die.net/man/3/ntohl) and `inline` endianness check: NOT YET.
* - `SafeEndianUnion`: NOT YET.

## How to use?
It's just a [simple header](https://github.com/therealcain/SafeEndianUnion/blob/main/SafeEndianUnion.hpp) to drop into your project, and just run.

Here are a few fair points:
* It does not use any external libraries ( like Boost ), so you don't have to link anything.
* Make sure you enable concepts and constraints in your compilers. ( in GCC it's `-fconcepts` ).
* Make sure the compiler is using C++20. ( in GCC and Clang it's `-std=++2a` or `-std=++20` ).

### Enable/Disable The Reflection System
The reflection system is disabled by default.
To enable it you can simply define it before you include the SafeEndianUnion header:
```cpp
#define EVI_ENABLE_REFLECTION_SYSTEM
#include "SafeEndianUnion.hpp"
```
