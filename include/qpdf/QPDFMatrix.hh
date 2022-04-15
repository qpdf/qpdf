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

#ifndef QPDFMATRIX_HH
#define QPDFMATRIX_HH

#include <qpdf/DLL.h>
#include <qpdf/QPDFObjectHandle.hh>
#include <string>

// This class represents a PDF transformation matrix using a tuple
// such that
//
//                      ┌       ┐
//                      │ a b 0 │
// (a, b, c, d, e, f) = │ c d 0 │
//                      │ e f 1 │
//                      └       ┘

class QPDFMatrix
{
  public:
    QPDF_DLL
    QPDFMatrix();
    QPDF_DLL
    QPDFMatrix(double a, double b, double c, double d, double e, double f);
    QPDF_DLL
    QPDFMatrix(QPDFObjectHandle::Matrix const&);

    // Returns the six values separated by spaces as real numbers with
    // trimmed zeroes.
    QPDF_DLL
    std::string unparse() const;

    QPDF_DLL
    QPDFObjectHandle::Matrix getAsMatrix() const;

    // Replace this with other * this
    QPDF_DLL
    void concat(QPDFMatrix const& other);

    // Same as concat(sx, 0, 0, sy, 0, 0)
    QPDF_DLL
    void scale(double sx, double sy);

    // Same as concat(1, 0, 0, 1, tx, ty);
    QPDF_DLL
    void translate(double tx, double ty);

    // Any value other than 90, 180, or 270 is ignored
    QPDF_DLL
    void rotatex90(int angle);

    // Transform a point. The underlying operation is to take
    // [x y 1] * this
    // and take the first and second rows of the result as xp and yp.
    QPDF_DLL
    void transform(double x, double y, double& xp, double& yp) const;

    // Transform a rectangle by creating a new rectangle that tightly
    // bounds the polygon resulting from transforming the four
    // corners.
    QPDF_DLL
    QPDFObjectHandle::Rectangle
    transformRectangle(QPDFObjectHandle::Rectangle r) const;

    // operator== tests for exact equality, not considering deltas for
    // floating point.
    QPDF_DLL
    bool operator==(QPDFMatrix const& rhs) const;
    QPDF_DLL
    bool
    operator!=(QPDFMatrix const& rhs) const
    {
        return !operator==(rhs);
    }

    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
};

#endif // QPDFMATRIX_HH
