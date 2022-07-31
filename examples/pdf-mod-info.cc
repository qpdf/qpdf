// Author: Vitaliy Pavlyuk

#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFWriter.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char const* version = "1.1";
static char const* whoami = nullptr;

void
usage()
{
    std::cerr << "Usage: " << whoami
              << " --in in_file [--out out_file] [--key key [--val val]?]+\n"
              << "Modifies/Adds/Removes PDF /Info entries in the in_file\n"
              << "and stores the result in out_file\n"
              << "Special mode: " << whoami << " --dump file\n"
              << "dumps all /Info entries to stdout\n";
    exit(2);
}

void
dumpInfoDict(
    QPDF& pdf, std::ostream& os = std::cout, std::string const& sep = ":\t")
{
    QPDFObjectHandle trailer = pdf.getTrailer();
    if (trailer.hasKey("/Info")) {
        for (auto& it: trailer.getKey("/Info").ditems()) {
            std::string val;
            if (it.second.isString()) {
                val = it.second.getStringValue();
            } else if (it.second.isName()) {
                val = it.second.getName();
            } else // according to PDF Spec 1.5, shouldn't happen
            {
                val = it.second.unparseResolved();
            }
            os << it.first.substr(1) << sep << val << std::endl; // skip '/'
        }
    }
}

void
pdfDumpInfoDict(char const* fname)
{
    try {
        QPDF pdf;
        pdf.processFile(fname);
        dumpInfoDict(pdf);
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(2);
    }
}

int
main(int argc, char* argv[])
{
    bool static_id = false;
    std::map<std::string, std::string> Keys;

    whoami = QUtil::getWhoami(argv[0]);

    if ((argc == 2) && (!strcmp(argv[1], "--version"))) {
        std::cout << whoami << " version " << version << std::endl;
        exit(0);
    }
    if ((argc == 3) && (!strcmp(argv[1], "--dump"))) {
        QTC::TC("examples", "pdf-mod-info --dump");
        pdfDumpInfoDict(argv[2]);
        exit(0);
    }

    char* fl_in = nullptr;
    char* fl_out = nullptr;
    std::string cur_key;

    for (int i = 1; i < argc; ++i) {
        if ((!strcmp(argv[i], "--in")) && (++i < argc)) {
            fl_in = argv[i];
        } else if ((!strcmp(argv[i], "--out")) && (++i < argc)) {
            fl_out = argv[i];
        } else if (!strcmp(argv[i], "--static-id")) // don't document
        {
            static_id = true; // this should be used in test suites only
        } else if ((!strcmp(argv[i], "--key")) && (++i < argc)) {
            QTC::TC("examples", "pdf-mod-info -key");
            cur_key = argv[i];
            if (!((cur_key.length() > 0) && (cur_key.at(0) == '/'))) {
                cur_key = "/" + cur_key;
            }
            Keys[cur_key] = "";
        } else if ((!strcmp(argv[i], "--val")) && (++i < argc)) {
            if (cur_key.empty()) {
                QTC::TC("examples", "pdf-mod-info usage wrong val");
                usage();
            }
            QTC::TC("examples", "pdf-mod-info -val");
            Keys[cur_key] = argv[i];
            cur_key.clear();
        } else {
            QTC::TC("examples", "pdf-mod-info usage junk");
            usage();
        }
    }
    if (!fl_in) {
        QTC::TC("examples", "pdf-mod-info no in file");
        usage();
    }
    if (!fl_out) {
        QTC::TC("examples", "pdf-mod-info in-place");
        fl_out = fl_in;
    }
    if (Keys.size() == 0) {
        QTC::TC("examples", "pdf-mod-info no keys");
        usage();
    }

    std::string fl_tmp = fl_out;
    fl_tmp += ".tmp";

    try {
        QPDF file;
        file.processFile(fl_in);

        QPDFObjectHandle filetrailer = file.getTrailer();
        QPDFObjectHandle fileinfo;

        for (auto const& it: Keys) {
            if (!fileinfo.isInitialized()) {
                if (filetrailer.hasKey("/Info")) {
                    QTC::TC("examples", "pdf-mod-info has info");
                    fileinfo = filetrailer.getKey("/Info");
                } else {
                    QTC::TC("examples", "pdf-mod-info file no info");
                    fileinfo = QPDFObjectHandle::newDictionary();
                    filetrailer.replaceKey("/Info", fileinfo);
                }
            }
            if (it.second == "") {
                fileinfo.removeKey(it.first);
            } else {
                QPDFObjectHandle elt = fileinfo.newString(it.second);
                elt.makeDirect();
                fileinfo.replaceKey(it.first, elt);
            }
        }
        QPDFWriter w(file, fl_tmp.c_str());
        w.setStreamDataMode(qpdf_s_preserve);
        w.setLinearization(true);
        w.setStaticID(static_id); // for testing only
        w.write();
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(2);
    }

    try {
        (void)remove(fl_out);
        QUtil::os_wrapper(
            "rename " + fl_tmp + " " + std::string(fl_out),
            rename(fl_tmp.c_str(), fl_out));
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        exit(2);
    }

    return 0;
}
