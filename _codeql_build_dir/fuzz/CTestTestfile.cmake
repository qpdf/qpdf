# CMake generated Testfile for 
# Source directory: /home/runner/work/qpdf/qpdf/fuzz
# Build directory: /home/runner/work/qpdf/qpdf/_codeql_build_dir/fuzz
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(fuzz "perl" "/home/runner/work/qpdf/qpdf/run-qtest" "--disable-tc" "--env" "QPDF_FUZZ_CORPUS=/home/runner/work/qpdf/qpdf/_codeql_build_dir/fuzz/qpdf_corpus" "--top" "/home/runner/work/qpdf/qpdf" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/fuzz" "--bin" "/home/runner/work/qpdf/qpdf/_codeql_build_dir/qpdf" "--code" "/home/runner/work/qpdf/qpdf/fuzz" "--color" "ON" "--show-on-failure" "OFF")
set_tests_properties(fuzz PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/qpdf/qpdf/fuzz/CMakeLists.txt;199;add_test;/home/runner/work/qpdf/qpdf/fuzz/CMakeLists.txt;0;")
