#include "pickle.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm> // For std::min (not directly used currently but good include for parsers)
#include <iomanip>   // For std::hex in error messages

PickleParser::PickleParser(const std::string& fname) : sPickleFileName(fname), cPickleVersion(0), size(0) {}

uint32_t PickleParser::read_le32(const char* data) const {
    const unsigned char* udata = reinterpret_cast<const unsigned char*>(data);
    return static_cast<uint32_t>(udata[0]) |
           (static_cast<uint32_t>(udata[1]) << 8) |
           (static_cast<uint32_t>(udata[2]) << 16) |
           (static_cast<uint32_t>(udata[3]) << 24);
}

uint16_t PickleParser::read_le16(const char* data) const {
    const unsigned char* udata = reinterpret_cast<const unsigned char*>(data);
    return static_cast<uint16_t>(udata[0]) |
           (static_cast<uint16_t>(udata[1]) << 8);
}

int64_t PickleParser::decode_signed_long(const unsigned char* bytes, size_t len) const {
    if (len == 0) {
        return 0LL;
    }
    
    size_t readable_len = len;
    if (len > sizeof(int64_t)) {
         std::cerr << "Warning: LONG integer with original length " << len 
                   << " exceeds int64_t capacity. Processing max " << sizeof(int64_t) 
                   << " bytes. Potential truncation/misinterpretation." << std::endl;
         readable_len = sizeof(int64_t); 
    }

    uint64_t u_val = 0;
    for (size_t i = 0; i < readable_len; ++i) {
        u_val |= static_cast<uint64_t>(bytes[i]) << (i * 8);
    }

    // Sign extension if the most significant bit of the actual 'readable_len'-byte number is set
    if (readable_len > 0 && (bytes[readable_len - 1] & 0x80)) {
        if (readable_len < sizeof(int64_t)) {
            // Create a mask to set all bits from (readable_len * 8) upwards to 1
            uint64_t sign_extension_mask = ~((1ULL << (readable_len * 8)) - 1);
            u_val |= sign_extension_mask;
        }
        // If readable_len == sizeof(int64_t) and MSB is set, u_val (when cast to int64_t) will be negative.
    }
    return static_cast<int64_t>(u_val);
}


void PickleParser::pickleOpener() {
    std::ifstream file(sPickleFileName, std::ios_base::binary | std::ios_base::ate);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + sPickleFileName);
    }
    size = file.tellg();
    if (size < 0) { 
        file.close();
        throw std::runtime_error("Failed to get file size or empty file: " + sPickleFileName);
    }
    file.seekg(0, std::ios_base::beg);
    buff.resize(static_cast<size_t>(size)); 
    file.read(buff.data(), size);
    if (file.gcount() != size) {
        file.close();
        throw std::runtime_error("Failed to read the full file content: " + sPickleFileName);
    }
    file.close();

    if (static_cast<size_t>(size) < 2 || static_cast<unsigned char>(buff[0]) != 0x80) { // PROTO opcode
        throw std::runtime_error("Invalid pickle format: Missing PROTO or incorrect start byte.");
    }
    cPickleVersion = static_cast<unsigned char>(buff[1]);
    std::cout << "Pickle version: " << static_cast<int>(cPickleVersion) << std::endl;
}

