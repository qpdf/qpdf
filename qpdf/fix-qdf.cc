#include <qpdf/QIntC.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFXRefEntry.hh>
#include <qpdf/QUtil.hh>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <regex>
#include <string_view>

static char const* whoami = nullptr;

static void
usage()
{
    std::cerr << "Usage: " << whoami << " [filename]" << std::endl;
    exit(2);
}

class QdfFixer
{
  public:
    QdfFixer(std::string const& filename);
    void processLines(std::string const& input);

  private:
    void fatal(std::string const&);
    void checkObjId(std::string const& obj_id);
    void adjustOstreamXref();
    void writeOstream();
    void writeBinary(unsigned long long val, size_t bytes);

    std::string filename;
    enum {
        st_top,
        st_in_obj,
        st_in_stream,
        st_after_stream,
        st_in_ostream_dict,
        st_in_ostream_offsets,
        st_in_ostream_outer,
        st_in_ostream_obj,
        st_in_xref_stream_dict,
        st_in_length,
        st_at_xref,
        st_before_trailer,
        st_in_trailer,
        st_done,
    } state;

    size_t lineno;
    qpdf_offset_t offset;
    qpdf_offset_t last_offset;
    int last_obj;
    std::vector<QPDFXRefEntry> xref;
    qpdf_offset_t stream_start;
    size_t stream_length;
    qpdf_offset_t xref_offset;
    size_t xref_f1_nbytes;
    size_t xref_f2_nbytes;
    size_t xref_size;
    std::vector<std::string_view> ostream;
    std::vector<qpdf_offset_t> ostream_offsets;
    std::vector<std::string_view> ostream_discarded;
    size_t ostream_idx;
    int ostream_id;
    std::string ostream_extends;
};

QdfFixer::QdfFixer(std::string const& filename) :
    filename(filename),
    state(st_top),
    lineno(0),
    offset(0),
    last_offset(0),
    last_obj(0),
    stream_start(0),
    stream_length(0),
    xref_offset(0),
    xref_f1_nbytes(0),
    xref_f2_nbytes(0),
    xref_size(0),
    ostream_idx(0),
    ostream_id(0)
{
}

void
QdfFixer::fatal(std::string const& msg)
{
    std::cerr << msg << std::endl;
    exit(2);
}

