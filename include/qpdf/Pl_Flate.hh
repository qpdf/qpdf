// Copyright (c) 2005-2021 Jay Berkenbilt
// Copyright (c) 2022-2025 Jay Berkenbilt and Manfred Holger
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Versions of qpdf prior to version 7 were released under the terms
// of version 2.0 of the Artistic License. At your option, you may
// continue to consider qpdf to be licensed under those terms. Please
// see the manual for additional information.

#ifndef PL_FLATE_HH
#define PL_FLATE_HH

#include <qpdf/Pipeline.hh>
#include <qpdf/QPDFLogger.hh>
#include <functional>
#include <memory>
#include <string>

class QPDF_DLL_CLASS Pl_Flate: public Pipeline
{
  public:
    static unsigned int const def_bufsize = 65536;

    enum action_e { a_inflate, a_deflate };

    QPDF_DLL
    Pl_Flate(
        char const* identifier,
        Pipeline* next,
        action_e action,
        unsigned int out_bufsize = def_bufsize);
    QPDF_DLL
    ~Pl_Flate() override;

    // Limit the memory used.
    // NB This is a static option affecting all Pl_Flate instances.
    QPDF_DLL
    static unsigned long long memory_limit();
    QPDF_DLL
    static void memory_limit(unsigned long long limit);

    QPDF_DLL
    void write(unsigned char const* data, size_t len) override;
    QPDF_DLL
    void finish() override;

    // Globally set compression level from 1 (fastest, least
    // compression) to 9 (slowest, most compression). Use -1 to set
    // the default compression level. This is passed directly to zlib.
    // This method returns a pointer to the current Pl_Flate object so
    // you can create a pipeline with
    // Pl_Flate(...)->setCompressionLevel(...)
    QPDF_DLL
    static void setCompressionLevel(int);

    QPDF_DLL
    void setWarnCallback(std::function<void(char const*, int)> callback);

    // Returns true if qpdf was built with zopfli support.
    QPDF_DLL
    static bool zopfli_supported();

    // Returns true if zopfli is enabled. Zopfli is enabled if QPDF_ZOPFLI is set to a value other
    // than "disabled" and zopfli support is compiled in.
    QPDF_DLL
    static bool zopfli_enabled();

    // If zopfli is supported, returns true. Otherwise, check the QPDF_ZOPFLI
    // environment variable as follows:
    // - "disabled" or "silent": return true
    // - "force": qpdf_exit_error, throw an exception
    // - Any other value: issue a warning, and return false
    QPDF_DLL
    static bool zopfli_check_env(QPDFLogger* logger = nullptr);

  private:
    QPDF_DLL_PRIVATE
    void handleData(unsigned char const* data, size_t len, int flush);
    QPDF_DLL_PRIVATE
    void checkError(char const* prefix, int error_code);
    QPDF_DLL_PRIVATE
    void warn(char const*, int error_code);
    QPDF_DLL_PRIVATE
    void finish_zopfli();

    QPDF_DLL_PRIVATE
    static int compression_level;

    class QPDF_DLL_PRIVATE Members
    {
        friend class Pl_Flate;

      public:
        Members(size_t out_bufsize, action_e action);
        ~Members();

      private:
        Members(Members const&) = delete;

        std::shared_ptr<unsigned char> outbuf;
        size_t out_bufsize;
        action_e action;
        bool initialized;
        void* zdata;
        unsigned long long written{0};
        std::function<void(char const*, int)> callback;
        std::unique_ptr<std::string> zopfli_buf;
    };

    std::unique_ptr<Members> m;
};

#endif // PL_FLATE_HH