void PickleParser::setParsers() {
    // STOP (0x2e '.')
    parserMap[0x2e] = [](const char* ) -> size_t { // data not used for 0-arg
        std::cout << "STOP: parsing complete" << std::endl;
        return 0;
    };

    // MARK (0x28 '(')
    parserMap[0x28] = [](const char* ) -> size_t {
        std::cout << "MARK: pushing mark" << std::endl;
        return 0;
    };

    // EMPTY_DICT (0x7d '}')
    parserMap[0x7d] = [](const char* ) -> size_t {
        std::cout << "EMPTY_DICT: creating empty dictionary" << std::endl;
        return 0;
    };

    // EMPTY_LIST (0x5d ']')
    parserMap[0x5d] = [](const char* ) -> size_t {
        std::cout << "EMPTY_LIST: creating empty list" << std::endl;
        return 0;
    };

    // BININT1 (0x4b 'K')
    parserMap[0x4b] = [this](const char* data) -> size_t {
        if ((data - buff.data()) + 1 > buff.size()) {
            throw std::runtime_error("BININT1: Not enough bytes for value");
        }
        uint8_t val = static_cast<uint8_t>(data[0]);
        std::cout << "BININT1: " << static_cast<int>(val) << std::endl;
        return 1;
    };

    // BININT2 (0x4d 'M')
    parserMap[0x4d] = [this](const char* data) -> size_t {
        if ((data - buff.data()) + 2 > buff.size()) { 
            throw std::runtime_error("BININT2: Not enough bytes for value");
        }
        uint16_t val = read_le16(data);
        std::cout << "BININT2: " << val << std::endl;
        return 2;
    };

    // BININT (0x4a 'J')
    parserMap[0x4a] = [this](const char* data) -> size_t {
        if ((data - buff.data()) + 4 > buff.size()) { 
            throw std::runtime_error("BININT: Not enough bytes for value");
        }
        uint32_t val = read_le32(data);
        std::cout << "BININT: " << val << std::endl;
        return 4;
    };

    // LONG (0x4c 'L')
    parserMap[0x4c] = [this](const char* data) -> size_t {
        size_t pos = 0;
        std::string num_str;
        size_t initial_arg_offset = data - buff.data();
        while (initial_arg_offset + pos < buff.size() && buff[initial_arg_offset + pos] != '\n') {
            num_str += buff[initial_arg_offset + pos];
            pos++;
        }
        if (initial_arg_offset + pos >= buff.size() || buff[initial_arg_offset + pos] != '\n') {
            throw std::runtime_error("LONG: No newline or buffer exhausted");
        }
        pos++; // Consume newline
        try {
            long long val = std::stoll(num_str);
            std::cout << "LONG: " << val << " (parsed as '" << num_str << "')" << std::endl;
        } catch (const std::exception& e) {
            throw std::runtime_error("LONG: Invalid number '" + num_str + "': " + e.what());
        }
        return pos;
    };

    // LONG1 (0x8a)
    parserMap[0x8a] = [this](const char* d) -> size_t {
        size_t arg_offset = d - buff.data();
        if (arg_offset + 1 > buff.size()) throw std::runtime_error("LONG1: Not enough bytes for length");
        uint8_t len = static_cast<uint8_t>(d[0]);
        if (arg_offset + 1 + len > buff.size()) {
             throw std::runtime_error("LONG1: Insufficient data for value. Need " + std::to_string(len) + " value bytes.");
        }
        int64_t val = decode_signed_long(reinterpret_cast<const unsigned char*>(d + 1), len);
        std::cout << "LONG1: " << val << std::endl;
        return 1 + len; 
    };

    // LONG4 (0x8b)
    parserMap[0x8b] = [this](const char* d) -> size_t {
        size_t arg_offset = d - buff.data();
        if (arg_offset + 4 > buff.size()) throw std::runtime_error("LONG4: Not enough bytes for length");
        uint32_t len_val = read_le32(d);
        size_t len = static_cast<size_t>(len_val);
        if (arg_offset + 4 + len > buff.size()) {
            throw std::runtime_error("LONG4: Insufficient data for value. Need " + std::to_string(len) + " value bytes.");
        }
        if (len_val > 0xFFFFFFF0 && len_val != 0) { // Basic sanity check for very large lengths
             std::cerr << "Warning: LONG4 potentially very large length " << len_val << std::endl;
        }
        int64_t val = decode_signed_long(reinterpret_cast<const unsigned char*>(d + 4), len);
        std::cout << "LONG4: " << val << std::endl;
        return 4 + len; 
    };

    // BINUNICODE (0x58 'X')
    parserMap[0x58] = [this](const char* data) -> size_t {
        size_t arg_offset = data - buff.data();
        if (arg_offset + 4 > buff.size()) throw std::runtime_error("BINUNICODE: Not enough bytes for length");
        uint32_t len_val = read_le32(data);
        size_t len = static_cast<size_t>(len_val);
        if (arg_offset + 4 + len > buff.size()) throw std::runtime_error("BINUNICODE: Not enough bytes for string (len: " + std::to_string(len) + ")");
        std::string str(data + 4, len);
        std::cout << "BINUNICODE: '" << str << "'" << std::endl;
        return 4 + len;
    };

    // BINSTRING (0x54 'T')
    parserMap[0x54] = [this](const char* data) -> size_t {
        size_t arg_offset = data - buff.data();
        if (arg_offset + 4 > buff.size()) throw std::runtime_error("BINSTRING: Not enough bytes for length");
        uint32_t len_val = read_le32(data);
        size_t len = static_cast<size_t>(len_val);
        if (arg_offset + 4 + len > buff.size()) throw std::runtime_error("BINSTRING: Not enough bytes for string (len: " + std::to_string(len) + ")");
        std::string str(data + 4, len);
        std::cout << "BINSTRING: '" << str << "' (" << len << " bytes)" << std::endl;
        return 4 + len;
    };

    // SHORTBINUNICODE (0x8c)
    parserMap[0x8c] = [this](const char* data) -> size_t {
        size_t arg_offset = data - buff.data();
        if (arg_offset + 1 > buff.size()) throw std::runtime_error("SHORTBINUNICODE: Not enough bytes for length");
        uint8_t len = static_cast<uint8_t>(data[0]);
        if (arg_offset + 1 + len > buff.size()) throw std::runtime_error("SHORTBINUNICODE: Not enough bytes for string (len: " + std::to_string(len) + ")");
        std::string str(data + 1, len);
        std::cout << "SHORTBINUNICODE: '" << str << "'" << std::endl;
        return 1 + len;
    };
    
    // BINUNICODE8 (user's 0x8d)
    parserMap[0x8d] = [this](const char* data) -> size_t {
        size_t arg_offset = data - buff.data();
        if (arg_offset + 8 > buff.size()) {
            throw std::runtime_error("BINUNICODE8: Not enough bytes for length");
        }
        uint64_t len_val = 0; 
        for (size_t i = 0; i < 8; ++i) {
            len_val |= static_cast<uint64_t>(static_cast<unsigned char>(data[i])) << (i * 8);
        }
        size_t len = static_cast<size_t>(len_val); 
        if (len_val > (buff.size() - (arg_offset + 8)) ) { // Check if declared length fits remaining buffer
             throw std::runtime_error("BINUNICODE8: Declared length " + std::to_string(len_val) + " exceeds remaining buffer space.");
        }
        std::string str(data + 8, len);
        std::cout << "BINUNICODE8: '" << str << "'" << std::endl;
        return 8 + len;
    };

    // BINBYTES (0x42 'B')
    parserMap[0x42] = [this](const char* data) -> size_t {
        size_t arg_offset = data - buff.data();
        if (arg_offset + 4 > buff.size()) throw std::runtime_error("BINBYTES: Not enough bytes for length");
        uint32_t len_val = read_le32(data);
        size_t len = static_cast<size_t>(len_val);
        if (arg_offset + 4 + len > buff.size()) throw std::runtime_error("BINBYTES: Not enough bytes for data (len: " + std::to_string(len) + ")");
        std::cout << "BINBYTES: " << len << " bytes" << std::endl;
        return 4 + len;
    };

    // SHORTBINBYTES (user's 0x8f)
    parserMap[0x8f] = [this](const char* data) -> size_t { 
        size_t arg_offset = data - buff.data();
        if (arg_offset + 1 > buff.size()) throw std::runtime_error("SHORTBINBYTES (0x8f): Not enough bytes for length");
        uint8_t len = static_cast<uint8_t>(data[0]);
        if (arg_offset + 1 + len > buff.size()) throw std::runtime_error("SHORTBINBYTES (0x8f): Not enough bytes for data (len: " + std::to_string(len) + ")");
        std::cout << "SHORTBINBYTES (0x8f): " << static_cast<int>(len) << " bytes" << std::endl;
        return 1 + len;
    };

    // GLOBAL (0x63 'c')
    parserMap[0x63] = [this](const char* data) -> size_t {
        size_t pos = 0; // Relative to 'data' pointer
        std::string module, name;
        size_t initial_arg_offset = data - buff.data(); // Offset of 'data' from start of 'buff'

        // Read module name
        while (initial_arg_offset + pos < buff.size() && buff[initial_arg_offset + pos] != '\n') {
            module += buff[initial_arg_offset + pos];
            pos++;
        }
        if (initial_arg_offset + pos >= buff.size() || buff[initial_arg_offset + pos] != '\n') {
            throw std::runtime_error("GLOBAL: No newline for module or buffer exhausted");
        }
        pos++; // Consume module's newline

        // Read object name
        while (initial_arg_offset + pos < buff.size() && buff[initial_arg_offset + pos] != '\n') {
            name += buff[initial_arg_offset + pos];
            pos++;
        }
        if (initial_arg_offset + pos >= buff.size() || buff[initial_arg_offset + pos] != '\n') {
            throw std::runtime_error("GLOBAL: No newline for name or buffer exhausted");
        }
        pos++; // Consume name's newline
        std::cout << "GLOBAL: module='" << module << "', name='" << name << "'" << std::endl;
        return pos; // Total bytes consumed from 'data' onwards
    };

    // BINPUT (0x71 'q')
    parserMap[0x71] = [this](const char* data) -> size_t {
        if ((data - buff.data()) + 1 > buff.size()) throw std::runtime_error("BINPUT: Not enough bytes for index");
        uint8_t index = static_cast<uint8_t>(data[0]);
        std::cout << "BINPUT: storing at index " << static_cast<int>(index) << std::endl;
        return 1;
    };

    // BINGET (0x68 'h')
    parserMap[0x68] = [this](const char* data) -> size_t {
        if ((data - buff.data()) + 1 > buff.size()) throw std::runtime_error("BINGET: Not enough bytes for index");
        uint8_t index = static_cast<uint8_t>(data[0]);
        std::cout << "BINGET: retrieving index " << static_cast<int>(index) << std::endl;
        return 1;
    };
    
    // GET (0x67 'g') - newline terminated string index
    parserMap[0x67] = [this](const char* data) -> size_t {
        size_t pos = 0;
        std::string index_str;
        size_t initial_arg_offset = data - buff.data();
        while (initial_arg_offset + pos < buff.size() && buff[initial_arg_offset + pos] != '\n') {
            index_str += buff[initial_arg_offset + pos];
            pos++;
        }
        if (initial_arg_offset + pos >= buff.size() || buff[initial_arg_offset + pos] != '\n') {
            throw std::runtime_error("GET (0x67): No newline or buffer exhausted for index string");
        }
        pos++; // Consume newline
        std::cout << "GET (0x67): retrieving from memo with index '" << index_str << "'" << std::endl;
        return pos;
    };

    // LONG_BINPUT (0x72 'r')
    parserMap[0x72] = [this](const char* data) -> size_t {
        if ((data - buff.data()) + 4 > buff.size()) {
            throw std::runtime_error("LONG_BINPUT (0x72): Not enough bytes for 4-byte index");
        }
        uint32_t index = read_le32(data);
        std::cout << "LONG_BINPUT (0x72 'r'): storing at index " << index << std::endl;
        return 4; 
    };

    // LONG_BINGET (0x6A 'j')
    parserMap[0x6A] = [this](const char* data) -> size_t {
        if ((data - buff.data()) + 4 > buff.size()) { 
            throw std::runtime_error("LONG_BINGET (0x6A): Not enough bytes for 4-byte index");
        }
        uint32_t index = read_le32(data);
        std::cout << "LONG_BINGET (0x6A 'j'): retrieving index " << index << std::endl;
        return 4;
    };

    // SETITEMS (0x75 'u')
    parserMap[0x75] = [](const char* ) -> size_t {
        std::cout << "SETITEMS: Setting multiple key-value pairs in dictionary" << std::endl;
        return 0;
    };

    // TUPLE (0x74 't')
    parserMap[0x74] = [](const char* ) -> size_t {
        std::cout << "TUPLE: Creating tuple from stack items" << std::endl;
        return 0;
    };

    // APPEND (0x61 'a')
    parserMap[0x61] = [](const char* ) -> size_t {
        std::cout << "APPEND: Appending item to list" << std::endl;
        return 0;
    };

    // APPENDS (0x65 'e')
    parserMap[0x65] = [](const char* ) -> size_t {
        std::cout << "APPENDS: Appending multiple items to list" << std::endl;
        return 0;
    };

    // SETITEM (0x73 's')
    parserMap[0x73] = [](const char* ) -> size_t {
        std::cout << "SETITEM: Setting a single key-value pair in dictionary" << std::endl;
        return 0;
    };

    // BUILD (0x62 'b')
    parserMap[0x62] = [](const char* ) -> size_t {
        std::cout << "BUILD: Setting object attributes" << std::endl;
        return 0;
    };

    // REDUCE (0x52 'R')
    parserMap[0x52] = [](const char* ) -> size_t {
        std::cout << "REDUCE (0x52 'R'): Applying callable to arguments" << std::endl;
        return 0;
    };
    
    // BINPERSID (0x51 'Q')
    parserMap[0x51] = [](const char* ) -> size_t {
        std::cout << "BINPERSID (0x51 'Q'): (Persistent ID from stack)" << std::endl;
        return 0; 
    };

    // TUPLE1 (0x85)
    parserMap[0x85] = [](const char* ) -> size_t {
        std::cout << "TUPLE1 (0x85): creating one-element tuple" << std::endl;
        return 0;
    };

    // TUPLE2 (0x86)
    parserMap[0x86] = [](const char* ) -> size_t {
        std::cout << "TUPLE2 (0x86): creating two-element tuple" << std::endl;
        return 0;
    };
    
    // NEWFALSE (0x89) - Corrected based on pickletools.dis()
    parserMap[0x89] = [](const char* ) -> size_t {
        std::cout << "NEWFALSE (0x89): pushing False" << std::endl; 
        return 0;
    };

    // EMPTY_TUPLE (0x29 ')')
    parserMap[0x29] = [](const char* ) -> size_t {
        std::cout << "EMPTY_TUPLE (0x29): pushing empty tuple" << std::endl;
        return 0;
    };

    // DUP (0x32 '2')
    parserMap[0x32] = [](const char* ) -> size_t {
        std::cout << "DUP (0x32 '2'): duplicating top of stack" << std::endl;
        return 0;
    };

    // Placeholder for opcode 0x02 - meaning in this file context is still unknown.
    parserMap[0x02] = [](const char* ) -> size_t {
        std::cout << "CUSTOM_OPCODE_0x02: encountered 0x02" << std::endl;
        return 0; 
    };
}

