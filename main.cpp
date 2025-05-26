#include "pickle.hpp"

int main()
{   
    PickleParser parser("archive/data.pkl");

    try
    {
        parser.pickleOpener();
    }
    catch(const std::exception& e)
    {
        fprintf(stderr, "%s\n", e.what());
    }
    
    parser.setParsers();
    parser.parsePickle();

    return 0;
}