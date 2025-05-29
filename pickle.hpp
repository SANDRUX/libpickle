#ifndef PICKLE_HPP
#define PICKLE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

class PickleParser {
public:
    explicit PickleParser(const std::string& fname);
    void pickleOpener();
    void parsePickle();
    void setParsers();
private:
    using opcodeParser = std::function<size_t(const char*)>;
    std::unordered_map<unsigned char, opcodeParser> parserMap;
    std::vector<char> buff;
    unsigned char cPickleVersion; // Changed to unsigned char
    std::string sPickleFileName;
    std::streamsize size; // Changed to std::streamsize for consistency with tellg/read
    
    uint32_t read_le32(const char* data) const;
    uint16_t read_le16(const char* data) const;
    int64_t decode_signed_long(const unsigned char* bytes, size_t len) const;
};

#endif // PICKLE_HPP