#include <string>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <vector>
#include <iostream>
#include <algorithm>

/**
 * Copyright 2023 Andrew Toone / Feersum Technology Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and 
 * to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS 
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
long addressOffset = 0;

enum Action {APPEND, INSERT, WRITE, OFFSET};

enum Mode {BYTES, WORDS, LONGS};

const std::string digits = "0123456789ABCDEF";

std::vector<std::string> read_file_as_strings(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return lines;
    }

    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }


    return lines;
}

std::vector<std::string> tokenize_string(const std::string& str) {
    std::vector<std::string> tokens;
    std::string current_token;

    enum class State {
        Normal,
        Quoted,
        Escaped,
        Comment
    } state = State::Normal;

    for (char c : str) {
        switch (state) {
            case State::Normal:
                if (c == '"') {
                    current_token += c;     // Strings are stored with quote marks at start and end..
                    state = State::Quoted;
                } else if (c == ' ' || c == '\t' || c == ',' || c == '#' || c == '\r') {
                    if(current_token.length() > 0) {
                        tokens.push_back(current_token);
                    }
                    current_token.clear();
                    if( c=='#' ) {
                        state = State::Comment;
                    } else if ( c==',' ) {  // Commas are stored as indiviual tokens
                        current_token += c;
                        tokens.push_back(current_token);
                        current_token.clear();
                    }
                } else {
                    current_token += c;
                }
                break;
            case State::Quoted:
                if (c == '\\') {
                    state = State::Escaped;
                } else {
                    current_token += c;
                }

                if (c == '"') {
                    state = State::Normal;
                }
                break;
            case State::Escaped:
                current_token += c;
                state = State::Quoted;
                break;
            case State::Comment:
                break;
        }
    }

    if (!current_token.empty()) {
        tokens.push_back(current_token);
    }

    return tokens;
}

long parse_number(std::string& str) {

    if( str.size() == 0 ) {
        throw std::invalid_argument( "Cannot parse empty string" );
    }

    bool isNegative = false;
    int start = 0;
    if( str[start] == '-' ) {
        isNegative = true;
        start++;
    }

    long multiplier = 1;

    int end = str.length()-1;
    if( str[end] == 'k' ) {
        multiplier = 1024;
        end--;
    } else if( str[end] == 'M' ) {
        multiplier = 1024 * 1024l;
        end--;
    }

    int base = 10;
    if( str.compare(start, 2, "0x") == 0 ) {
        base = 16;
        start+=2;
    } else if (str.compare( start, 2, "0b") == 0) {
        base = 2;
        start+=2;
    }

    long value = 0;
    
    for(int index = start; index<=end; index++) {
        if( str[index] == '_' ) continue;

        value = value * base;
        int num = digits.find(toupper(str[index]));
        if( num < 0 || num >= base ) {
            std::cerr << "Invalid char in '" << str << "' - '" << str[index] <<"' is not a valid digit" << std::endl;

            throw std::invalid_argument( "Invalid digit in number" );
        }
        value += num;
    }
    value = value * multiplier;

    return isNegative ? -value: value;
}

bool equalsIgnoreCase(const std::string& a, const std::string& b) {
    return std::equal(a.begin(), a.end(),
                      b.begin(), b.end(),
                      [](char a, char b) {
                          return tolower(a) == tolower(b);
                      });
}

void check_no_more_tokens(std::vector<std::string> tokens, const unsigned int size) {
    if(tokens.size() > size ) {
        throw std::invalid_argument("Unexpected token at end of line");
    }
}

long parse_number(std::vector<std::string> tokens, const unsigned int index) {
    if( index >= tokens.size() ) {
        throw std::invalid_argument("Missing parameter for "+tokens[index-1]);
    }
    return parse_number(tokens[index]);
}

std::vector<std::byte> read_hex(std::vector<std::string> tokens, unsigned int &index) {
    if( index >= tokens.size() ) {
        throw std::invalid_argument("Missing parameter for "+tokens[index-1]);
    }
    std::vector<std::byte> result;

    std::string str = tokens[index];
    for( unsigned int i=0; i<str.length(); i+=2) {
        if( i+1 == str.length() ) {
            throw std::invalid_argument("Odd number of digits in hex block: "+str);
        }
        int high = digits.find(toupper(str[i]));
        int low = digits.find(toupper(str[i+1]));

        result.push_back((std::byte) (((high << 4) & 0x0F0) | (low & 0x0F)));
    }
    return result;
}

std::vector<std::byte> read_data(std::vector<std::string> tokens, unsigned int &index) {
    if( index >= tokens.size() ) {
        throw std::invalid_argument("Missing parameter for "+tokens[index-1]);
    }

    std::vector<std::byte> result;

    Mode mode = Mode::BYTES;
    bool littleEndian = true;

    do {
        if( tokens[index].length() > 0 && tokens[index][0] == '"' ) {
            std::string txt = tokens[index].substr(1, tokens[index].length()-2 );
            for(char& ch: txt) {
                result.push_back((std::byte)ch);
            }
            index++;
            if( (index >= tokens.size()) || (tokens[index] != ",") ) {
                break;
            }
        } 
        else if( equalsIgnoreCase(tokens[index], "word" )) {
            mode = Mode::WORDS;
        }
        else if( equalsIgnoreCase(tokens[index], "long")) {
            mode = Mode::LONGS;
        }
        else if( equalsIgnoreCase(tokens[index], "byte")) {
            mode = Mode::BYTES;
        } 
        else if( equalsIgnoreCase(tokens[index], "le")) {
            littleEndian = true;
        } 
        else if( equalsIgnoreCase(tokens[index], "be")) {
            littleEndian = false;
        } 
        else {
            long value = parse_number(tokens[index]);
            // TODO: Check if cast to byte looses bits
            switch(mode) {
                case Mode::BYTES:
                    if( value > 0x0FF || value < -0x080 ) {
                        throw std::invalid_argument("Value '"+tokens[index]+"' is outside of byte range");
                    }
                    result.push_back((std::byte)value);
                    break;
                case Mode::WORDS:
                    if( value > 0x0FFFF || value < -0x08000 ) {
                        throw std::invalid_argument("Value '"+tokens[index]+"' is outside of word range");
                    }
                    if( littleEndian ) {
                        result.push_back((std::byte)(value & 0x0FF));
                        result.push_back((std::byte)((value >> 8) & 0x0FF));
                    }
                    else {
                        result.push_back((std::byte)((value >> 8) & 0x0FF));
                        result.push_back((std::byte)(value & 0x0FF));
                    }
                    break;
                case Mode::LONGS:
                    if( littleEndian ) {
                        result.push_back((std::byte)(value & 0x0FF));
                        result.push_back((std::byte)((value >> 8) & 0x0FF));
                        result.push_back((std::byte)((value >> 16) & 0x0FF));
                        result.push_back((std::byte)((value >> 24) & 0x0FF));
                    }
                    else {
                        result.push_back((std::byte)((value >> 24) & 0x0FF));
                        result.push_back((std::byte)((value >> 16) & 0x0FF));
                        result.push_back((std::byte)((value >> 8) & 0x0FF));
                        result.push_back((std::byte)(value & 0x0FF));
                    }
                    break;
            }
            index++;
            if( (index >= tokens.size()) || (tokens[index] != ",") ) {
                break;
            }            
        }

        index++;
    }
    while( index < tokens.size() );

    index--;

    return result;
}

void check_offsets(long &fromByte, long toByte, long &countBytes, long dataSize, long maxBytes, long exactBytes) {
    if( fromByte > dataSize ) {
        throw std::invalid_argument("Cannot read from byte "+std::to_string(fromByte)+" - input too short ("+std::to_string(dataSize)+")");
    }
    if(toByte > 0) {
        if(toByte <= fromByte) {
            throw std::invalid_argument("Cannot read to byte "+std::to_string(toByte)+" - location must be after start ("+std::to_string(fromByte)+")");
        }
        else if(toByte > dataSize) {
            throw std::invalid_argument("Cannot read to byte "+std::to_string(toByte)+" - input too short ("+std::to_string(dataSize)+")");
        } 
        else if(countBytes > 0) {
            throw std::invalid_argument("Cannot specify both 'to' byte and 'bytes' values");
        }
        countBytes = toByte - fromByte;
    }
    if( countBytes > 0) {
        if( countBytes > (dataSize - fromByte) ) {
            throw std::invalid_argument("Cannot read "+std::to_string(countBytes)+" bytes starting at byte "+std::to_string(fromByte)+" - input too short ("+std::to_string(dataSize)+")");
        }
    } else if( countBytes == 0) {
        throw std::invalid_argument("Invalid length specified for input");
    } else {
        countBytes = dataSize - fromByte;
    }

    if( maxBytes > 0 ) {
        if( maxBytes < countBytes ) {
            throw std::invalid_argument("Data exceeds specified max length - use 'bytes <length>' or 'to <offset>' to truncate input - "+std::to_string(countBytes)+" bytes available");
        }       
    }
    if( exactBytes > 0 ) {
        if( exactBytes < countBytes ) {
            throw std::invalid_argument("Data exceeds specified exact length - use 'bytes <length>' or 'to <offset>' to truncate input - "+std::to_string(countBytes)+" bytes available");
        }       
    }
}

std::vector<std::byte> read_file(std::string filename, long &fromByte, long toByte, long &countBytes, long maxBytes, long exactBytes) {
    std::streampos fileSize;
    std::ifstream file(filename, std::ios::binary);

    // get its size:
    file.seekg(0, std::ios::end);
    fileSize = file.tellg();

    check_offsets(fromByte, toByte, countBytes, fileSize, maxBytes, exactBytes);
    file.seekg(fromByte, std::ios::beg);

    // read the data:
    std::vector<std::byte> fileData(countBytes);
    file.read((char*) &fileData[0], countBytes);
    return fileData;
}

void process_tokens(std::vector<std::string> tokens, std::vector<std::byte> &data, long &previousEnd, Action &previousAction) {
    // Guaranteed to have at least one token

    Action action;

    unsigned int index = 0;

    long atAddress  = -1;
    long maxBytes   = -1;
    long exactBytes = -1;
    long fromByte   = 0;
    long toByte     = -1;
    long countBytes = -1;

    long interleaveFirst = -1;
    long interleaveSecond = -1;

    if( equalsIgnoreCase(tokens[index], "append") ) {
        action = Action::APPEND;
        index++;
    }
    else if( equalsIgnoreCase(tokens[index], "insert") ) {
        action = Action::INSERT;
        index++;
    }
    else if( equalsIgnoreCase(tokens[index], "write") ) {
        action = Action::WRITE;
        index++;
    }
    else if( equalsIgnoreCase(tokens[index], "data") ) {
        action = previousAction;
        if( previousAction != Action::APPEND ) {
            atAddress = previousEnd;
        }
    }
    else if( equalsIgnoreCase(tokens[index], "offset") ) {
        index++;
        if(tokens.size() <= index) {
            throw std::invalid_argument("Offset requires a numeric argument");
        }
        addressOffset = parse_number(tokens, index);
        if( addressOffset < 0 ) {
            throw std::invalid_argument("Address offset must be positive or zero");
        }
        check_no_more_tokens(tokens, 2);
        return;
    }
    else {
        std::cout << "Got Unknown action '" << tokens[index] << "'" << std::endl;

        throw std::invalid_argument("Unknown action "+tokens[index]);
    }

    previousAction = action;

    bool padOnce = false;

    std::vector<std::byte> padData;
    std::vector<std::byte> newData;
    std::vector<std::byte> readData;
    std::vector<std::byte> interleaveData;
    std::string filename;


    while( index < tokens.size() ) {
        if( equalsIgnoreCase(tokens[index], "at") ) {
            atAddress = parse_number(tokens, ++index) + addressOffset;
        } else if( equalsIgnoreCase(tokens[index], "max") ) {
            maxBytes = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "exactly") ) {
            exactBytes = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "from") ) {
            fromByte = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "to") ) {
            toByte = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "bytes") ) {
            countBytes = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "pad" ) ) {
            if( ++index < tokens.size() && equalsIgnoreCase(tokens[index], "once") ) {
                padOnce = true;
                index++;
            }
            padData = read_data(tokens, index);
        } else if( equalsIgnoreCase(tokens[index], "data") ) {
            readData = read_data(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "hex") ) {
            readData = read_hex(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "file") ) {
            if( index >= tokens.size()-1 ) {
                throw std::invalid_argument("File requires a filename parameter");
            }
            filename = tokens[++index];  
        } else if( equalsIgnoreCase(tokens[index], "interleave") ) {
            if( index >= tokens.size()-2 ) {
                throw std::invalid_argument("Interleave requires two size parameters");
            }
            interleaveFirst = parse_number(tokens, ++index);
            if( equalsIgnoreCase(tokens[++index], ",") ) {
                index++;
                if( index >= tokens.size()-1 ) {
                    throw std::invalid_argument("Interleave requires two size parameters");
                }
            }
            interleaveSecond = parse_number(tokens, index);
            if( interleaveFirst <= 0 || interleaveSecond <= 0 ) {
                throw std::invalid_argument("Interleave sizes must be at least 1");
            }

            if( readData.size() > 0 ) {
                check_offsets(fromByte, toByte, countBytes, readData.size(), maxBytes, exactBytes);
                interleaveData = std::vector<std::byte>(readData.begin()+fromByte, readData.begin()+fromByte+countBytes);
            } 
            else if( filename.length() > 0 ) {
                interleaveData = read_file(filename, fromByte, toByte, countBytes, maxBytes, exactBytes);
            }
            else {
                throw std::invalid_argument("Interleave requires input data to be provided before command");
            }

            if( interleaveData.size() % interleaveFirst != 0 ) {
                throw std::invalid_argument("First dataset for interleave must be an exact multiple of "+std::to_string(interleaveFirst)+" bytes - but is "+std::to_string(interleaveData.size()));
            }

            readData.clear();
            filename = "";
            fromByte   = 0;
            toByte     = -1;
            countBytes = -1;
        } else {
            throw std::invalid_argument("Unexpected token: "+tokens[index]);
        }


        index++;
    }

    if( readData.size() > 0 ) {
        check_offsets(fromByte, toByte, countBytes, readData.size(), maxBytes, exactBytes);
        newData = std::vector<std::byte>(readData.begin()+fromByte, readData.begin()+fromByte+countBytes);
    } 
    else if( filename.length() > 0 ) {
        newData = read_file(filename, fromByte, toByte, countBytes, maxBytes, exactBytes);
    }

    if( interleaveData.size() > 0) {
        if( newData.size() == 0 && padData.size() == 0 ) {
            throw std::invalid_argument("Second dataset or pad data required for interleave");
        }
        if( newData.size() == 0 && padData.size() == 0 ) {
            throw std::invalid_argument("Interleave is missing second data set, or pad data");
        }
        if( newData.size() % interleaveSecond != 0) {
            throw std::invalid_argument("Second dataset for interleave must be an exact multiple of "+std::to_string(interleaveSecond)+" bytes - but is "+std::to_string(newData.size()));
        }
        if( newData.size() != 0 && ((newData.size() / interleaveSecond) != (interleaveData.size() / interleaveFirst))) {
            throw std::invalid_argument("Interleave requires same number of chunks for each data set. Interleaving "+std::to_string(interleaveFirst)+"/"+std::to_string(interleaveSecond)+
                " gives "+std::to_string(interleaveData.size() / interleaveFirst)+" and "+std::to_string(newData.size() / interleaveSecond)+" chunks respectively");
        }

        if( newData.size() != 0 ) {
            std::vector<std::byte> result;
            
            unsigned int secondIndex = 0;

            for( unsigned int index = 0; index < interleaveData.size(); index+=interleaveFirst ) {
                result.insert(result.end(), interleaveData.begin()+index, interleaveData.begin()+index+interleaveFirst);
                result.insert(result.end(), newData.begin()+secondIndex, newData.begin()+secondIndex+interleaveSecond);
                secondIndex += interleaveSecond;
            }
            newData = result;
        }
        else {
            std::vector<std::byte> result;
            
            unsigned int secondIndex;

            for( unsigned int index = 0; index < interleaveData.size(); index+=interleaveFirst ) {
                result.insert(result.end(), interleaveData.begin()+index, interleaveData.begin()+index+interleaveFirst);
                secondIndex = 0;
                for( int i=0; i<interleaveSecond; i++ ) {
                    result.push_back(padData[secondIndex]);
                    if( secondIndex < padData.size()-1 || !padOnce ) {
                        secondIndex = (secondIndex+1) % padData.size();
                    }
                }
            }
            newData = result;
        }

    }
    if( exactBytes > 0 ) {
        if( newData.size() < (unsigned long)exactBytes ) {
            if( padData.size() == 0 ) {
                throw std::invalid_argument("Pad data must be specified to extend input to exact length");
            }

            unsigned int index = 0;
            while( newData.size() < (unsigned long)exactBytes ) {
                newData.push_back(padData[index]);
                if( index < padData.size()-1 || !padOnce ) {
                    index = (index+1) % padData.size();
                }
            }
        }
    }

    switch(action) {
        case Action::APPEND: 
            if( atAddress >= 0 ) {
                throw std::invalid_argument("Append should not have 'at <address>' parameter");
            }
            data.insert(data.end(), newData.begin(), newData.end());
            previousEnd = data.size();
            std::cout << "Appended " << data.size() << " bytes " << std::endl;
            break;
        case Action::INSERT: 
            if( atAddress < 0 ) {
                throw std::invalid_argument("Insert requires 'at <address>' parameter");
            }
            if( (unsigned long)atAddress >= data.size() ) {
                throw std::invalid_argument("Insert position "+std::to_string(atAddress)+" is beyond end of data ("+std::to_string(data.size())+")");
            }
            data.insert(data.begin()+atAddress, newData.begin(), newData.end());
            previousEnd = atAddress+data.size();
            std::cout << "Inserted " << data.size() << " bytes at " << atAddress << std::endl;
            break;
        case Action::WRITE: 
            if( atAddress < 0 ) {
                throw std::invalid_argument("Write requires 'at <address>' parameter");
            }
            if( (unsigned long)atAddress > data.size() ) {
                throw std::invalid_argument("Write position "+std::to_string(atAddress)+" is beyond end of data ("+std::to_string(data.size())+")");
            }
            if( (unsigned long)atAddress == data.size() ) {
                data.insert(data.end(), newData.begin(), newData.end());
                previousEnd = data.size();
            }
            else if( (unsigned long)atAddress+newData.size() >= data.size() ) {
                data.erase(data.begin()+atAddress, data.end());
                data.insert(data.end(), newData.begin(), newData.end());
                previousEnd = data.size();
            }
            else {
                data.erase(data.begin()+(unsigned long)atAddress, data.begin()+(unsigned long)atAddress+newData.size());
                data.insert(data.begin()+(unsigned long)atAddress, newData.begin(), newData.end());
                previousEnd = atAddress+data.size();
            }
            std::cout << "Wrote " << newData.size() << " bytes at " << atAddress << std::endl;
            break;
        default:
            std::cerr << "Unsupported action " << action << std::endl;
    }
}

void process_script(std::vector<std::string> script, std::string filename) {

    int lineNumber = 1;

    std::vector<std::byte> data;

    long previousEnd = -1;
    Action previousAction = Action::APPEND;

    try {
        for (const std::string& line: script) {
            std::vector<std::string> tokens = tokenize_string(line);

            if( tokens.size() > 0 ) {
                process_tokens(tokens, data, previousEnd, previousAction);
            }
            lineNumber++;
        }
    }
    catch(...) {
        std::cerr << "Error on line " << lineNumber << std::endl;
        throw;
    }

    if( data.size() > 0 ) {
        std::ofstream outfile(filename, std::ios::out | std::ios::binary); 
        outfile.write((const char*)data.data(), data.size());
        std::cout << "Wrote " << data.size() << " bytes to " << filename << std::endl;
    }
    else {
        std::cout << "No data created, file not written" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if( argc != 3 ) {
        std::cout << "Globber v1.0 - build binary data files with scripts" << std::endl;
        std::cout << "  Usage: globber script-file output-file" << std::endl;
        std::cout << "Script reference: " << std::endl;
        std::cout << "  Each line of the script file is of the form  <command> <arguments> # comment"<< std::endl;
        std::cout << "  Commands:" << std::endl;
        std::cout << "     append         - append data" << std::endl;
        std::cout << "     insert         - insert data" << std::endl;
        std::cout << "     write          - overwrite data" << std::endl;
        std::cout << "     offset <value> - set offset for insert/overwrite" << std::endl;
        std::cout << "  Arguments:" << std::endl;
        std::cout << "     file <filename>                   - read data from file" << std::endl;
        std::cout << "     data <value> [,<value>...]        - read data from list of values" << std::endl;
        std::cout << "     hex <hexblock>                    - read data from hex stream" << std::endl;
        std::cout << "     from <offset>                     - read from offset in data" << std::endl;
        std::cout << "     to <offset>                       - read to offset in data" << std::endl;
        std::cout << "     bytes <offset>                    - read exact number of bytes in data" << std::endl;
        std::cout << "     exactly <value>                   - extend data to an exact length" << std::endl;
        std::cout << "     pad [once] <value> [,<value>...]  - pad to required length with sequence of values" << std::endl;
        std::cout << "     at <value>                        - insert or write data at the given offset" << std::endl;
        std::cout << "     interleave <value1> [,] <value2>  - interleave a first data set with a second in chunks of value1 and value2 bytes" << std::endl;
        std::cout << "   Values:" << std::endl;
        std::cout << "     123                -> Decimal number" << std::endl;
        std::cout << "     0x12               -> Hex number" << std::endl;
        std::cout << "     0b0101             -> Binary number" << std::endl;
        std::cout << "       - Use underscores for readability, append 'k' or 'M' for kilobyte or megabyte offsets" << std::endl;
        std::cout << "       - Values in data/pad are interpreted as bytes by default, use 'word', 'long', 'byte' to change" << std::endl;
        std::cout << "       - Word and Long values are written as little endian, use 'le' or 'be' to change" << std::endl;
        std::cout << "       - e.g.   data 0b0101_000, 0x00, word 1234, 4567, be 0xCAFE" << std::endl;
        std::cout << "     \"String\\n\"     -> String (sequence of bytes)" << std::endl;
        return 0;
    }

    std::vector<std::string> script = read_file_as_strings(argv[1]);
    process_script(script, argv[2]);
    return 0;
}