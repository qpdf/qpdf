# Building QPDF with Emscripten

This guide explains how to compile QPDF to JavaScript/WebAssembly using Emscripten.

> **Note**: For pre-built bundles and a convenient wrapper around the WebAssembly build, check out [qpdf.js](https://github.com/j3k0/qpdf.js).

## Prerequisites

Install Emscripten by following the [official installation instructions](https://emscripten.org/docs/getting_started/downloads.html).

## Quick Start

1. Create and enter build directory:
```bash
mkdir build-emscripten && cd build-emscripten
```

2. Build and copy to examples:
```bash
# Configure and build
emcmake cmake .. \
    -DEMSCRIPTEN=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF && \
emmake make -j8 qpdf && \
cp qpdf/qpdf.* ../examples/qpdf-js/lib/
```

3. Test the build:
```bash
cd ../examples/qpdf-js
python3 serve.py
```

4. Open `http://localhost:8523/test.html` in your browser

## Build Output

The build produces these files in `build-emscripten/qpdf/`:
- `qpdf.js`: JavaScript glue code
- `qpdf.wasm`: WebAssembly binary

## Browser Usage Example

Here's a minimal example to get started:

```html
<!DOCTYPE html>
<html>
<head>
    <title>QPDF Test</title>
</head>
<body>
    <pre id="output"></pre>
    
    <script src="qpdf.js"></script>
    <script>
        function print(text) {
            document.getElementById('output').textContent += text + '\n';
            console.log(text); // Also log to console for debugging
        }

        QPDF({
            keepAlive: true,
            logger: print,
            ready: function(qpdf) {
                // Test QPDF is working
                qpdf.execute(['--copyright'], function(err) {
                    if (err) {
                        print('Error: ' + err.message);
                        return;
                    }
                    print('QPDF loaded successfully!');
                });
            }
        });
    </script>
</body>
</html>
```

For a complete working example with PDF operations like checking and encryption, see [examples/qpdf-js/test.html](examples/qpdf-js/test.html) in the repository.

## Additional Resources

- [qpdf.js](https://github.com/j3k0/qpdf.js) - Pre-built WebAssembly bundles with JavaScript wrapper
- [Emscripten Documentation](https://emscripten.org/docs/getting_started/index.html)
- [WebAssembly](https://webassembly.org/)
- [QPDF Documentation](https://qpdf.readthedocs.io/) 