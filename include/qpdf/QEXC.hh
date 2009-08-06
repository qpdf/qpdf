// Copyright (c) 2005-2009 Jay Berkenbilt
//
// This file is part of qpdf.  This software may be distributed under
// the terms of version 2 of the Artistic License which may be found
// in the source distribution.  It is provided "as is" without express
// or implied warranty.

#ifndef __QEXC_HH__
#define __QEXC_HH__

#include <qpdf/DLL.hh>

#include <string>
#include <exception>
#include <errno.h>

namespace QEXC
{
    // This namespace contains all exception classes used by the
    // library.

    // The class hierarchy is as follows:

    //   std::exception
    //   |
    //   +-> QEXC::Base
    //       |
    //       +-> QEXC::General
    //       |
    //       +-> QEXC::Internal

    // QEXC::General is the base class of all standard user-defined
    // exceptions and "expected" error conditions raised by QClass.
    // Applications or libraries using QClass are encouraged to derive
    // their own exceptions from these classes if they wish.  It is
    // entirely reasonable for code to catch QEXC::General or specific
    // subclasses of it as part of normal error handling.

    // QEXC::Internal is reserved for internal errors.  These should
    // be used only for situations that indicate a likely bug in the
    // software itself.  This may include improper use of a library
    // function.  Operator errors should not be able to cause Internal
    // errors.  (There may be some exceptions to this such as users
    // invoking programs that were intended only to be invoked by
    // other programs.)  QEXC::Internal should generally not be
    // trapped except in terminate handlers or top-level exception
    // handlers which will want to translate them into error messages
    // and cause the program to exit.  Such top-level handlers may
    // want to catch std::exception instead.

    // All subclasses of QEXC::Base implement a const unparse() method
    // which returns a std::string const&.  They also override
    // std::exception::what() to return a char* with the same value.
    // unparse() should be implemented in such a way that a program
    // catching QEXC::Base or std::exception can use the text returned
    // by unparse() (or what()) without any exception-specific
    // adornment.  (The program may prefix the program name or other
    // general information.)  Note that std::exception::what() is a
    // const method that returns a const char*.  For this reason, it
    // is essential that unparse() return a const reference to a
    // string so that what() can be implemented by calling unparse().
    // This means that the string that unparse() returns a reference
    // to must not be allocated on the stack in the call to unparse().
    // The recommended way to do this is for derived exception classes
    // to store their string descriptions by calling the protected
    // setMessage() method and then to not override unparse().

    class Base: public std::exception
    {
	// This is the common base class for all exceptions in qclass.
	// Application/library code should not generally catch this
	// directly.  See above for caveats.
      public:
	DLL_EXPORT
	Base();
	DLL_EXPORT
	Base(std::string const& message);
	DLL_EXPORT
	virtual ~Base() throw() {}
	DLL_EXPORT
	virtual std::string const& unparse() const;
	DLL_EXPORT
	virtual const char* what() const throw();

      protected:
	DLL_EXPORT
	void setMessage(std::string const& message);

      private:
	std::string message;
    };

    class General: public Base
    {
	// This is the base class for normal user/library-defined
	// error conditions.
      public:
	DLL_EXPORT
	General();
	DLL_EXPORT
	General(std::string const& message);
	DLL_EXPORT
	virtual ~General() throw() {};
    };

    // Note that Internal is not derived from General.  Internal
    // errors are too severe.  We don't want internal errors
    // accidentally trapped as part of QEXC::General.  If you are
    // going to deal with internal errors, you have to do so
    // explicitly.
    class Internal: public Base
    {
      public:
	DLL_EXPORT
	Internal(std::string const& message);
	DLL_EXPORT
	virtual ~Internal() throw() {};
    };

    class System: public General
    {
      public:
	DLL_EXPORT
	System(std::string const& prefix, int sys_errno);
	DLL_EXPORT
	virtual ~System() throw() {};
	DLL_EXPORT
	int getErrno() const;

      private:
	int sys_errno;
    };
};

#endif // __QEXC_HH__
