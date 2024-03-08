#include <qpdf/ObjTable.hh>

struct Test
{
    int value{0};
};

class Table: public ObjTable<Test>
{
  public:
    Table()
    {
        initialize(5);
    }

    void
    test()
    {
        for (int i = 0; i < 10; ++i) {
            (*this)[i].value = 2 * i;
            (*this)[1000 + i].value = 2 * (1000 + i);
        }

        forEach([](auto i, auto const& item) -> void {
            std::cout << std::to_string(i) << " : " << std::to_string(item.value) << "\n";
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
