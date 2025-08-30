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

#ifndef QPDFCRYPTOIMPL_HH
#define QPDFCRYPTOIMPL_HH

#include <qpdf/DLL.h>
#include <string>

// This class is part of qpdf's pluggable crypto provider support.  Most users won't need to know or
// care about this class, but you can use it if you want to supply your own crypto implementation.
// To do so, provide an implementation of QPDFCryptoImpl, ensure that you register it by calling
// QPDFCryptoProvider::registerImpl, and make it the default by calling
// QPDFCryptoProvider::setDefaultProvider.
class QPDF_DLL_CLASS QPDFCryptoImpl
{
  public:
    QPDFCryptoImpl() = default;

    virtual ~QPDFCryptoImpl() = default;

    // Random Number Generation

    virtual void provideRandomData(unsigned char* data, size_t len) = 0;

    // Hashing

    typedef unsigned char MD5_Digest[16];
    virtual void MD5_init() = 0;
    virtual void MD5_update(unsigned char const* data, size_t len) = 0;
    virtual void MD5_finalize() = 0;
    virtual void MD5_digest(MD5_Digest) = 0;

    virtual void SHA2_init(int bits) = 0;
    virtual void SHA2_update(unsigned char const* data, size_t len) = 0;
    virtual void SHA2_finalize() = 0;
    virtual std::string SHA2_digest() = 0;

    // Encryption/Decryption

    // qpdf must support RC4 to be able to work with older PDF files and readers. Search for RC4 in
    // README.md

    // key_len of -1 means treat key_data as a null-terminated string
    virtual void RC4_init(unsigned char const* key_data, int key_len = -1) = 0;
    // out_data = 0 means to encrypt/decrypt in place
    virtual void
    RC4_process(unsigned char const* in_data, size_t len, unsigned char* out_data = nullptr) = 0;
    virtual void RC4_finalize() = 0;

    static size_t constexpr rijndael_buf_size = 16;
    virtual void rijndael_init(
        bool encrypt,
        unsigned char const* key_data,
        size_t key_len,
        bool cbc_mode,
        unsigned char* cbc_block) = 0;
    virtual void rijndael_process(unsigned char* in_data, unsigned char* out_data) = 0;
    virtual void rijndael_finalize() = 0;

    // Convenience functions to support chars and strings
    void
    rijndael_init(bool encrypt, std::string_view key, bool cbc_mode, std::string_view cbc_block)
    {
        rijndael_init(
            encrypt,
            reinterpret_cast<const unsigned char*>(key.data()),
            key.size(),
            cbc_mode,
            reinterpret_cast<unsigned char*>(const_cast<char*>(cbc_block.data())));
    }
    void
    rijndael_process(const char* in_data, char* out_data)
    {
        rijndael_process(
            reinterpret_cast<unsigned char*>(const_cast<char*>(in_data)),
            reinterpret_cast<unsigned char*>(out_data));
    }
};

#endif // QPDFCRYPTOIMPL_HH
