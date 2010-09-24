#include <qpdf/PointerHolder.hh>

#include <iostream>
#include <stdlib.h>
#include <list>

#include <qpdf/QUtil.hh>

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

void callHello(ObjectHolder const& oh)
{
    oh.getPointer()->hello();
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
	ObjectHolder oh2(oh1);
	ObjectHolder oh3(new Object);
	ObjectHolder oh4;
	ObjectHolder oh5;
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
    }

    ol1.front().getPointer()->hello();
    ol1.front()->hello();
    (*ol1.front()).hello();
    callHello(ol1.front());
    ol1.pop_front();
    std::cout << "goodbye" << std::endl;
    return 0;
}
