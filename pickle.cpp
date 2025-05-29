#include "pickle.hpp"

PickleParser::PickleParser(const std::string& filename) : sPickleFileName(filename), cPickleVersion(0), size(0) {}

uint32_t PickleParser::read_le32(const char* data) const {
    return static_cast<uint32_t>(static_cast<unsigned char>(data[0])) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data[1])) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data[2])) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data[3])) << 24);
}

uint16_t PickleParser::read_le16(const char* data) const {
    return static_cast<uint16_t>(static_cast<unsigned char>(data[0])) |
           (static_cast<uint16_t>(static_cast<unsigned char>(data[1])) << 8);
}

void PickleParser::pickleOpener() {
    std::ifstream file(sPickleFileName, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + sPickleFileName);
    }
    size = file.tellg();
    file.seekg(0, std::ios::beg);
    buff.resize(size);
    file.read(buff.data(), size);
    file.close();

    if (size >= 2 && static_cast<unsigned char>(buff[0]) == 0x80) {
        cPickleVersion = static_cast<int>(static_cast<unsigned char>(buff[1]));
        std::cout << "Pickle version: " << cPickleVersion << std::endl;
    } else {
        throw std::runtime_error("Invalid pickle file: missing PROTO");
    }
}

void PickleParser::setParsers() {
    parserMap[static_cast<unsigned char>('.')] = [this](const char* data) -> size_t {
        std::cout << "STOP: Ending parsing" << std::endl;
        return 0;
    };
    parserMap[static_cast<unsigned char>('(')] = [this](const char* data) -> size_t {
        std::cout << "MARK: Pushing mark to stack" << std::endl;
        pickleStack.push(nullptr);
        return 0;
    };
    parserMap[static_cast<unsigned char>('}')] = [this](const char* data) -> size_t {
        std::cout << "EMPTY_DICT: Pushing empty dictionary" << std::endl;
        pickleStack.push(new Dictionary());
        return 0;
    };
    parserMap[static_cast<unsigned char>(']')] = [this](const char* data) -> size_t {
        std::cout << "EMPTY_LIST: Pushing empty list" << std::endl;
        pickleStack.push(new List());
        return 0;
    };
    parserMap[static_cast<unsigned char>('X')] = [this](const char* data) -> size_t {
        if (data + 4 > buff.data() + buff.size()) {
            throw std::runtime_error("BINUNICODE: Not enough bytes for length");
        }
        uint32_t len = read_le32(data);
        if (data + 4 + len > buff.data() + buff.size()) {
            throw std::runtime_error("BINUNICODE: Not enough bytes for string");
        }
        std::string str(data + 4, len);
        std::cout << "BINUNICODE: Length=" << len << ", String='" << str.substr(0, 50) << "...'" << std::endl;
        pickleStack.push(new BINUNICODE(str));
        return 4 + len;
    };
    parserMap[static_cast<unsigned char>('u')] = [this](const char* data) -> size_t {
        std::cout << "SETITEMS: Setting key-value pairs in dictionary" << std::endl;
        std::vector<IValue*> items;
        while (!pickleStack.empty() && pickleStack.top() != nullptr) {
            items.push_back(pickleStack.top());
            pickleStack.pop();
        }
        if (pickleStack.empty() || pickleStack.top() != nullptr) {
            throw std::runtime_error("SETITEMS: No mark found");
        }
        pickleStack.pop();
        if (pickleStack.empty()) {
            throw std::runtime_error("SETITEMS: No dictionary on stack");
        }
        Dictionary* dict = dynamic_cast<Dictionary*>(pickleStack.top());
        if (!dict) {
            throw std::runtime_error("SETITEMS: Top item is not a dictionary");
        }
        std::cout << "SETITEMS: Collected " << items.size() << " items" << std::endl;
        for (size_t i = 0; i < items.size(); ++i) {
            std::cout << "Item " << i << ": Type=" << items[i]->get_type();
            if (auto* unicode = dynamic_cast<BINUNICODE*>(items[i])) {
                std::cout << ", Value='" << unicode->value.substr(0, 50) << "...'";
            } else if (auto* int1 = dynamic_cast<BININT1*>(items[i])) {
                std::cout << ", Value=" << static_cast<int>(int1->value);
            } else if (auto* int2 = dynamic_cast<BININT2*>(items[i])) {
                std::cout << ", Value=" << int2->value;
            }
            std::cout << std::endl;
        }
        if (items.size() % 2 != 0) {
            throw std::runtime_error("SETITEMS: Invalid item count: " + std::to_string(items.size()));
        }
        for (size_t i = 0; i < items.size(); i += 2) {
            IValue* key = items[i + 1];
            IValue* value = items[i];
            BINUNICODE* str_key = dynamic_cast<BINUNICODE*>(key);
            if (!str_key) {
                throw std::runtime_error("SETITEMS: Key is not a string");
            }
            std::cout << "SETITEMS: Setting key '" << str_key->value.substr(0, 50) << "...'" << std::endl;
            dict->set(str_key->value, value);
        }
        std::cout << "SETITEMS: Stack size after: " << pickleStack.size() << std::endl;
        return 0;
    };
    parserMap[static_cast<unsigned char>('K')] = [this](const char* data) -> size_t {
        uint8_t value = static_cast<uint8_t>(data[0]);
        std::cout << "BININT1: Pushing integer " << static_cast<int>(value) << std::endl;
        pickleStack.push(new BININT1(value));
        return 1;
    };
    parserMap[static_cast<unsigned char>('M')] = [this](const char* data) -> size_t {
        if (data + 2 > buff.data() + buff.size()) {
            throw std::runtime_error("BININT2: Not enough bytes");
        }
        uint16_t value = read_le16(data);
        std::cout << "BININT2: Pushing integer " << value << std::endl;
        pickleStack.push(new BININT2(value));
        return 2;
    };
    parserMap[static_cast<unsigned char>('J')] = [this](const char* data) -> size_t {
        uint32_t value = read_le32(data);
        std::cout << "BININT: Pushing integer " << value << std::endl;
        pickleStack.push(new BININT(value));
        return 4;
    };
    parserMap[static_cast<unsigned char>('c')] = [this](const char* data) -> size_t {
        std::string module;
        size_t i = 0;
        while (i < size && data[i] != '\n') { module += data[i++]; }
        i++;
        std::string name;
        while (i < size && data[i] != '\n') { name += data[i++]; }
        std::cout << "GLOBAL: Module=" << module << ", Name=" << name << std::endl;
        pickleStack.push(new GlobalRef(module, name));
        return i + 1;
    };
    parserMap[static_cast<unsigned char>('a')] = [this](const char* data) -> size_t {
        std::cout << "APPEND: Appending item to list" << std::endl;
        if (pickleStack.size() < 2) {
            throw std::runtime_error("APPEND: Not enough items on stack");
        }
        IValue* item = pickleStack.top();
        pickleStack.pop();
        List* list = dynamic_cast<List*>(pickleStack.top());
        if (!list) {
            throw std::runtime_error("APPEND: Top item is not a list");
        }
        list->items.push_back(item);
        return 0;
    };
    parserMap[static_cast<unsigned char>('b')] = [this](const char* data) -> size_t {
        std::cout << "BUILD: Setting object state" << std::endl;
        if (pickleStack.size() < 2) {
            throw std::runtime_error("BUILD: Not enough items on stack");
        }
        IValue* state = pickleStack.top();
        pickleStack.pop();
        IValue* obj = pickleStack.top();
        Dictionary* dict = dynamic_cast<Dictionary*>(obj);
        if (dict && dynamic_cast<Dictionary*>(state)) {
            std::cout << "BUILD: Updating dictionary state" << std::endl;
        }
        return 0;
    };
    parserMap[static_cast<unsigned char>('e')] = [this](const char* data) -> size_t {
        std::cout << "APPENDS: Appending multiple items to list" << std::endl;
        std::vector<IValue*> items;
        while (!pickleStack.empty() && pickleStack.top() != nullptr) {
            items.push_back(pickleStack.top());
            pickleStack.pop();
        }
        if (pickleStack.empty() || pickleStack.top() != nullptr) {
            throw std::runtime_error("APPENDS: No mark found");
        }
        pickleStack.pop();
        if (pickleStack.empty()) {
            throw std::runtime_error("APPENDS: No list on stack");
        }
        List* list = dynamic_cast<List*>(pickleStack.top());
        if (!list) {
            throw std::runtime_error("APPENDS: Top item is not a list");
        }
        for (auto* item : items) {
            list->items.push_back(item);
        }
        return 0;
    };
    parserMap[static_cast<unsigned char>('t')] = [this](const char* data) -> size_t {
        std::cout << "TUPLE: Creating tuple from stack items" << std::endl;
        std::vector<IValue*> items;
        while (!pickleStack.empty() && pickleStack.top() != nullptr) {
            items.push_back(pickleStack.top());
            pickleStack.pop();
        }
        if (pickleStack.empty() || pickleStack.top() != nullptr) {
            throw std::runtime_error("TUPLE: No mark found on stack");
        }
        pickleStack.pop();
        std::reverse(items.begin(), items.end());
        Tuple* tuple = new Tuple();
        tuple->items = items;
        pickleStack.push(tuple);
        std::cout << "TUPLE: Pushed tuple with " << items.size() << " items" << std::endl;
        return 0;
    };

    if (cPickleVersion >= 2) {
        parserMap[static_cast<unsigned char>(0x80)] = [this](const char* data) -> size_t {
            std::cout << "PROTO: Protocol version " << static_cast<int>(static_cast<unsigned char>(data[0])) << std::endl;
            return 1;
        };
        parserMap[static_cast<unsigned char>(0x71)] = [this](const char* data) -> size_t {
            unsigned char index = static_cast<unsigned char>(data[0]);
            if (!pickleStack.empty()) {
                memo[index] = pickleStack.top();
                std::cout << "BINPUT: Stored object at index " << static_cast<int>(index) << std::endl;
            } else {
                throw std::runtime_error("BINPUT: Stack empty at index " + std::to_string(index));
            }
            return 1;
        };
        parserMap[static_cast<unsigned char>('h')] = [this](const char* data) -> size_t {
            unsigned char index = static_cast<unsigned char>(data[0]);
            if (memo.count(index)) {
                pickleStack.push(memo[index]);
                std::cout << "BINGET: Retrieving index " << static_cast<int>(index) << std::endl;
            } else {
                throw std::runtime_error("Memo index not found: " + std::to_string(index));
            }
            return 1;
        };
        parserMap[static_cast<unsigned char>(0x85)] = [this](const char* data) -> size_t {
            std::cout << "TUPLE1: Creating 1-tuple" << std::endl;
            if (pickleStack.empty()) {
                throw std::runtime_error("TUPLE1: Stack empty");
            }
            IValue* item = pickleStack.top();
            pickleStack.pop();
            Tuple* tuple = new Tuple();
            tuple->items.push_back(item);
            pickleStack.push(tuple);
            return 0;
        };
        parserMap[static_cast<unsigned char>(0x86)] = [this](const char* data) -> size_t {
            std::cout << "TUPLE2: Creating 2-tuple" << std::endl;
            if (pickleStack.size() < 2) {
                throw std::runtime_error("TUPLE2: Not enough items on stack");
            }
            IValue* second = pickleStack.top();
            pickleStack.pop();
            IValue* first = pickleStack.top();
            pickleStack.pop();
            Tuple* tuple = new Tuple();
            tuple->items = {first, second};
            pickleStack.push(tuple);
            return 0;
        };
        parserMap[static_cast<unsigned char>(0x87)] = [this](const char* data) -> size_t {
            std::cout << "TUPLE3: Creating 3-tuple" << std::endl;
            if (pickleStack.size() < 3) {
                throw std::runtime_error("TUPLE3: Not enough items on stack");
            }
            IValue* third = pickleStack.top();
            pickleStack.pop();
            IValue* second = pickleStack.top();
            pickleStack.pop();
            IValue* first = pickleStack.top();
            pickleStack.pop();
            Tuple* tuple = new Tuple();
            tuple->items = {first, second, third};
            pickleStack.push(tuple);
            return 0;
        };
        parserMap[static_cast<unsigned char>(0x88)] = [this](const char* data) -> size_t {
            std::cout << "NEWTRUE: Pushing True" << std::endl;
            pickleStack.push(new BININT1(1));
            return 0;
        };
        parserMap[static_cast<unsigned char>(0x89)] = [this](const char* data) -> size_t {
            std::cout << "NEWFALSE: Pushing False" << std::endl;
            pickleStack.push(new BININT1(0));
            return 0;
        };
        parserMap[static_cast<unsigned char>(0x8a)] = [this](const char* data) -> size_t {
            if (data + 1 > buff.data() + buff.size()) {
                throw std::runtime_error("LONG1: Not enough bytes for length");
            }
            uint8_t len = static_cast<uint8_t>(data[0]);
            if (data + 1 + len > buff.data() + buff.size()) {
                throw std::runtime_error("LONG1: Not enough bytes for value");
            }
            int64_t val = 0;
            for (size_t i = 0; i < len; ++i) {
                val |= static_cast<int64_t>(static_cast<unsigned char>(data[i + 1])) << (i * 8);
            }
            if (len > 0 && (data[len] & 0x80)) {
                val |= ~((1LL << (len * 8)) - 1);
            }
            std::cout << "LONG1: Pushing long " << val << " with length " << static_cast<int>(len) << std::endl;
            pickleStack.push(new LONG1(val));
            return len + 1;
        };
        parserMap[static_cast<unsigned char>(0x8b)] = [this](const char* data) -> size_t {
            uint32_t len = read_le32(data);
            if (data + 4 + len > buff.data() + buff.size()) {
                throw std::runtime_error("LONG4: Not enough bytes");
            }
            int64_t val = 0;
            for (size_t i = 0; i < len; ++i) {
                val |= static_cast<int64_t>(static_cast<unsigned char>(data[i + 4])) << (i * 8);
            }
            if (len > 0 && (data[len + 3] & 0x80)) {
                val |= ~((1LL << (len * 8)) - 1);
            }
            std::cout << "LONG4: Pushing long " << val << " with length " << len << std::endl;
            pickleStack.push(new LONG4(val));
            return len + 4;
        };
        parserMap[static_cast<unsigned char>(0x50)] = [this](const char* data) -> size_t {
            std::string pid;
            size_t i = 0;
            // Read until newline or end of buffer
            while (i < size && data[i] != '\n') { pid += data[i++]; }
            
            // Debug: Print the persistent ID and surrounding bytes
            std::cout << "PERSID: Persistent ID '" << pid << "'" << std::endl;
            std::cout << "Next 10 bytes after PID: ";
            for (size_t j = 0; j < 10 && i + j < size; ++j) {
                std::cout << std::hex << static_cast<int>(static_cast<unsigned char>(data[i + j])) << " ";
            }
            std::cout << std::dec << std::endl;

            // Validate pid
            if (pid.empty()) {
                throw std::runtime_error("PERSID: Persistent ID is empty, cannot construct valid filepath");
            }

            std::string filepath = "archive/data/" + pid;
            std::ifstream file(filepath, std::ios::binary);
            std::cout << "PIIIIIIIIIIIIIIIIIIIID " << pid << "PIIIIIID"<< std::endl; 
            if (!file.is_open()) {
                throw std::runtime_error("PERSID: Could not open file: " + filepath);
            }
            std::vector<char> file_data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            pickleStack.push(new BINBYTES(std::vector<uint8_t>(file_data.begin(), file_data.end())));
            return i + 1; // Return bytes consumed, including the newline
        };
        parserMap[static_cast<unsigned char>('Q')] = [this](const char* data) -> size_t {
            std::cout << "BINPERSID: Processing persistent ID" << std::endl;
            if (pickleStack.empty()) {
                throw std::runtime_error("BINPERSID: Stack empty");
            }
            IValue* top = pickleStack.top();
            Tuple* tuple = dynamic_cast<Tuple*>(top);
            if (!tuple || tuple->items.size() < 3) {
                throw std::runtime_error("BINPERSID: Expected tuple with at least 3 items");
            }
            BINUNICODE* pid_str = dynamic_cast<BINUNICODE*>(tuple->items[2]);
            if (!pid_str) {
                throw std::runtime_error("BINPERSID: Expected string in tuple position 2");
            }
            std::string pid = pid_str->value;
            std::cout << "Persistent ID: " << pid << std::endl;
            std::string filename = "archive/data/" + pid;
            std::ifstream file(filename, std::ios::binary);
            if (!file.is_open()) {
                throw std::runtime_error("BINPERSID: Cannot open file " + filename);
            }
            file.seekg(0, std::ios::end);
            size_t size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<uint8_t> buffer(size);
            file.read(reinterpret_cast<char*>(buffer.data()), size);
            file.close();
            pickleStack.pop();
            pickleStack.push(new BINBYTES(buffer));
            std::cout << "BINPERSID: Loaded " << size << " bytes from " << filename << std::endl;
            return 0;
        };
    }
}

