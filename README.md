# Safe Endian Union
## What is it?
`SafeEndianUnion`is a class written in C++20, that using in the underlying implementation a C `union` with endianness safety, that doesn't matter what you do, it will work on Little Endian and Big Endian machines.
Many times people avoid using `union` since their endianness problem, and using bitwise operations to not rely on the endianness of the targeted machine, but these bitwise operations are very ugly most of the case, and can make developers hard to understand it, `union` solves the bitwise ugliness, and now it solves also the endianness issue.

## Usage Example:

### Converting an RGBA value to hex and hex to RGBA:
```cpp
struct RGBA { 
    unsigned char r, g, b, a;
};

int main()
{
    // First parameter: specify the order of the struct.
    // Second parameter: expanding the union to:
    // union { unsigned int; RGBA; };
    evi::SafeEndianUnion<evi::ByteOrder::Big, evi::Union<unsigned int /*hex*/, RGBA>> uni;
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
Test it yourself on [godbolt!](https://godbolt.org/z/zW5nnc)

### Status Flags
```cpp
struct StatusFlags
{
    unsigned int N : 1; // Negative
    unsigned int V : 1; // Overflow
    unsigned int U : 1; // Unused
    unsigned int B : 1; // Break
    unsigned int D : 1; // Decimal
    unsigned int I : 1; // Interrupt
    unsigned int Z : 1; // Zero
    unsigned int C : 1; // Carry
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

Try it yourself on [godbolt](https://godbolt.org/z/Gsd5sT)!

## Rules
Since C++ does not support reflection by default yet, i couldn't check the `struct` fields, so please avoid the following when making your `struct`:
* Do not use pointers or references in your `struct`s, this could break the union.
* Do not have different types in your struct, stick to only one type, this may change the size of the `struct` due to
[alignment and padding.](http://www.catb.org/esr/structure-packing/)
* The `struct` must be a [POD.](https://en.wikipedia.org/wiki/Passive_data_structure)
* `evi::Union<...>` accepts only arithmetic types and a stack allocated arrays ( either `std::array<T, N>` or `array[N]` ).
* `evi::Union<...>` will fail your compilation if you pass to it `volatile` or `const` types, it will also fail for references and pointers.

## Benchmark
### Info:
* CPU: Ryzen 7 3700x.
* Compiler: GNU G++-10.
* Compiler Flags: O3.
* Little Endian was tested on my machine.
* Big Endian was tested with qemu and mips compiler under WSL2.
### Results:
* RGBA to HEX and HEX to RGBA:
* * Little Endian: NOT YET.
* * Big Endian: NOT YET.

## TODO
* Make it faster by using intrinsic functions.
