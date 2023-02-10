# Globber - Scripted file assembler

Globber was developed out of a need to easily, repeatably create and modify binary files. It uses a simple
scripting language to build files to order.

## Specific requirements

* Cross platform command line tool
* Simple language to describe file manipulation
* No dependencies on external libraries or runtime environments
* Easy to build

Globber is therefore written in standard C++17, in a single file, with no third party dependencies.

## Non-goals

* Does not parse or interpret file formats (images etc.)
* Is not beautiful code
* Not Turing complete
* Not designed to handle extremely large files

## Example Scripts

Append two files

```
append file inA.bin
append file inB.bin
```

Create a 12K file filled with 0xFF

```
append exactly 12k pad 0xFF
```

Change specific bytes within a file

```
append file inA.bin
write at 210 data 0x32, 0x64
```