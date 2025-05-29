#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <stack>
#include <functional>
#include <cstdint>
#include <iostream>
#include <fstream>

struct IValue {
    size_t length = 0;
    virtual ~IValue() = default;
    virtual std::string get_type() const { return typeid(*this).name(); }
};

struct Dictionary : public IValue {
    std::unordered_map<std::string, IValue*> data;
    void set(const std::string& key, IValue* value) {
        data[key] = value;
    }
    IValue* get(const std::string& key) const {
        auto it = data.find(key);
        return it != data.end() ? it->second : nullptr;
    }
    ~Dictionary() {
        for (auto& pair : data) delete pair.second;
    }
};

struct GlobalRef : public IValue {
    GlobalRef(std::string m, std::string n) : module(m), name(n) {}
    std::string module;
    std::string name;
};

struct Tuple : public IValue {
    std::vector<IValue*> items;
    ~Tuple() {
        for (auto* item : items) delete item;
    }
};

struct List : public IValue {
    std::vector<IValue*> items;
    ~List() {
        for (auto* item : items) delete item;
    }
};

struct BININT1 : public IValue {
    uint8_t value;
    explicit BININT1(uint8_t v) : value(v) {}
};

struct BININT2 : public IValue {
    uint16_t value;
    explicit BININT2(uint16_t v) : value(v) {}
};

struct BININT : public IValue {
    uint32_t value;
    explicit BININT(uint32_t v) : value(v) {}
};

struct LONG : public IValue {
    long long value;
    explicit LONG(long long v) : value(v) {}
};

struct LONG1 : public IValue {
    int64_t value;
    explicit LONG1(int64_t v) : value(v) {}
};

struct LONG4 : public IValue {
    int64_t value;
    explicit LONG4(int64_t v) : value(v) {}
};

struct BINSTRING : public IValue {
    std::string value;
    explicit BINSTRING(const std::string& v) : value(v) {}
};

struct BINBYTES : public IValue {
    std::vector<uint8_t> value;
    explicit BINBYTES(const std::vector<uint8_t>& v) : value(v) {}
};

struct SHORTBINBYTES : public IValue {
    std::vector<uint8_t> value;
    explicit SHORTBINBYTES(const std::vector<uint8_t>& v) : value(v) {}
};

struct BINUNICODE : public IValue {
    std::string value;
    explicit BINUNICODE(const std::string& v) : value(v) {}
};

struct BINUNICODE8 : public IValue {
    std::string value;
    explicit BINUNICODE8(const std::string& v) : value(v) {}
};

struct SHORTBINUNICODE : public IValue {
    std::string value;
    explicit SHORTBINUNICODE(const std::string& v) : value(v) {}
};

struct PersID : public IValue {
    std::string id;
    explicit PersID(const std::string& pid) : id(pid) {}
};

class PickleParser {
public:
    explicit PickleParser(const std::string&);
    void pickleOpener();
    void parsePickle();
    void setParsers();
private:
    using opcodeParser = std::function<size_t(const char*)>;
    std::unordered_map<unsigned char, opcodeParser> parserMap;
    std::vector<char> buff;
    int cPickleVersion;
    std::string sPickleFileName;
    size_t size;
    std::unordered_map<int, IValue*> memo;
    std::stack<IValue*> pickleStack;
    uint32_t read_le32(const char* data) const;
    uint16_t read_le16(const char* data) const;
};