void
QdfFixer::processLines(std::string const& input)
{
    using namespace std::literals;

    static const std::regex re_n_0_obj("^(\\d+) 0 obj\n$");
    static const std::regex re_extends("/Extends (\\d+ 0 R)");
    static const std::regex re_ostream_obj("^%% Object stream: object (\\d+)");
    static const std::regex re_num("^\\d+\n$");
    static const std::regex re_size_n("^  /Size \\d+\n$");

    auto sv_diff = [](size_t i) { return static_cast<std::string_view::difference_type>(i); };

    lineno = 0;
    bool more = true;
    auto len_line = sv_diff(0);

    std::string_view line;
    std::string_view input_view{input.data(), input.size()};
    size_t offs = 0;

    auto b_line = input.cbegin();
    std::smatch m;
    auto const matches = [&m, &b_line, &len_line](std::regex const& r) {
        return std::regex_search(b_line, b_line + len_line, m, r);
    };

    while (more) {
        ++lineno;
        last_offset = offset;
        b_line += len_line;

        offs = input_view.find('\n');
        if (offs == std::string::npos) {
            more = false;
            line = input_view;
        } else {
            offs++;
            line = input_view.substr(0, offs);
            input_view.remove_prefix(offs);
        }
        len_line = sv_diff(line.size());
        offset += len_line;

        if (state == st_top) {
            if (matches(re_n_0_obj)) {
                checkObjId(m[1].str());
                state = st_in_obj;
            } else if (line.compare("xref\n"sv) == 0) {
                xref_offset = last_offset;
                state = st_at_xref;
            }
            std::cout << line;
        } else if (state == st_in_obj) {
            std::cout << line;
            if (line.compare("stream\n"sv) == 0) {
                state = st_in_stream;
                stream_start = offset;
            } else if (line.compare("endobj\n"sv) == 0) {
                state = st_top;
            } else if (line.find("/Type /ObjStm"sv) != line.npos) {
                state = st_in_ostream_dict;
                ostream_id = last_obj;
            } else if (line.find("/Type /XRef"sv) != line.npos) {
                xref_offset = xref.back().getOffset();
                xref_f1_nbytes = 0;
                auto t = xref_offset;
                while (t) {
                    t >>= 8;
                    ++xref_f1_nbytes;
                }
                // Figure out how many bytes we need for ostream
                // index. Make sure we get at least 1 byte even if
                // there are no object streams.
                int max_objects = 1;
                for (auto const& e: xref) {
                    if ((e.getType() == 2) && (e.getObjStreamIndex() > max_objects)) {
                        max_objects = e.getObjStreamIndex();
                    }
                }
                while (max_objects) {
                    max_objects >>= 8;
                    ++xref_f2_nbytes;
                }
                auto esize = 1 + xref_f1_nbytes + xref_f2_nbytes;
                xref_size = 1 + xref.size();
                auto length = xref_size * esize;
                std::cout << "  /Length " << length << "\n"
                          << "  /W [ 1 " << xref_f1_nbytes << " " << xref_f2_nbytes << " ]"
                          << "\n";
                state = st_in_xref_stream_dict;
            }
        } else if (state == st_in_ostream_dict) {
            if (line.compare("stream\n"sv) == 0) {
                state = st_in_ostream_offsets;
            } else {
                ostream_discarded.push_back(line);
                if (matches(re_extends)) {
                    ostream_extends = m[1].str();
                }
            }
            // discard line
        } else if (state == st_in_ostream_offsets) {
            if (matches(re_ostream_obj)) {
                checkObjId(m[1].str());
                stream_start = last_offset;
                state = st_in_ostream_outer;
                ostream.push_back(line);
            } else {
                ostream_discarded.push_back(line);
            }
            // discard line
        } else if (state == st_in_ostream_outer) {
            adjustOstreamXref();
            ostream_offsets.push_back(last_offset - stream_start);
            state = st_in_ostream_obj;
            ostream.push_back(line);
        } else if (state == st_in_ostream_obj) {
            ostream.push_back(line);
            if (matches(re_ostream_obj)) {
                checkObjId(m[1].str());
                state = st_in_ostream_outer;
            } else if (line.compare("endstream\n"sv) == 0) {
                stream_length = QIntC::to_size(last_offset - stream_start);
                writeOstream();
                state = st_in_obj;
            }
        } else if (state == st_in_xref_stream_dict) {
            if ((line.find("/Length"sv) != line.npos) || (line.find("/W"sv) != line.npos)) {
                // already printed
            } else if (line.find("/Size"sv) != line.npos) {
                auto xref_size = 1 + xref.size();
                std::cout << "  /Size " << xref_size << "\n";
            } else {
                std::cout << line;
            }
            if (line.compare("stream\n"sv) == 0) {
                writeBinary(0, 1);
                writeBinary(0, xref_f1_nbytes);
                writeBinary(0, xref_f2_nbytes);
                for (auto const& x: xref) {
                    unsigned long long f1 = 0;
                    unsigned long long f2 = 0;
                    unsigned int type = QIntC::to_uint(x.getType());
                    if (1 == type) {
                        f1 = QIntC::to_ulonglong(x.getOffset());
                    } else {
                        f1 = QIntC::to_ulonglong(x.getObjStreamNumber());
                        f2 = QIntC::to_ulonglong(x.getObjStreamIndex());
                    }
                    writeBinary(type, 1);
                    writeBinary(f1, xref_f1_nbytes);
                    writeBinary(f2, xref_f2_nbytes);
                }
                std::cout << "\nendstream\nendobj\n\n"
                          << "startxref\n"
                          << xref_offset << "\n%%EOF\n";
                state = st_done;
            }
        } else if (state == st_in_stream) {
            if (line.compare("endstream\n"sv) == 0) {
                stream_length = QIntC::to_size(last_offset - stream_start);
                state = st_after_stream;
            }
            std::cout << line;
        } else if (state == st_after_stream) {
            if (line.compare("%QDF: ignore_newline\n"sv) == 0) {
                if (stream_length > 0) {
                    --stream_length;
                }
            } else if (matches(re_n_0_obj)) {
                checkObjId(m[1].str());
                state = st_in_length;
            }
            std::cout << line;
        } else if (state == st_in_length) {
            if (!matches(re_num)) {
                fatal(filename + ":" + std::to_string(lineno) + ": expected integer");
            }
            std::string new_length = std::to_string(stream_length) + "\n";
            offset -= QIntC::to_offset(line.length());
            offset += QIntC::to_offset(new_length.length());
            std::cout << new_length;
            state = st_top;
        } else if (state == st_at_xref) {
            auto n = xref.size();
            std::cout << "0 " << 1 + n << "\n0000000000 65535 f \n";
            for (auto const& e: xref) {
                std::cout << QUtil::int_to_string(e.getOffset(), 10) << " 00000 n \n";
            }
            state = st_before_trailer;
        } else if (state == st_before_trailer) {
            if (line.compare("trailer <<\n"sv) == 0) {
                std::cout << line;
                state = st_in_trailer;
            }
            // no output
        } else if (state == st_in_trailer) {
            if (matches(re_size_n)) {
                std::cout << "  /Size " << 1 + xref.size() << "\n";
            } else {
                std::cout << line;
            }
            if (line.compare(">>\n"sv) == 0) {
                std::cout << "startxref\n" << xref_offset << "\n%%EOF\n";
                state = st_done;
            }
        } else if (state == st_done) {
            // ignore
        }
    }
}

