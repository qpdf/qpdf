// Copyright (c) 2005-2020 Jay Berkenbilt
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

#ifndef QINTC_HH
#define QINTC_HH

#include <qpdf/DLL.h>
#include <qpdf/Types.h>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <sstream>
#include <cassert>
#include <type_traits>

// This namespace provides safe integer conversion that detects
// overflows. It uses short, cryptic names for brevity.

namespace QIntC // QIntC = qpdf Integer Conversion
{
    // to_u is here for backward-compatibility from before we required
    // C++-11.
    template <typename T>
    class to_u
    {
      public:
        typedef typename std::make_unsigned<T>::type type;
    };

    // Basic IntConverter class, which converts an integer from the
    // From class to one of the To class if it can be done safely and
    // throws a range_error otherwise. This class is specialized for
    // each permutation of signed/unsigned for the From and To
    // classes.
    template <typename From, typename To,
              bool From_signed = std::numeric_limits<From>::is_signed,
              bool To_signed = std::numeric_limits<To>::is_signed>
    class IntConverter
    {
    };

    template <typename From, typename To>
    class IntConverter<From, To, false, false>
    {
      public:
        static To convert(From const& i)
        {
            // From and To are both unsigned.
            if (i > std::numeric_limits<To>::max())
            {
                std::ostringstream msg;
                msg << "integer out of range converting " << i
                    << " from a "
                    << sizeof(From) << "-byte unsigned type to a "
                    << sizeof(To) << "-byte unsigned type";
                throw std::range_error(msg.str());
            }
            return static_cast<To>(i);
        }
    };

    template <typename From, typename To>
    class IntConverter<From, To, true, true>
    {
      public:
        static To convert(From const& i)
        {
            // From and To are both signed.
            if ((i < std::numeric_limits<To>::min()) ||
                (i > std::numeric_limits<To>::max()))
            {
                std::ostringstream msg;
                msg << "integer out of range converting " << i
                    << " from a "
                    << sizeof(From) << "-byte signed type to a "
                    << sizeof(To) << "-byte signed type";
                throw std::range_error(msg.str());
            }
            return static_cast<To>(i);
        }
    };

    template <typename From, typename To>
    class IntConverter<From, To, true, false>
    {
      public:
        static To convert(From const& i)
        {
            // From is signed, and To is unsigned. If i > 0, it's safe to
            // convert it to the corresponding unsigned type and to
            // compare with To's max.
            auto ii = static_cast<typename to_u<From>::type>(i);
            if ((i < 0) || (ii > std::numeric_limits<To>::max()))
            {
                std::ostringstream msg;
                msg << "integer out of range converting " << i
                    << " from a "
                    << sizeof(From) << "-byte signed type to a "
                    << sizeof(To) << "-byte unsigned type";
                throw std::range_error(msg.str());
            }
            return static_cast<To>(i);
        }
    };

    template <typename From, typename To>
    class IntConverter<From, To, false, true>
    {
      public:
        static To convert(From const& i)
        {
            // From is unsigned, and to is signed. Convert To's max to the
            // unsigned version of To and compare i against that.
            auto maxval = static_cast<typename to_u<To>::type>(
                std::numeric_limits<To>::max());
            if (i > maxval)
            {
                std::ostringstream msg;
                msg << "integer out of range converting " << i
                    << " from a "
                    << sizeof(From) << "-byte unsigned type to a "
                    << sizeof(To) << "-byte signed type";
                throw std::range_error(msg.str());
            }
            return static_cast<To>(i);
        }
    };

    // Specific converters. The return type of each function must match
    // the second template parameter to IntConverter.
    template <typename T>
    char to_char(T const& i)
    {
        return IntConverter<T, char>::convert(i);
    }

    template <typename T>
    unsigned char to_uchar(T const& i)
    {
        return IntConverter<T, unsigned char>::convert(i);
    }

    template <typename T>
    short to_short(T const& i)
    {
        return IntConverter<T, short>::convert(i);
    }

    template <typename T>
    unsigned short to_ushort(T const& i)
    {
        return IntConverter<T, unsigned short>::convert(i);
    }

    template <typename T>
    int to_int(T const& i)
    {
        return IntConverter<T, int>::convert(i);
    }

    template <typename T>
    unsigned int to_uint(T const& i)
    {
        return IntConverter<T, unsigned int>::convert(i);
    }

    template <typename T>
    size_t to_size(T const& i)
    {
        return IntConverter<T, size_t>::convert(i);
    }

    template <typename T>
    qpdf_offset_t to_offset(T const& i)
    {
        return IntConverter<T, qpdf_offset_t>::convert(i);
    }

    template <typename T>
    long  to_long(T const& i)
    {
        return IntConverter<T, long >::convert(i);
    }

    template <typename T>
    unsigned long  to_ulong(T const& i)
    {
        return IntConverter<T, unsigned long >::convert(i);
    }

    template <typename T>
    long long to_longlong(T const& i)
    {
        return IntConverter<T, long long>::convert(i);
    }

    template <typename T>
    unsigned long long to_ulonglong(T const& i)
    {
        return IntConverter<T, unsigned long long>::convert(i);
    }
};

#endif // QINTC_HH
