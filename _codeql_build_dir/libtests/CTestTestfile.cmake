# CMake generated Testfile for 
# Source directory: /home/runner/work/qpdf/qpdf/libtests
# Build directory: /home/runner/work/qpdf/qpdf/_codeql_build_dir/libtests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(libtests "perl" "/home/runner/work/qpdf/qpdf/run-qtest" "--disable-tc" "--top" "/home/runner/work/qpdf/qpdf" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/libtests" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/qpdf" "--code" "/home/runner/work/qpdf/qpdf/libtests" "--color" "ON" "--show-on-failure" "OFF" "--tc" "/home/runner/work/qpdf/qpdf/libtests/*.cc" "--tc" "/home/runner/work/qpdf/qpdf/libqpdf/*.cc" "--tc" "/home/runner/work/qpdf/qpdf/libqpdf/qpdf/bits_functions.hh")
set_tests_properties(libtests PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/qpdf/qpdf/libtests/CMakeLists.txt;56;add_test;/home/runner/work/qpdf/qpdf/libtests/CMakeLists.txt;0;")
