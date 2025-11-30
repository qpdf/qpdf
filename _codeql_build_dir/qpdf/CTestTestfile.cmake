# CMake generated Testfile for 
# Source directory: /home/runner/work/qpdf/qpdf/qpdf
# Build directory: /home/runner/work/qpdf/qpdf/_codeql_build_dir/qpdf
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(qpdf "perl" "/home/runner/work/qpdf/qpdf/run-qtest" "--disable-tc" "--top" "/home/runner/work/qpdf/qpdf" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/qpdf" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/libqpdf" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/compare-for-test" "--code" "/home/runner/work/qpdf/qpdf/qpdf" "--color" "ON" "--show-on-failure" "OFF" "--tc" "/home/runner/work/qpdf/qpdf/qpdf/*.cc" "--tc" "/home/runner/work/qpdf/qpdf/qpdf/*.c" "--tc" "/home/runner/work/qpdf/qpdf/libqpdf/*.cc")
set_tests_properties(qpdf PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/qpdf/qpdf/qpdf/CMakeLists.txt;50;add_test;/home/runner/work/qpdf/qpdf/qpdf/CMakeLists.txt;0;")
