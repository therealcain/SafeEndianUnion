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
Test it yourself on [godbolt](https://godbolt.org/z/1E713T)!

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

## What you can and can't do
* You cannot use pointers or references in your `struct`s.
* You cannot have different types in your struct, stick to only one type, this may change the size of the `struct` due to
[alignment and padding.](http://www.catb.org/esr/structure-packing/)
* * You cannot use bitfields.
* The `struct` must be a [POD.](https://en.wikipedia.org/wiki/Passive_data_structure)
* `evi::Union<...>` accepts only arithmetic types and a stack allocated arrays ( either `std::array<T, N>` or `array[N]` ).
* Your `struct` is limited only up to 32 fields.

## How to use?
It's just a [simple header](https://github.com/therealcain/SafeEndianUnion/blob/main/SafeEndianUnion.hpp) to drop into your project, and just run.

Here are a few fair points:
* It does not use any external libraries ( like Boost ), so you don't have to link anything.
* Make sure you enable concepts and constraints in your compilers. ( in GCC it's `-fconcepts` ).
* Make sure the compiler is using C++20. ( in GCC and Clang it's `-std=++2a` or `-std=++20` ).

## TODO
* Implement a trivially copyable class named `evi::Bitfield<T, Len...>` to imitate the behivour of bitfields, without getting errors.
* Support `std::tuple` in addition to `struct`.
* Recursive reflection system to check cases like this: 
```cpp
struct S1 {
    struct { 
        uint8_t val; 
    } S2; // check this, currently this makes an error.
    
    uint8_t val;
};
evi::SafeEndianUnion<evi::ByteOrder::Little, evi::Union<uint16_t, S1>> uni;
```
