QPDF is copyright (c) 2005-2020 Jay Berkenbilt

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

Versions of qpdf prior to version 7 were released under the terms of version 2.0 of the Artistic License. At your option, you may continue to consider qpdf to be licensed under those terms. Please see the manual for additional information.

The qpdf distribution includes a copy of [qtest](http://qtest.qbilt.org), which is released under the terms of the [version 2.0 of the Artistic license](https://opensource.org/licenses/Artistic-2.0), which can be found at https://opensource.org/licenses/Artistic-2.0.

The Rijndael encryption implementation used as the basis for AES encryption and decryption support comes from Philip J. Erdelsky's public domain implementation.  The files `libqpdf/rijndael.cc` and `libqpdf/qpdf/rijndael.h` remain in the public domain.  They were obtained from
* http://www.efgh.com/software/rijndael.htm
* http://www.efgh.com/software/rijndael.txt

The embedded sha2 code comes from sphlib 3.0
* http://www.saphir2.com/sphlib/

That code has the following license:
  ```
  Copyright (c) 2007-2011  Projet RNRT SAPHIR

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  ```
