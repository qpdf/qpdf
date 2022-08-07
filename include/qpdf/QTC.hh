// Copyright (c) 2005-2022 Jay Berkenbilt
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

#ifndef QTC_HH
#define QTC_HH

#include <qpdf/DLL.h>

// Defining QPDF_DISABLE_QTC will effectively compile out any QTC::TC
// calls in any code that includes this file, but QTC will still be
// built into the library. That way, it is possible to build and
// package qpdf with QPDF_DISABLE_QTC while still making QTC::TC
// available to end users.

namespace QTC
{
    QPDF_DLL
    void TC_real(char const* const scope, char const* const ccase, int n = 0);

    inline void
    TC(char const* const scope, char const* const ccase, int n = 0)
    {
#ifndef QPDF_DISABLE_QTC
        TC_real(scope, ccase, n);
#endif // QPDF_DISABLE_QTC
    }
}; // namespace QTC

#endif // QTC_HH
