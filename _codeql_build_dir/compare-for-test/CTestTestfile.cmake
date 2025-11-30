# CMake generated Testfile for 
# Source directory: /home/runner/work/qpdf/qpdf/compare-for-test
# Build directory: /home/runner/work/qpdf/qpdf/_codeql_build_dir/compare-for-test
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(compare-for-test "perl" "/home/runner/work/qpdf/qpdf/run-qtest" "--disable-tc" "--top" "/home/runner/work/qpdf/qpdf" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/compare-for-test" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/libqpdf" "--code" "/home/runner/work/qpdf/qpdf/compare-for-test" "--color" "ON" "--show-on-failure" "OFF" "--tc" "/home/runner/work/qpdf/qpdf/compare-for-test/*.cc")
set_tests_properties(compare-for-test PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/qpdf/qpdf/compare-for-test/CMakeLists.txt;6;add_test;/home/runner/work/qpdf/qpdf/compare-for-test/CMakeLists.txt;0;")
