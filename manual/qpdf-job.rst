
.. _qpdf-job:

QPDFJob: a Job-Based Interface
==============================

All of the functionality from the :command:`qpdf` command-line
executable is available from inside the C++ library using the
``QPDFJob`` class. There are several ways to access this functionality:

- Command-line options

  - Run the :command:`qpdf` command line

  - Use from the C++ API with ``QPDFJob::initializeFromArgv``

  - Use from the C API with ``qpdfjob_run_from_argv`` from
    :file:`qpdfjob-c.h`. If you are calling from a Windows-style main
    and have an argv array of ``wchar_t``, you can use
    ``qpdfjob_run_from_wide_argv``.

- The job JSON file format

  - Use from the CLI with the :qpdf:ref:`--job-json-file` parameter

  - Use from the C++ API with ``QPDFJob::initializeFromJson``

  - Use from the C API with ``qpdfjob_run_from_json`` from :file:`qpdfjob-c.h`

  - Note: this is unrelated to :qpdf:ref:`--json` but can be combined
    with it. For more information on qpdf JSON (vs. QPDFJob JSON), see
    :ref:`json`.

- The ``QPDFJob`` C++ API

If you can understand how to use the :command:`qpdf` CLI, you can
understand the ``QPDFJob`` class and the JSON file. qpdf guarantees
that all of the above methods are in sync. Here's how it works:

.. list-table:: QPDFJob Interfaces
   :widths: 30 30 30
   :header-rows: 1

   - - CLI
     - JSON
     - C++

   - - ``--some-option``
     - ``"someOption": ""``
     - ``config()->someOption()``

   - - ``--some-option=value``
     - ``"someOption": "value"``
     - ``config()->someOption("value")``

   - - positional argument
     - ``"otherOption": "value"``
     - ``config()->otherOption("value")``

In the JSON file, the JSON structure is an object (dictionary) whose
keys are command-line flags converted to camelCase. Positional
arguments have some corresponding key, which you can find by running
``qpdf`` with the :qpdf:ref:`--job-json-help` flag. For example, input
and output files are named by positional arguments on the CLI. In the
JSON, they appear in the ``"inputFile"`` and ``"outputFile"`` keys.
The following are equivalent:

.. It would be nice to have an automated test that these are all the
   same, but we have so few live examples that it's not worth it for
   now.

CLI:
  ::

     qpdf infile.pdf outfile.pdf \
        --pages . other.pdf --password=x 1-5 -- \
        --encrypt user owner 256 --print=low -- \
        --object-streams=generate

Job JSON:
  .. code-block:: json

      {
        "inputFile": "infile.pdf",
        "outputFile": "outfile.pdf",
        "pages": [
          {
            "file": "."
          },
          {
            "file": "other.pdf",
            "password": "x",
            "range": "1-5"
          }
        ],
        "encrypt": {
          "userPassword": "user",
          "ownerPassword": "owner",
          "256bit": {
            "print": "low"
          }
        },
        "objectStreams": "generate"
      }

C++ code:
  .. code-block:: c++

      #include <qpdf/QPDFJob.hh>
      #include <qpdf/QPDFUsage.hh>
      #include <iostream>

      int main(int argc, char* argv[])
      {
          try
          {
              QPDFJob j;
              j.config()
                  ->inputFile("infile.pdf")
                  ->outputFile("outfile.pdf")
                  ->pages()
                  ->pageSpec(".", "1-z")
                  ->pageSpec("other.pdf", "1-5", "x")
                  ->endPages()
                  ->encrypt(256, "user", "owner")
                  ->print("low")
                  ->endEncrypt()
                  ->objectStreams("generate")
                  ->checkConfiguration();
              j.run();
          }
          catch (QPDFUsage& e)
          {
              std::cerr << "configuration error: " << e.what() << std::endl;
              return 2;
          }
          catch (std::exception& e)
          {
              std::cerr << "other error: " << e.what() << std::endl;
              return 2;
          }
          return 0;
      }

