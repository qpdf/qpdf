# CMake generated Testfile for 
# Source directory: /home/runner/work/qpdf/qpdf/examples
# Build directory: /home/runner/work/qpdf/qpdf/_codeql_build_dir/examples
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(examples "perl" "/home/runner/work/qpdf/qpdf/run-qtest" "--disable-tc" "--top" "/home/runner/work/qpdf/qpdf" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/examples" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/qpdf" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/compare-for-test" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/libqpdf" "--code" "/home/runner/work/qpdf/qpdf/examples" "--color" "ON" "--show-on-failure" "OFF" "--tc" "/home/runner/work/qpdf/qpdf/examples/*.cc" "--tc" "/home/runner/work/qpdf/qpdf/examples/*.c")
set_tests_properties(examples PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/qpdf/qpdf/examples/CMakeLists.txt;42;add_test;/home/runner/work/qpdf/qpdf/examples/CMakeLists.txt;0;")
