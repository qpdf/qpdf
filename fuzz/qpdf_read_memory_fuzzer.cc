#include "qpdf/qpdf-c.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const unsigned char* data, size_t size) {
  const size_t kMaxSize = 64 * 1024; // 64 KiB
  size = std::min(size, kMaxSize);
  _qpdf_data* qpdf = qpdf_init();
  const char* buffer = reinterpret_cast<const char*>(data);
  qpdf_read_memory(qpdf, /*description=*/"", buffer, size, /*password=*/"");
  qpdf_cleanup(&qpdf);
  return 0;
}
