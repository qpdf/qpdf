/* Copyright (c) 2005-2017 Jay Berkenbilt
 *
 * This file is part of qpdf.  This software may be distributed under
 * the terms of version 2 of the Artistic License which may be found
 * in the source distribution.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __QPDF_FILEINPUTSOURCE_HH__
#define __QPDF_FILEINPUTSOURCE_HH__

#include <qpdf/InputSource.hh>

class FileInputSource: public InputSource
{
  public:
    QPDF_DLL
    FileInputSource();
    QPDF_DLL
    void setFilename(char const* filename);
    QPDF_DLL
    void setFile(char const* description, FILE* filep, bool close_file);
    QPDF_DLL
    virtual ~FileInputSource();
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
    FileInputSource(FileInputSource const&);
    FileInputSource& operator=(FileInputSource const&);

    void destroy();

    bool close_file;
    std::string filename;
    FILE* file;
};

#endif // __QPDF_FILEINPUTSOURCE_HH__
