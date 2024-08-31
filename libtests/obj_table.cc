#include <qpdf/ObjTable.hh>

struct Test
{
    Test() = default;
    Test(int value) :
        value(value)
    {
    }
    int value{0};
};

class Table: public ObjTable<Test>
{
  public:
    Table()
    {
        resize(5);
    }

    void
    test()
    {
        for (int i = 0; i < 10; ++i) {
            (*this)[i].value = 2 * i;
            (*this)[1000 + i].value = 2 * (1000 + i);
        }
        for (int i: {50, 60, 70, 98, 99, 100, 101, 150, 198, 199, 200, 201}) {
            (*this)[i].value = 2 * i;
        }
        resize(100);
        for (int i: {1, 99, 100, 105, 110, 120, 205, 206, 207, 210}) {
            (*this)[i].value = 3 * i;
        }
        resize(200);

        for (int i = 1; i < 10; ++i) {
            emplace_back(i);
        }

        forEach([](auto i, auto const& item) -> void {
            if (item.value) {
                std::cout << std::to_string(i) << " : " << std::to_string(item.value) << "\n";
            }
        });

        std::cout << "2000 : " << std::to_string((*this)[2000].value) << "\n";
    }
};

int
main()
{
    Table().test();

    std::cout << "object table tests done\n";
    return 0;
}
