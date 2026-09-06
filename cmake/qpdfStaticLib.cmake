# Find the static library for `name`. If `hint_dir` is set, only look there for the archive. Sets
# `out_var` to the absolute path of the archive or to `<out_var>-NOTFOUND`.
function(qpdf_find_static_lib out_var name hint_dir)
  set(saved_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_STATIC_LIBRARY_SUFFIX})
  if(hint_dir)
    find_library(${out_var} NAMES ${name} PATHS ${hint_dir} NO_DEFAULT_PATH)
  else()
    find_library(${out_var} NAMES ${name})
  endif()
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${saved_suffixes})
  set(${out_var} ${${out_var}} PARENT_SCOPE)
endfunction()
