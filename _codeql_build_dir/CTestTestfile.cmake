# CMake generated Testfile for 
# Source directory: /home/runner/work/qpdf/qpdf
# Build directory: /home/runner/work/qpdf/qpdf/_codeql_build_dir
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(check-assert "perl" "/home/runner/work/qpdf/qpdf/check_assert")
set_tests_properties(check-assert PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/qpdf/qpdf/CMakeLists.txt;355;add_test;/home/runner/work/qpdf/qpdf/CMakeLists.txt;0;")
subdirs("include")
subdirs("libqpdf")
subdirs("compare-for-test")
subdirs("qpdf")
subdirs("libtests")
subdirs("examples")
subdirs("zlib-flate")
subdirs("manual")
subdirs("fuzz")