void PickleParser::parsePickle() {
    size_t i = 2; // Start after PROTO (buff[0]) and VERSION (buff[1])
    size_t file_s = static_cast<size_t>(size);

    while (i < file_s) {
        unsigned char opcode = static_cast<unsigned char>(buff[i]);
        auto it = parserMap.find(opcode);
        if (it != parserMap.end()) {
            size_t consumed_args_length = 0;
            try {
                // Pass pointer to the byte *after* the current opcode.
                // Handlers are responsible for their own bounds checking.
                consumed_args_length = it->second(buff.data() + i + 1);
            } catch (const std::exception& e) {
                 std::cerr << "Error during parsing opcode 0x" << std::hex << static_cast<int>(opcode) << std::dec 
                           << " ('" << (isprint(opcode) ? static_cast<char>(opcode) : '?') << "')"
                           << " at stream offset " << i << ": " << e.what() << std::endl;
                 throw; 
            }
            i += (1 + consumed_args_length); // 1 for opcode, plus length of its arguments
        } else {
            std::cout << "Unknown opcode: 0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(opcode) 
                      << " (decimal: " << std::dec << static_cast<int>(opcode) << ")"
                      << " ('" << (isprint(opcode) ? static_cast<char>(opcode) : '?') << "')"
                      << " at stream offset " << i << std::endl;
            i += 1;  // Skip unknown opcode and continue
        }
    }
     if (i == file_s ) {
        if (file_s > 0 && static_cast<unsigned char>(buff[file_s-1]) == 0x2e /* STOP */) {
             // Parsing finished exactly at file end and last was STOP, expected good outcome.
        } else if (file_s == 0 || (file_s > 0 && static_cast<unsigned char>(buff[file_s-1]) != 0x2e)) {
             std::cerr << "Warning: Parsing finished at end of file, but last opcode was not STOP (or file was empty/too short after header)." << std::endl;
        }
    } else if (i > file_s) {
        std::cerr << "Error: Parser overran buffer by " << (i - file_s) << " bytes. This should not happen." << std::endl;
    } else { // i < file_s
         std::cerr << "Warning: Parsing finished prematurely. " << (file_s - i) << " bytes remaining." << std::endl;
    }
}