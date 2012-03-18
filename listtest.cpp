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
    ListElement<Apple>  mLinks;
};

int main ()
{
    Apple a("Granny smith");
    Apple b("Braeburn");

    List<Apple> list(&Apple::mLinks);

    list.Prepend(&a);
    list.Append(&b);

    List<Apple>::Iterator i = list.Begin();

    for (List<Apple>::Iterator i = list.Begin(); i; i++) {
        std::cout << "Name: " << i->mType << std::endl;
        list.Remove(*i);
    }
}
