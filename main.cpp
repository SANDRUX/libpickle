#include "pickle.hpp"
#include <iostream>

int main() {
    try {
        PickleParser parser("archive/data.pkl");
        parser.pickleOpener();
        parser.setParsers();
        parser.parsePickle();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}