Note the ``QPDFUsage`` exception above. This is thrown whenever a
configuration error occurs. These exactly correspond to usage messages
issued by the :command:`qpdf` CLI for things like omitting an output
file, specifying `--pages` multiple times, or other invalid
combinations of options. ``QPDFUsage`` is thrown by the argv and JSON
interfaces as well as the native ``QPDFJob`` interface.

It is also possible to mix and match command-line options and JSON
from the CLI. For example, you could create a file called
:file:`my-options.json` containing the following:

.. code-block:: json

    {
      "encrypt": {
        "userPassword": "",
        "ownerPassword": "owner",
        "256bit": {
        }
      },
      "objectStreams": "generate"
    }

and use it with other options to create 256-bit encrypted (but
unrestricted) files with object streams while specifying other
parameters on the command line, such as

::

   qpdf infile.pdf outfile.pdf --job-json-file=my-options.json

.. _qpdfjob-design:

See also :file:`examples/qpdf-job.cc` in the source distribution as
well as comments in ``QPDFJob.hh``.


QPDFJob Design
--------------

This section describes some of the design rationale and history behind
``QPDFJob``.

Documentation of ``QPDFJob`` is divided among three places:

- "HOW TO ADD A COMMAND-LINE ARGUMENT" in :file:`README-maintainer`
  provides a quick reminder of how to add a command-line argument.

- The source file :file:`generate_auto_job` has a detailed explanation
  about how ``QPDFJob`` and ``generate_auto_job`` work together.

- This chapter of the manual has other details.

Prior to qpdf version 10.6.0, the qpdf CLI executable had a lot of
functionality built into it that was not callable from the library as
such. This created a number of problems:

- Some of the logic in :file:`qpdf.cc` was pretty complex, such as
  image optimization, generating JSON output, and many of the page
  manipulations. While those things could all be coded using the C++
  API, there would be a lot of duplicated code.

- Page splitting and merging will get more complicated over time as
  qpdf supports a wider range of document-level options. It would be
  nice to be able to expose this to library users instead of baking it
  all into the CLI.

- Users of other languages who just wanted an interface to do things
  that the CLI could do didn't have a good way to do it, such as just
  handing a library call a set of command-line options or an
  equivalent JSON object that could be passed in as a string.

- The qpdf CLI itself was almost 8,000 lines of code. It needed to be
  refactored, cleaned up, and split.

- Exposing a new feature via the command-line required making lots of
  small edits to lots of small bits of code, and it was easy to forget
  something. Adding a code generator, while complex in some ways,
  greatly reduces the chances of error when extending qpdf.

Here are a few notes on some design decisions about QPDFJob and its
various interfaces.

- Bare command-line options (flags with no parameter) map to config
  functions that take no options and to JSON keys whose values are
  required to be the empty string. The rationale is that we can later
  change these bare options to options that take an optional parameter
  without breaking backward compatibility in the CLI or the JSON.
  Options that take optional parameters generate two config functions:
  one has no arguments, and one that has a ``char const*`` argument.
  This means that adding an optional parameter to a previously bare
  option also doesn't break binary compatibility.

- Adding a new argument to :file:`job.yml` automatically triggers
  almost everything by declaring and referencing things that you have
  to implement. This way, once you get the code to compile and link,
  you know you haven't forgotten anything. There are two tricky cases:

  - If an argument handler has to do something special, like call a
    nested config method or select an option table, you have to
    implement it manually. This is discussed in
    :file:`generate_auto_job`.

  - When you add an option that has optional parameters or choices,
    both of the handlers described above are declared, but only the
    one that takes an argument is referenced. You have to remember to
    implement the one that doesn't take an argument or else people
    will get a linker error if they try to call it. The assumption is
    that things with optional parameters started out as bare, so the
    argument-less version is already there.

- If you have to add a new option that requires its own option table,
  you will have to do some extra work including adding a new nested
  Config class, adding a config member variable to ``ArgParser`` in
  :file:`QPDFJob_argv.cc` and ``Handlers`` in :file:`QPDFJob_json.cc`,
  and make sure that manually implemented handlers are consistent with
  each other. It is best to add explicit test cases for all the
  various ways to get to the option.
