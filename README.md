# Globber - Scripted file assembler

Globber was developed out of a need to easily, repeatably create and modify data files. It uses a simple
scripting language to build binary, raw data and text files to order.

# What does it do?

Globber reads a script, where each command successively builds or modifies a blob in memory that is then written
on completion. The blob starts as empty, and commands can append external files or streams of data. Commands can
also overwrite sections, or insert new data.

Globber can therefore concatenate files, edit them, extract sections, fill or pad them in a repeatable,
easily understood manner.

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

# Script Reference

A line starts with a command followed by one or more arguments
```
append # - Append data to the output
insert # - Insert data at a location in the output, shifting existing data after
write  # - Write data at a location in the output, replacing any existing data
offset # - Set an address offset, useful when building structures
```

Data can be a file, one or more numeric values, strings or hex blocks
```
append file afile.bin                           # - Appends afile.bin to the output
append data 12,34, "This is a string\n"         # - Appends bytes to the output
append hex  00AF73D9B0CAFE                      # - Appends hex data to the output
```

Numeric values are decimal by default. An underscore can be used to improve readability.
Use 0x to prefix hex values, or 0b to prefix binary values.
Adding 'k' or 'M' multiplies the value by 1024 or 1,048,576 respectively.

```
123          -> 123
0xA          -> 10
0b_0100_1010 -> 74
12k          -> 12288
```

Data is interpreted as a byte sequence by default, accepting values in the range -128 to 255.
Word and Long values can be read by inserting `word` or `long` in the data stream. All 
subsequent values will be interpreted as word or long data, generating two or four byte 
sequences. Use `byte` to return to interpreting values as single bytes.

By default, output is little endian, this can be controlled with `be` or `le` 

```
data 12, -1, word 12k, -386, long be 0xDEADBEEF
```

Produces the byte sequence: `0cff 0030 7efe dead beef`

## Selecting parts of data

Parts of the input data can be selected with the `from`, `to` and `bytes` parameters.
If `from` is not specified, the start of the input is assumed.
Only one of `to` or `bytes` can be specified. If neither is used, the end of the input is assumed.

```
append file in.bin from 278 bytes 1k  # - Appends 1024 bytes from in.bin, starting at offset 278
append file in.bin from 12 to 200     # - Appends 188 bytes from in.bin, starting at offset 12
```

## Padding data

Input data can be padded to a given length with a sequence of bytes.
Specify the desired length with `exactly <value>`.
Provide a sequence of one or more bytes to pad with, using `pad <value>[,<value>...]`
The sequence is repeated until the desired length is reached. 
Use `pad once <values>[,<value>..]` to stop the full sequence being repeated - the last byte is used
to fill to the desired length.

```
append exactly 1k pad "Hello, "            # - Appends 1024 bytes, filled with 'Hello, Hello, Hello, '
append exactly 1k pad once "Hello."        # - Appends 1024 bytes, filled with 'Hello...............'

append exactly 1k file in.bin pad 0x00     # - Appends the file in.bin, padded to 1024 bytes with 0's
```

## Inserting and overwriting

When using `insert` or `write` commands, the `at <value>` argument specifies the location to insert or
overwrite data.

```
insert file in.bin at 100                  # - Inserts the file at offset 100
write data 0x01 at 200                     # - Write byte 0x01 at offset 200
```

## Interleaving data

Two data sets can be interleaved by specifying the length of chunks from the first set and second set. The first
data set can be any of `data`, `hex` or `file`, and the second can also be `pad` - in which case the pad value is
expanded to fit the specified chunk size.

```
append data "abcdef" interleave 2 4 pad "-"    # - Produces the output 'ab----cd----ef----'
```

# Example Scripts

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