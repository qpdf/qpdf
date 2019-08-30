#include <iostream>
#include <cassert>
#include <functional>
#include <type_traits>
#include <cstdint>
#include <vector>
#include <map>
#include <memory>

// Functional programming

// Function that returns a callable in the form of a lambda
std::function<int (int)> make_adder(int x)
{
    return ([=](int a) -> int{ return x + a; });
}

void do_functional()
{
    // Lambda with no capture
    auto simple_lambda = [](int a){ return a + 3; };
    assert(simple_lambda(5) == 8);

    // Capture by value
    int x = 5;
    auto by_value = [x](int a){ return a + x; };
    assert(by_value(1) == 6);
    x = 7; // change not seen by lambda
    assert(by_value(1) == 6);
    // Also >> at end of template
    assert((std::is_convertible<decltype(by_value),
                                std::function<int(int)>>::value));

    // Capture by reference
    auto by_reference = [&x](int a){ return a + x; };
    assert(by_reference(1) == 8);
    x = 8; // change seen my lambda
    assert(by_reference(1) == 9);

    // Get a callable from a function
    auto add3 = make_adder(3);
    assert(add3(5) == 8);

    auto make_addr_lambda = [](int a) {
        return [a](int b) { return a + b; };
    };

    assert(make_addr_lambda(6)(8) == 14);
}

// Integer types, type traits

template <typename T>
void check_size(size_t size, bool is_signed)
{
    assert(sizeof(T) == size);
    assert(std::is_signed<T>::value == is_signed);
}

void do_inttypes()
{
    // static_assert is a compile-time check
    static_assert(1 == sizeof(int8_t), "int8_t check");
    check_size<int8_t>(1, true);
    check_size<uint8_t>(1, false);
    check_size<int16_t>(2, true);
    check_size<uint16_t>(2, false);
    check_size<int32_t>(4, true);
    check_size<uint32_t>(4, false);
    check_size<int64_t>(8, true);
    check_size<uint64_t>(8, false);

    // auto, decltype
    auto x = 5LL;
    check_size<decltype(x)>(8, true);
    assert((std::is_same<long long, decltype(x)>::value));
}

class A
{
  public:
    static constexpr auto def_value = 5;
    A(int x) :
        x(x)
    {
    }
    // Constructor delegation
    A() : A(def_value)
    {
    }
    int getX() const
    {
        return x;
    }

  private:
    int x;
};

void do_iteration()
{
    // Initializers, foreach syntax, auto for iterators
    std::vector<int> v = { 1, 2, 3, 4 };
    assert(v.size() == 4);
    int sum = 0;
    for (auto i: v)
    {
        sum += i;
    }
    assert(10 == sum);
    for (auto i = v.begin(); i != v.end(); ++i)
    {
        sum += *i;
    }
    assert(20 == sum);

    std::vector<A> v2 = { A(), A(3) };
    assert(5 == v2.at(0).getX());
    assert(3 == v2.at(1).getX());
}

// Variadic template

template<class A1>
void variadic1(A1 const& a1)
{
    assert(a1 == 12);
}

template<class A1, class A2>
void variadic1(A1 const& a1, A2 const& a2)
{
    assert(a1 == a2);
}

template<class ...Args>
void variadic(Args... args)
{
    variadic1(args...);
}

template<class A>
bool pairwise_equal(A const& a, A const& b)
{
    return (a == b);
}

template<class T, class ...Rest>
bool pairwise_equal(T const& a, T const& b, Rest... rest)
{
    return pairwise_equal(a, b) && pairwise_equal(rest...);
}

void do_variadic()
{
    variadic(15, 15);
    variadic(12);
    assert(pairwise_equal(5, 5, 2.0, 2.0, std::string("a"), std::string("a")));
    assert(! pairwise_equal(5, 5, 2.0, 3.0));
}

// deleted, default

class B
{
  public:
    B(int x) :
        x(x)
    {
    }
    B() : B(5)
    {
    }
    int getX() const
    {
        return x;
    }

    virtual ~B() = default;
    B(B const&) = delete;
    B& operator=(B const&) = delete;

  private:
    int x;
};

void do_default_deleted()
{
    B b1;
    assert(5 == b1.getX());
    assert(std::is_copy_constructible<A>::value);
    assert(! std::is_copy_constructible<B>::value);
}

// smart pointers

class C
{
  public:
    C(int id = 0) :
        id(id)
    {
        incr(id);
    }
    ~C()
    {
        decr(id);
    }
    C(C const& rhs) : C(rhs.id)
    {
    }
    C& operator=(C const& rhs)
    {
        if (&rhs != this)
        {
            decr(id);
            id = rhs.id;
            incr(id);
        }
        return *this;
    }
    static void check(size_t size, int v, int count)
    {
        assert(m.size() == size);
        auto p = m.find(v);
        if (p != m.end())
        {
            assert(p->second == count);
        }
    }

  private:
    void incr(int i)
    {
        ++m[i];
    }
    void decr(int i)
    {
        if (--m[i] == 0)
        {
            m.erase(i);
        }
    }

    static std::map<int, int> m;
    int id;
};

std::map<int, int> C::m;

std::shared_ptr<C> make_c(int id)
{
    return std::make_shared<C>(id);
}

std::shared_ptr<C> make_c_array(std::vector<int> const& is)
{
    auto p = std::shared_ptr<C>(new C[is.size()], std::default_delete<C[]>());
    C* pp = p.get();
    for (size_t i = 0; i < is.size(); ++i)
    {
        pp[i] = C(is.at(i));
    }
    return p;
}

void do_smart_pointers()
{
    auto p1 = make_c(1);
    C::check(1, 1, 1);
    auto p2 = make_c_array({2, 3, 4, 5});
    for (auto i: {1, 2, 3, 4, 5})
    {
        C::check(5, i, 1);
    }
    {
        C::check(5, 1, 1);
        C c3(*p1);
        C::check(5, 1, 2);
    }
    C::check(5, 1, 1);
    p1 = nullptr;
    C::check(4, 1, 0);
    p2 = nullptr;
    C::check(0, 0, 0);
    {
        std::unique_ptr<C> p3(new C(6));
        C::check(1, 6, 1);
    }
    C::check(0, 0, 0);
}

int main()
{
    do_functional();
    do_inttypes();
    do_iteration();
    do_variadic();
    do_default_deleted();
    do_smart_pointers();
    std::cout << "assertions passed\n";
    return 0;
}
