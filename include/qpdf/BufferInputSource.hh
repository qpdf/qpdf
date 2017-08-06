/* Copyright (c) 2005-2015 Jay Berkenbilt
 *
 * This file is part of qpdf.  This software may be distributed under
 * the terms of version 2 of the Artistic License which may be found
 * in the source distribution.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __QPDF_BUFFERINPUTSOURCE_HH__
#define __QPDF_BUFFERINPUTSOURCE_HH__

#include <qpdf/InputSource.hh>
#include <qpdf/Buffer.hh>

class BufferInputSource: public InputSource
{
  public:
    QPDF_DLL
    BufferInputSource(std::string const& description, Buffer* buf,
                      bool own_memory = false);
    QPDF_DLL
    BufferInputSource(std::string const& description,
                      std::string const& contents);
    QPDF_DLL
    virtual ~BufferInputSource();
    QPDF_DLL
    virtual qpdf_offset_t findAndSkipNextEOL();
    QPDF_DLL
    virtual std::string const& getName() const;
    QPDF_DLL
    virtual qpdf_offset_t tell();
    QPDF_DLL
    virtual void seek(qpdf_offset_t offset, int whence);
    QPDF_DLL
    virtual void rewind();
    QPDF_DLL
    virtual size_t read(char* buffer, size_t length);
    QPDF_DLL
    virtual void unreadCh(char ch);

  private:
    bool own_memory;
    std::string description;
    Buffer* buf;
    qpdf_offset_t cur_offset;
};

#endif // __QPDF_BUFFERINPUTSOURCE_HH__
