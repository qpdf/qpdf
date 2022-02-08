// We need to test the deprecated API
#ifdef POINTERHOLDER_TRANSITION
# undef POINTERHOLDER_TRANSITION
#endif
#define POINTERHOLDER_TRANSITION 0
#include <qpdf/PointerHolder.hh>

#include <iostream>
#include <stdlib.h>
#include <list>

class Object
{
  public:
    Object();
    ~Object();
    void hello();
    void hello() const;

  private:
    static int next_id;
    int id;
};


int Object::next_id = 0;

Object::Object()
{
    this->id = ++next_id;
    std::cout << "created Object, id " << this->id << std::endl;
}

Object::~Object()
{
    std::cout << "destroyed Object, id " << this->id << std::endl;
}

void
Object::hello()
{
    std::cout << "calling Object::hello for " << this->id << std::endl;
}

void
Object::hello() const
{
    std::cout << "calling Object::hello const for " << this->id << std::endl;
}

typedef PointerHolder<Object> ObjectHolder;

void callHello(ObjectHolder& oh)
{
    oh.getPointer()->hello();
    oh->hello();
    (*oh).hello();
}

void callHelloWithGet(ObjectHolder const& oh)
{
    oh.get()->hello();
    oh->hello();
    (*oh).hello();
}

int main(int argc, char* argv[])
{
    std::list<ObjectHolder> ol1;

    ObjectHolder oh0;
    {
        std::cout << "hello" << std::endl;
        Object* o1 = new Object;
        ObjectHolder oh1(o1);
        std::cout << "oh1 refcount = " << oh1.getRefcount() << std::endl;
        ObjectHolder oh2(oh1);
        std::cout << "oh1 refcount = " << oh1.getRefcount() << std::endl;
        std::cout << "oh2 refcount = " << oh2.use_count() << std::endl;
        ObjectHolder oh3(new Object);
        ObjectHolder oh4;
        ObjectHolder oh5;
        std::cout << "oh5 refcount = " << oh5.getRefcount() << std::endl;
        if (oh4 == oh5)
        {
            std::cout << "nulls equal" << std::endl;
        }
        oh3 = oh1;
        oh4 = oh2;
        if (oh3 == oh4)
        {
            std::cout << "equal okay" << std::endl;
        }
        if ((! (oh3 < oh4)) && (! (oh4 < oh3)))
        {
            std::cout << "less than okay" << std::endl;
        }
        ol1.push_back(oh3);
        ol1.push_back(oh3);
        Object* o3 = new Object;
        oh0 = o3;
        PointerHolder<Object const> oh6(new Object());
        oh6->hello();
    }

    ol1.front().getPointer()->hello();
    ol1.front()->hello();
    (*ol1.front()).hello();
    callHello(ol1.front());
    callHelloWithGet(ol1.front());
    ol1.pop_front();
    std::cout << "array" << std::endl;
    PointerHolder<Object> o_arr1_ph(true, new Object[2]);
    std::cout << "goodbye" << std::endl;
    return 0;
}