void
QdfFixer::checkObjId(std::string const& cur_obj_str)
{
    if (std::stoi(cur_obj_str) != ++last_obj) {
        fatal(
            filename + ":" + std::to_string(lineno) + ": expected object " +
            std::to_string(last_obj));
    }
    xref.push_back(QPDFXRefEntry(1, last_offset, 0));
}

void
QdfFixer::adjustOstreamXref()
{
    xref.back() = QPDFXRefEntry(2, ostream_id, QIntC::to_int(ostream_idx++));
}

void
QdfFixer::writeOstream()
{
    auto first = ostream_offsets.at(0);
    auto onum = ostream_id;
    std::string offsets;
    auto n = ostream_offsets.size();
    for (auto iter: ostream_offsets) {
        iter -= QIntC::to_offset(first);
        ++onum;
        offsets += std::to_string(onum) + " " + std::to_string(iter) + "\n";
    }
    auto offset_adjust = QIntC::to_offset(offsets.size());
    first += offset_adjust;
    stream_length += QIntC::to_size(offset_adjust);
    std::string dict_data = "";
    dict_data += "  /Length " + std::to_string(stream_length) + "\n";
    dict_data += "  /N " + std::to_string(n) + "\n";
    dict_data += "  /First " + std::to_string(first) + "\n";
    if (!ostream_extends.empty()) {
        dict_data += "  /Extends " + ostream_extends + "\n";
    }
    dict_data += ">>\n";
    offset_adjust += QIntC::to_offset(dict_data.length());
    std::cout << dict_data << "stream\n" << offsets;
    for (auto const& o: ostream) {
        std::cout << o;
    }

    for (auto const& o: ostream_discarded) {
        offset -= QIntC::to_offset(o.length());
    }
    offset += offset_adjust;

    ostream_idx = 0;
    ostream_id = 0;
    ostream.clear();
    ostream_offsets.clear();
    ostream_discarded.clear();
    ostream_extends.clear();
}

void
QdfFixer::writeBinary(unsigned long long val, size_t bytes)
{
    if (bytes > sizeof(unsigned long long)) {
        throw std::logic_error("fix-qdf::writeBinary called with too many bytes");
    }
    std::string data(bytes, '\0');
    for (auto i = bytes; i > 0; --i) {
        data[i - 1] = static_cast<char>(val & 0xff); // i.e. val % 256
        val >>= 8;                                   // i.e. val = val / 256
    }
    std::cout << data;
}

static int
realmain(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);
    QUtil::setLineBuf(stdout);
    char const* filename = nullptr;
    if (argc > 2) {
        usage();
    } else if ((argc > 1) && (strcmp(argv[1], "--version") == 0)) {
        std::cout << whoami << " from qpdf version " << QPDF::QPDFVersion() << std::endl;
        return 0;
    } else if ((argc > 1) && (strcmp(argv[1], "--help") == 0)) {
        usage();
    } else if (argc == 2) {
        filename = argv[1];
    }
    std::string input;
    if (filename == nullptr) {
        filename = "standard input";
        QUtil::binary_stdin();
        input = QUtil::read_file_into_string(stdin);
    } else {
        input = QUtil::read_file_into_string(filename);
    }
    QUtil::binary_stdout();
    QdfFixer qf(filename);
    qf.processLines(input);
    return 0;
}

#ifdef WINDOWS_WMAIN

extern "C" int
wmain(int argc, wchar_t* argv[])
{
    return QUtil::call_main_from_wmain(argc, argv, realmain);
}

#else

int
main(int argc, char* argv[])
{
    return realmain(argc, argv);
}

#endif
