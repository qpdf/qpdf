/* Copyright (c) 2005-2015 Jay Berkenbilt
 *
 * This file is part of qpdf.  This software may be distributed under
 * the terms of version 2 of the Artistic License which may be found
 * in the source distribution.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __RANDOMDATAPROVIDER_HH__
#define __RANDOMDATAPROVIDER_HH__

#include <string.h> // for size_t

class RandomDataProvider
{
  public:
    virtual ~RandomDataProvider()
    {
    }
    virtual void provideRandomData(unsigned char* data, size_t len) = 0;

  protected:
    RandomDataProvider()
    {
    }

  private:
    RandomDataProvider(RandomDataProvider const&);
    RandomDataProvider& operator=(RandomDataProvider const&);
};

#endif // __RANDOMDATAPROVIDER_HH__
