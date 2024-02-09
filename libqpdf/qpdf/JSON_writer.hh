#ifndef JSON_WRITER_HH
#define JSON_WRITER_HH

#include <qpdf/JSON.hh>
#include <qpdf/Pipeline.hh>

#include <string_view>

// Writer is a small utility class to aid writing JSON to a pipeline. Methods are designed to allow
// chaining of calls.
//
// Some uses of the class have a significant performance impact. The class is intended purely for
// internal use to allow it to be adapted as needed to maintain performance.
class JSON::Writer
{
  public:
    Writer(Pipeline* p, size_t depth) :
        p(p),
        indent(2 * depth)
    {
    }

    Writer&
    write(char const* data, size_t len)
    {
        p->write(reinterpret_cast<unsigned char const*>(data), len);
        return *this;
    }

    Writer&
    writeNext()
    {
        auto n = indent;
        if (first) {
            first = false;
            write(&spaces[1], n % n_spaces + 1);
        } else {
            write(&spaces[0], n % n_spaces + 2);
        }
        while (n >= n_spaces) {
            write(&spaces[2], n_spaces);
            n -= n_spaces;
        }
        return *this;
    }

    Writer&
    writeStart(char const& c)
    {
        write(&c, 1);
        first = true;
        indent += 2;
        return *this;
    }

    Writer&
    writeEnd(char const& c)
    {
        if (indent > 1) {
            indent -= 2;
        }
        if (!first) {
            first = true;
            writeNext();
        }
        first = false;
        write(&c, 1);
        return *this;
    }

    Writer&
    operator<<(std::string_view sv)
    {
        p->write(reinterpret_cast<unsigned char const*>(sv.data()), sv.size());
        return *this;
    }

    Writer&
    operator<<(char const* s)
    {
        *this << std::string_view{s};
        return *this;
    }

    Writer&
    operator<<(bool val)
    {
        *this << (val ? "true" : "false");
        return *this;
    }

    Writer&
    operator<<(int val)
    {
        *this << std::to_string(val);
        return *this;
    }

    Writer&
    operator<<(size_t val)
    {
        *this << std::to_string(val);
        return *this;
    }

    Writer&
    operator<<(JSON&& j)
    {
        j.write(p, indent / 2);
        return *this;
    }

    static std::string encode_string(std::string const& utf8);

  private:
    Pipeline* p;
    bool first{true};
    size_t indent;

    static constexpr std::string_view spaces =
        ",\n                                                  ";
    static constexpr auto n_spaces = spaces.size() - 2;
};

#endif // JSON_WRITER_HH
