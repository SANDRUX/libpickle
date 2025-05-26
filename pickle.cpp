#include "pickle.hpp"
#include <exception>
#include <bit>

PickleParser::PickleParser(const std::string& fname) : sPickleFileName(fname) { }

void PickleParser::pickleOpener()
{
    std::ifstream pickleFile(sPickleFileName, std::ios_base::binary | std::ios_base::ate);
    size = pickleFile.tellg();
    pickleFile.seekg(0, std::ios::beg);

    buff.resize(size); // set the intitial size of the vector 

    if (!pickleFile.read(buff.data(), size))
    {
        throw std::runtime_error("couldn't load pickle data from the file! \n");
    }   

    if (size >= 2 && static_cast<unsigned char>(buff[0]) == 0x80)
    {
        cPickleVersion = static_cast<int>(buff[1]);
        fprintf(stdout, "Pickle successfully opened. The protocol vesion: %d\n", cPickleVersion);
    }
    else
    {
        throw std::runtime_error("Invalid pickle format detected! \n");
    }
} 

void PickleParser::setParsers()
{
    parserMap[0x4b] = readBININT1;
    parserMap[0x4d] = readBININT2;
    parserMap[0x4a] = readBININT;
    parserMap[0x4c] = readLONG;
    parserMap[0x8a] = readLONG1;
    parserMap[0x8b] = readLONG4;
    parserMap[0x54] = readBINSTRING;
    parserMap[0x42] = readBINBYTES;
    parserMap[0x58] = readBINUNICODE;
    parserMap[0x8d] = readBINUNICODE8;
    parserMap[0x8c] = readSHORTBINUNICODE;
    parserMap[0x8f] = readSHORTBINBYTES;
}

void PickleParser::parsePickle()
{
    for (size_t i = 2; i < size;)
    {
        auto it = parserMap.find(buff[i]);
        if (it != parserMap.end())
        {
            i += it->second((buff.data() + i));
            continue;
        }
        else 
        {
            std::printf("Unknown opcode: %2x\n", buff[i]);
            i++;
            continue;
        }
        if (buff[i] == 0x2e)
        {
            std::printf("THE END\n");
            break;
        }
        i++;
    }
}

size_t readBININT1(const char* data)
{
    std::printf("BININT1: %d\n", static_cast<uint8_t>(*data));
    return 1;
}

size_t readBININT2(const char* data)
{
    uint16_t value = (static_cast<uint8_t>(*data)) | (static_cast<uint8_t>(*(data + 1)) << 8);

    std::printf("BININT2: %d\n", value);
    return 2;
}

size_t readBININT(const char* data)
{
    int32_t value = (static_cast<uint8_t>(*data)) | (static_cast<uint8_t>(*(data + 1)) << 8) |
    (static_cast<uint8_t>(*(data + 2)) << 16) | (static_cast<uint8_t>(*(data + 3)) << 24);
    
    std::printf("BININT: %d\n", value);
    return 4;
}

size_t readLONG(const char* data)
{
    std::string numString;
    int index = 0;

    while (data[index] != '\n')
    {
        numString += data[index];
        index ++;
    }
    ++index;

    long long value = std::stoll(numString);
    std::printf("LONG: %lld\n", value);

    return index;
}

size_t readLONG1(const char* data)
{
    uint8_t length = static_cast<uint8_t>(*data); 
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data + 1);

    if (length > 8)
    {
        throw std::runtime_error("LONG1 length too large!");
        return 0; 
    }

    int64_t result = 0;
    if (length > 0) 
    {
        bool negative = (bytes[length - 1] & 0x80) != 0;

        for (int i = length - 1; i >= 0; --i) 
        {
            result <<= 8;
            result |= static_cast<int64_t>(bytes[i]);
        }

        if (negative) 
        {
            for (int i = length; i < 8; ++i)
            {
                result |= static_cast<int64_t>(0xFF) << (i * 8);
            }
        }
    }

    std::printf("LONG1: %ld\n", result);

    return length + 1;
}

size_t readLONG4(const char* data)
{
    uint32_t length = 
        static_cast<uint8_t>(data[0]) |
        (static_cast<uint8_t>(data[1]) << 8) |
        (static_cast<uint8_t>(data[2]) << 16) |
        (static_cast<uint8_t>(data[3]) << 24);

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data + 4);

    if (length > 8) {
        throw std::runtime_error("LONG4 length too large!");
        return 0;
    }

    int64_t result = 0;
    if (length > 0) 
    {
        bool negative = (bytes[length - 1] & 0x80) != 0;

        for (int i = length - 1; i >= 0; --i) 
        {
            result <<= 8;
            result |= static_cast<int64_t>(bytes[i]);
        }

        if (negative) 
        {
            for (int i = length; i < 8; ++i)
                result |= static_cast<int64_t>(0xFF) << (i * 8);
        }
    }

    std::printf("LONG4: %ld\n", result);

    return length + 4;
}

size_t readBINSTRING(const char* data)
{
    uint32_t length = static_cast<uint8_t>(data[1]) |
                    (static_cast<uint8_t>(data[2]) << 8) |
                    (static_cast<uint8_t>(data[3]) << 16) |
                    (static_cast<uint8_t>(data[4]) << 24);

    std::string result(data + 4, data + 4 + length);

    std::printf("BINSTRING: \"%s\"\n", result.c_str());

    return length + 4;
}

size_t readBINBYTES(const char* data)
{
    uint32_t length = static_cast<uint8_t>(data[1]) |
                      (static_cast<uint8_t>(data[2]) << 8) |
                      (static_cast<uint8_t>(data[3]) << 16) |
                      (static_cast<uint8_t>(data[4]) << 24);

    std::printf("BINBYTES: ");
    for (size_t i = 0; i < length; i++)
    {
        std::printf("%02X ", data[i + 4]);
    }
    std::printf("\n");

    return length + 4;
}

size_t readSHORTBINBYTES(const char* data)
{
    uint8_t length = static_cast<uint8_t>(data[0]);

    std::printf("SHORT_BINBYTES: ");
    for (size_t i = 0; i < length; i++)
    {
        std::printf("%02X ", data[i + 4]);
    }
    std::printf("\n");

    return length + 1;
}

size_t readBINUNICODE(const char* data)
{
    uint32_t length = static_cast<uint8_t>(data[0]) |
                      (static_cast<uint8_t>(data[1]) << 8) |
                      (static_cast<uint8_t>(data[2]) << 16) |
                      (static_cast<uint8_t>(data[3]) << 24);

    std::string utf8str(data + 4, data + 4 + length);

    std::printf("BINUNICODE: %s\n", utf8str.c_str());

    return length + 4;
}

size_t readBINUNICODE8(const char* data)
{
    uint64_t length = 0;
    for (int i = 0; i < 8; ++i) 
    {
        length |= (static_cast<uint64_t>(static_cast<uint8_t>(data[i])) << (8 * i));
    }

    std::string utf8str(data + 8, data + 8 + length);

    std::printf("BINUNICODE8: %s\n", utf8str.c_str());

    return length + 8;
}

size_t readSHORTBINUNICODE(const char* data)
{
    uint8_t length = static_cast<uint8_t>(data[0]);
    std::string utf8str(data + 1, data + 1 + length);

    std::printf("SHORT_BINUNICODE: %s\n", utf8str.c_str());

    return length + 1;
}

