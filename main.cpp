#include "pickle.hpp"
#include <iostream>
#include <string> // Required for std::string

int main(int argc, char* argv[]) { // Added argc, argv for flexibility
    std::string pickle_file_path = "archive/data.pkl"; // Default path

    if (argc > 1) { // Allow specifying file path via command line
        pickle_file_path = argv[1];
    }
    std::cout << "Attempting to parse: " << pickle_file_path << std::endl;

    try {
        PickleParser parser(pickle_file_path);
        parser.pickleOpener();
        parser.setParsers();
        parser.parsePickle();
        std::cout << "Pickle parsing finished." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}