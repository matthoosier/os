#include <iostream>
#include <string>
#include "include/kernel/list.hpp"

class Apple
{
public:
    Apple (const std::string & type)
        : mType(type)
        , mLinks()
    {
    }

    std::string         mType;
    ListElement         mLinks;
};

int main ()
{
    Apple a("Granny smith");
    Apple b("Braeburn");

    typedef List<Apple, &Apple::mLinks> ListType;

    ListType list;

    list.Append(&b);
    list.Append(&a);

    ListType::Iterator i = list.Begin();

    for (ListType::Iterator i = list.Begin(); i; i++) {
        std::cout << "Name: " << i->mType << std::endl;
        list.Remove(*i);
    }
}
