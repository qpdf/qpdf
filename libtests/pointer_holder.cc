// We need to test the deprecated API
#ifdef POINTERHOLDER_TRANSITION
# undef POINTERHOLDER_TRANSITION
#endif
#define POINTERHOLDER_TRANSITION 0
#include <qpdf/PointerHolder.hh>

#include <iostream>
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

void
callHello(ObjectHolder& oh)
{
    oh.getPointer()->hello();
    oh->hello();
    (*oh).hello();
}

void
callHelloWithGet(ObjectHolder const& oh)
{
    oh.get()->hello();
    oh->hello();
    (*oh).hello();
}

void
test_ph()
{
    std::list<ObjectHolder> ol1;

    ObjectHolder oh0;
    {
        std::cout << "hello" << std::endl;
        auto* o1 = new Object;
        ObjectHolder oh1(o1);
        std::cout << "oh1 refcount = " << oh1.getRefcount() << std::endl;
        ObjectHolder oh2(oh1);
        std::cout << "oh1 refcount = " << oh1.getRefcount() << std::endl;
        std::cout << "oh2 refcount = " << oh2.use_count() << std::endl;
        ObjectHolder oh3(new Object);
        ObjectHolder oh4;
        ObjectHolder oh5;
        std::cout << "oh5 refcount = " << oh5.getRefcount() << std::endl;
        if (oh4 == oh5) {
            std::cout << "nulls equal" << std::endl;
        }
        oh3 = oh1;
        oh4 = oh2;
        if (oh3 == oh4) {
            std::cout << "equal okay" << std::endl;
        }
        if ((!(oh3 < oh4)) && (!(oh4 < oh3))) {
            std::cout << "less than okay" << std::endl;
        }
        ol1.push_back(oh3);
        ol1.push_back(oh3);
        auto* o3 = new Object;
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
}

PointerHolder<Object>
make_object_ph()
{
    return new Object;
}

std::shared_ptr<Object>
make_object_sp()
{
    return std::make_shared<Object>();
}

PointerHolder<Object const>
make_object_const_ph()
{
    return new Object;
}

std::shared_ptr<Object const>
make_object_const_sp()
{
    return std::make_shared<Object const>();
}

void
hello_ph(PointerHolder<Object> o)
{
    o->hello();
}

void
hello_sp(std::shared_ptr<Object> o)
{
    o->hello();
}

void
hello_ph_const(PointerHolder<Object const> o)
{
    o->hello();
}

void
hello_sp_const(std::shared_ptr<Object const> o)
{
    o->hello();
}

void
ph_sp_compat()
{
    // Ensure bidirectional compatibility between PointerHolder and
    // shared_ptr.
    std::cout << "compat" << std::endl;
    PointerHolder<Object> ph_from_ph = make_object_ph();
    std::shared_ptr<Object> sp_from_ph = make_object_ph();
    PointerHolder<Object> ph_from_sp = make_object_sp();
    std::shared_ptr<Object> sp_from_sp = make_object_sp();
    hello_sp(ph_from_ph);
    hello_ph(sp_from_ph);
    hello_sp(ph_from_sp);
    hello_ph(sp_from_sp);
    PointerHolder<Object const> ph_const_from_ph = make_object_const_ph();
    std::shared_ptr<Object const> sp_const_from_ph = make_object_const_ph();
    PointerHolder<Object const> ph_const_from_sp = make_object_const_sp();
    std::shared_ptr<Object const> sp_const_from_sp = make_object_const_sp();
    hello_sp_const(ph_const_from_ph);
    hello_ph_const(sp_const_from_ph);
    hello_sp_const(ph_const_from_sp);
    hello_ph_const(sp_const_from_sp);
    PointerHolder<Object> arr1_ph;
    {
        std::cout << "initialize ph array from shared_ptr" << std::endl;
        std::shared_ptr<Object> arr1(new Object[2], std::default_delete<Object[]>());
        arr1_ph = arr1;
    }
    std::cout << "delete ph array" << std::endl;
    arr1_ph = nullptr;
    std::shared_ptr<Object> arr2_sp;
    {
        std::cout << "initialize sp array from PointerHolder" << std::endl;
        PointerHolder<Object> arr2(true, new Object[2]);
        arr2_sp = arr2;
    }
    std::cout << "delete sp array" << std::endl;
    arr2_sp = nullptr;
    std::cout << "end compat" << std::endl;
}

std::list<PointerHolder<Object>>
get_ph_list()
{
    std::list<PointerHolder<Object>> l = {
        make_object_sp(),
        make_object_ph(),
    };
    return l;
}

std::list<std::shared_ptr<Object>>
get_sp_list()
{
    std::list<std::shared_ptr<Object>> l = {
        make_object_sp(),
        make_object_ph(),
    };
    return l;
}

void
ph_sp_containers()
{
    std::cout << "containers" << std::endl;
    // Demonstrate that using auto makes it easy to switch interfaces
    // from using a container of one shared pointer type to a
    // container of the other.
    auto phl1 = get_ph_list();
    auto phl2 = get_sp_list();
    std::cout << "end containers" << std::endl;
}

int
main(int argc, char* argv[])
{
    test_ph();
    ph_sp_compat();
    ph_sp_containers();
    return 0;
}
