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

    std::transform(str.begin(), str.end(), str.begin(), ::toupper);

    long value = 0;
    const std::string digits = "0123456789ABCDEF";
    for(int index = start; index<=end; index++) {
        if( str[index] == '_' ) continue;

        value = value * base;
        int num = digits.find(str[index]);
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


std::vector<std::byte> read_data(std::vector<std::string> tokens, unsigned int &index) {
    if( index >= tokens.size() ) {
        throw std::invalid_argument("Missing parameter for "+tokens[index-1]);
    }

    std::vector<std::byte> result;

    Mode mode = Mode::BYTES;
    bool littleEndian = true;

    do {
        if( tokens[index].length() > 0 && tokens[index][0] == '"' ) {
            std::string txt = tokens[index].substr(1, tokens[index].length()-1 );
            for(char& ch: txt) {
                result.push_back((std::byte)ch);
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
            throw std::invalid_argument("Cannot specify both to byte and byte count values");
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
            throw std::invalid_argument("Data exceeds specified max length - use 'count <bytes>' or 'to <offset>' to truncate input - "+std::to_string(countBytes)+" bytes available");
        }       
    }
    if( exactBytes > 0 ) {
        if( exactBytes < countBytes ) {
            throw std::invalid_argument("Data exceeds specified exact length - use 'count <bytes>' or 'to <offset>' to truncate input - "+std::to_string(countBytes)+" bytes available");
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

void process_tokens(std::vector<std::string> tokens, std::vector<std::byte> &data) {
    // Guaranteed to have at least one token

    Action action;

    if( equalsIgnoreCase(tokens[0], "append") ) {
        action = Action::APPEND;
    }
    else if( equalsIgnoreCase(tokens[0], "insert") ) {
        action = Action::INSERT;
    }
    else if( equalsIgnoreCase(tokens[0], "write") ) {
        action = Action::WRITE;
    }
    else if( equalsIgnoreCase(tokens[0], "offset") ) {
        if(tokens.size() < 2) {
            throw std::invalid_argument("Offset requires a numeric argument");
        }
        addressOffset = parse_number(tokens, 1);
        if( addressOffset < 0 ) {
            throw std::invalid_argument("Address offset must be positive or zero");
        }
        check_no_more_tokens(tokens, 2);
        return;
    }
    else {
        std::cout << "Got Unknown action '" << tokens[0] << "'" << std::endl;

        throw std::invalid_argument("Unknown action "+tokens[0]);
    }

    long atAddress  = -1;
    long maxBytes   = -1;
    long exactBytes = -1;
    long fromByte   = 0;
    long toByte     = -1;
    long countBytes = -1;

    std::vector<std::byte> padData;
    std::vector<std::byte> newData;

    unsigned int index = 1;
    while( index < tokens.size() ) {
        if( equalsIgnoreCase(tokens[index], "at") ) {
            atAddress = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "max") ) {
            maxBytes = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "exactly") ) {
            exactBytes = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "from") ) {
            fromByte = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "to") ) {
            toByte = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "count") ) {
            countBytes = parse_number(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "pad" ) ) {
            padData = read_data(tokens, ++index);
        } else if( equalsIgnoreCase(tokens[index], "data") ) {
            std::vector<std::byte> readData = read_data(tokens, ++index);
            check_offsets(fromByte, toByte, countBytes, readData.size(), maxBytes, exactBytes);
            newData = std::vector<std::byte>(readData.begin()+fromByte, readData.begin()+fromByte+countBytes);
        } else if( equalsIgnoreCase(tokens[index], "file") ) {
            newData = read_file(tokens[++index], fromByte, toByte, countBytes, maxBytes, exactBytes);
        }

        index++;
    }
    switch(action) {
        case Action::APPEND: 
            if( atAddress >= 0 ) {
                throw std::invalid_argument("Append should not have 'at <address>' parameter");
            }
            data.insert(data.end(), newData.begin(), newData.end());
            std::cout << "Append " << data.size() << std::endl;
            break;
        case Action::INSERT: 
            if( atAddress < 0 ) {
                throw std::invalid_argument("Insert requires 'at <address>' parameter");
            }
            std::cout << "Insert" << std::endl;
            break;
        case Action::WRITE: 
            if( atAddress < 0 ) {
                throw std::invalid_argument("Write requires 'at <address>' parameter");
            }
            std::cout << "Write" << std::endl;
            break;
        default:
            std::cerr << "Unsupported action " << action << std::endl;
    }
}

void process_script(std::vector<std::string> script, std::string filename) {

    int lineNumber = 1;

    std::vector<std::byte> data;

    try {
        for (const std::string& line: script) {
            std::vector<std::string> tokens = tokenize_string(line);

            if( tokens.size() > 0 ) {
                process_tokens(tokens, data);
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
        std::cout << "Globber - build binary data files with scripts" << std::endl;
        std::cout << "  Usage: globber script-file output-file" << std::endl;
        return 0;
    }

    std::vector<std::string> script = read_file_as_strings(argv[1]);
    process_script(script, argv[2]);
    return 0;
}