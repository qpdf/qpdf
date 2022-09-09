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

#ifndef QPDFLOGGER_HH
#define QPDFLOGGER_HH

#include <qpdf/DLL.h>
#include <qpdf/Pipeline.hh>
#include <iostream>
#include <memory>

class QPDFLogger
{
  public:
    QPDF_DLL
    static std::shared_ptr<QPDFLogger> create();

    // Return the default logger. In general, you should use the
    // default logger. You can also create your own loggers and use
    // them QPDF and QPDFJob objects, but there are few reasons to do
    // so. One reason may if you are using multiple QPDF or QPDFJob
    // objects in different threads and want to capture output and
    // errors to different streams. (Note that a single QPDF or
    // QPDFJob can't be safely used from multiple threads, but it is
    // safe to use separate QPDF and QPDFJob objects on separate
    // threads.) Another possible reason would be if you are writing
    // an application that uses the qpdf library directly and qpdf is
    // also used by a downstream library or if you are using qpdf from
    // a library and don't want to interfere with potential uses of
    // qpdf by other libraries or applications.
    QPDF_DLL
    static std::shared_ptr<QPDFLogger> defaultLogger();

    // Defaults:
    //
    // info -- if save is standard output, standard error, else standard output
    // warn -- whatever error points to
    // error -- standard error
    // save -- undefined unless set
    //
    // "info" is used for diagnostic messages, verbose messages, and
    // progress messages. "warn" is used for warnings. "error" is used
    // for errors. "save" is used for saving output -- see below.
    //
    // On deletion, finish() is called for the standard output and
    // standard error pipelines, which flushes output. If you supply
    // any custom pipelines, you must call finish() on them yourself.
    // Note that calling finish is not needed for string, stdio, or
    // ostream pipelines.
    //
    // NOTES ABOUT THE SAVE PIPELINE
    //
    // The save pipeline is used by QPDFJob when some kind of binary
    // output is being saved. This includes saving attachments and
    // stream data and also includes when the output file is standard
    // output. If you want to grab that output, you can call setSave.
    // See examples/qpdfjob-save-attachment.cc and
    // examples/qpdfjob-c-save-attachment.c.
    //
    // You should never set the save pipeline to the same destination
    // as something else. Doing so will corrupt your save output. If
    // you want to save to standard output, use the method
    // saveToStandardOutput(). In addition to setting the save
    // pipeline, that does the following extra things:
    //
    // * If standard output has been used, a logic error is thrown
    // * If info is set to standard output at the time of the set save
    //   call, it is switched to standard error.
    //
    // This is not a guarantee. You can still mess this up in ways
    // that are not checked. Here are a few examples:
    //
    // * Don't set any pipeline to standard output *after* passing it
    //   to setSave()
    // * Don't use a separate mechanism to write stdout/stderr other
    //   than QPDFLogger::standardOutput()
    // * Don't set anything to the same custom pipeline that save is
    //   set to.
    //
    // Just be sure that if you change pipelines around, you should
    // avoid having the save pipeline also be used for any other
    // purpose. The special case for saving to standard output allows
    // you to call saveToStandardOutput() early without having to
    // worry about the info pipeline.

    QPDF_DLL
    void info(char const*);
    QPDF_DLL
    void info(std::string const&);
    QPDF_DLL
    std::shared_ptr<Pipeline> getInfo(bool null_okay = false);

    QPDF_DLL
    void warn(char const*);
    QPDF_DLL
    void warn(std::string const&);
    QPDF_DLL
    std::shared_ptr<Pipeline> getWarn(bool null_okay = false);

    QPDF_DLL
    void error(char const*);
    QPDF_DLL
    void error(std::string const&);
    QPDF_DLL
    std::shared_ptr<Pipeline> getError(bool null_okay = false);

    QPDF_DLL
    std::shared_ptr<Pipeline> getSave(bool null_okay = false);

    QPDF_DLL
    std::shared_ptr<Pipeline> standardOutput();
    QPDF_DLL
    std::shared_ptr<Pipeline> standardError();
    QPDF_DLL
    std::shared_ptr<Pipeline> discard();

    // Passing a null pointer resets to default
    QPDF_DLL
    void setInfo(std::shared_ptr<Pipeline>);
    QPDF_DLL
    void setWarn(std::shared_ptr<Pipeline>);
    QPDF_DLL
    void setError(std::shared_ptr<Pipeline>);
    // See notes above about the save pipeline
    QPDF_DLL
    void setSave(std::shared_ptr<Pipeline>, bool only_if_not_set);
    QPDF_DLL
    void saveToStandardOutput(bool only_if_not_set);

    // Shortcut for logic to reset output to new output/error streams.
    // out_stream is used for info, err_stream is used for error, and
    // warning is cleared so that it follows error.
    QPDF_DLL
    void setOutputStreams(std::ostream* out_stream, std::ostream* err_stream);

  private:
    QPDFLogger();
    std::shared_ptr<Pipeline>
    throwIfNull(std::shared_ptr<Pipeline>, bool null_okay);

    class Members
    {
        friend class QPDFLogger;

      public:
        QPDF_DLL
        ~Members();

      private:
        Members();
        Members(Members const&) = delete;

        std::shared_ptr<Pipeline> p_discard;
        std::shared_ptr<Pipeline> p_real_stdout;
        std::shared_ptr<Pipeline> p_stdout;
        std::shared_ptr<Pipeline> p_stderr;
        std::shared_ptr<Pipeline> p_info;
        std::shared_ptr<Pipeline> p_warn;
        std::shared_ptr<Pipeline> p_error;
        std::shared_ptr<Pipeline> p_save;
    };
    std::shared_ptr<Members> m;
};

#endif // QPDFLOGGER_HH
