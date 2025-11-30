file(REMOVE_RECURSE
  "libqpdf.a"
  "libqpdf.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/libqpdf_static.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