void PickleParser::parsePickle() {
    size_t i = 0;
    while (i < size) {
        unsigned char opcode = static_cast<unsigned char>(buff[i]);
        std::cout << "Processing opcode: 0x" << std::hex << static_cast<int>(opcode) << std::dec
                  << ", position: " << i << ", stack size: " << pickleStack.size() << std::endl;
        if (parserMap.find(opcode) != parserMap.end()) {
            size_t consumed = parserMap[opcode](buff.data() + i + 1);
            std::cout << "Consumed " << consumed << " bytes, advancing to position " << i + 1 + consumed << std::endl;
            i += 1 + consumed;
        } else {
            std::cout << "Unknown opcode: " << static_cast<int>(opcode) << " at position " << i << std::endl;
            std::cout << "Next 20 bytes: ";
            for (size_t j = i; j < i + 20 && j < size; ++j) {
                std::cout << std::hex << static_cast<int>(static_cast<unsigned char>(buff[j])) << " ";
            }
            std::cout << std::dec << std::endl;
            std::cout << "ASCII: ";
            for (size_t j = i; j < i + 20 && j < size; ++j) {
                char c = buff[j];
                std::cout << (std::isprint(static_cast<unsigned char>(c)) ? c : '.');
            }
            std::cout << std::endl;
            i += 1;  // Skip the unknown opcode
        }
    }
}