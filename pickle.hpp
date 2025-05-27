#include <cstdint>
#include <functional>
#include <fstream>
#include <string>
#include <unordered_map>
#include <stack>

struct IValue
{
    virtual ~IValue() = default;
    // virtual void printType() const = 0;
    size_t length;
};

struct Dictionary : public IValue
{
    std::vector <std::unordered_map<IValue *, IValue *>> key_value;
};

struct BININT1 : public IValue
{
    uint8_t value;
};

struct BININT2 : public IValue
{
    uint16_t value;
};

struct BININT : public IValue
{
    uint32_t value;
};

struct LONG : public IValue
{
    long long value;
};

struct LONG1 : public IValue
{
    int64_t value;
};

struct LONG4 : public IValue
{
    int64_t value;
};

struct BINSTRING : public IValue
{
    std::string value;
};

struct BINBYTES : public IValue
{
    std::vector <uint8_t> value;
};

struct SHORTBINBYTES : public IValue
{
    std::vector <uint8_t> value;
};

struct BINUNICODE : public IValue
{
    std::string value;
};

struct BINUNICODE8 : public IValue
{
    std::string value;
};

struct SHORTBINUNICODE : public IValue
{
    std::string value;
};

class PickleParser
{
public: 
    PickleParser(const std::string &);
    void pickleOpener();    
    void parsePickle();
    void setParsers();
    using opcodeParser = size_t(*)(const char*, IValue**);
    
private:
    using opcodeParserMapType = std::unordered_map<unsigned char, opcodeParser>;
    
    opcodeParserMapType parserMap;
    std::vector<char> buff;
    int cPickleVersion;
    std::string sPickleFileName;
    size_t size;
    std::unordered_map<int, IValue *> memo;
    std::stack<IValue*> pickleStack;
};

size_t readBININT(const char, IValue**);
size_t readBININT1(const char*, IValue**);
size_t readBININT2(const char*, IValue**);
size_t readBINUNICODE(const char*, IValue**);
size_t readSHORTBINUNICODE(const char*, IValue**);
size_t readLONG(const char*, IValue**);
size_t readLONG1(const char*, IValue**);
size_t readLONG4(const char*, IValue**);
size_t readBINSTRING(const char*, IValue**);
size_t readBINBYTES(const char*, IValue**);
size_t readBINUNICODE8(const char*, IValue**);
size_t readSHORTUNICODE(const char*, IValue**);
size_t readSHORTBINBYTES(const char*, IValue**);