#include "pickle.hpp"

int main()
{   
    PickleParser parser("archive/data.pkl");

    try
    {
        parser.pickleOpener();
        parser.setParsers();
        parser.parsePickle();
    }
    catch(const std::exception& e)
    {
        fprintf(stderr, "%s\n", e.what());
    }

    return 0;
}