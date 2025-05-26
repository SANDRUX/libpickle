#include <cstdint>
#include <functional>
#include <fstream>
#include <string>
#include <unordered_map>

class PickleParser
{
public: 
    PickleParser(const std::string &);
    void pickleOpener();    
    void parsePickle();
    void setParsers();
    using opcodeParser = size_t(*)(const char*);
    
private:
    using opcodeParserMapType = std::unordered_map<unsigned char, opcodeParser>;
    
    opcodeParserMapType parserMap;
    std::vector<char> buff;
    int cPickleVersion;
    std::string sPickleFileName;
    size_t size;
};

size_t readBININT(const char*);
size_t readBININT1(const char*);
size_t readBININT2(const char*);
size_t readBINUNICODE(const char*);
size_t readSHORTBINUNICODE(const char*);
size_t readLONG(const char*);
size_t readLONG1(const char*);
size_t readLONG4(const char*);
size_t readBINSTRING(const char*);
size_t readBINBYTES(const char*);
size_t readBINUNICODE8(const char*);
size_t readSHORTUNICODE(const char*);
size_t readSHORTBINBYTES(const char *);