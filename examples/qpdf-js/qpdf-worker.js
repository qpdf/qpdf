/* eslint-env worker */
/* global FS, callMain */

const scriptURL = self.location.href;
const basePath = scriptURL.substring(0, scriptURL.lastIndexOf('/') + 1);
let errorOrWarning = '';

function stdout(txt) {
  postMessage({
    type: 'stdout',
    line: txt
  });
}

function debugLog(type, ...args) {
  const msg = args.map(arg => {
    if (arg instanceof Error) {
      return arg.message + '\n' + arg.stack;
    }
    if (typeof arg === 'object') {
      return JSON.stringify(arg, null, 2);
    }
    return String(arg);
  }).join(' ');
  // stdout(`[DEBUG:${type}] ${msg}`);
}


var Module = {
  thisProgram: 'qpdf',
  noInitialRun: true,
  print: function(text) {
    stdout(text);
  },
  printErr: function(text) {
    if (text.startsWith('WARNING: ')) {
      errorOrWarning = text.slice(8);
    }
    stdout('[stderr] ' + text);
  },
  onRuntimeInitialized: function() {
    debugLog('init', 'QPDF runtime initialized');
    postMessage({
      type: 'ready'
    });
  },
  locateFile: function(path, prefix) {
    if (path.endsWith('.wasm')) {
      return basePath + 'lib/' + path;
    }
    return prefix + path;
  },
  quit: function(status, toThrow) {
    debugLog('quit', `Program quit with status: ${status}`, toThrow);
    if (toThrow) {
      throw status;
    }
  }
};

importScripts(basePath + 'lib/qpdf.js');

function getFileData(fileName) {
  try {
    if (!FS.analyzePath(fileName).exists) {
      debugLog('getFileData', `File ${fileName} does not exist`);
      return null;
    }
    const file = FS.root.contents[fileName];
    if (!file) {
      debugLog('getFileData', `Could not access ${fileName} in FS`);
      return null;
    }
    debugLog('getFileData', `Successfully read ${fileName}, size: ${file.contents.length}`);
    return file.contents;
  } catch (error) {
    debugLog('getFileData', 'Error reading file:', error);
    return null;
  }
}

onmessage = function(event) {
  var message = event.data;

  switch (message.type) {
    case 'save': {
      const filename = message.filename;
      const arrayBuffer = message.arrayBuffer;
      
      try {
        debugLog('save', `Saving ${filename}, size: ${arrayBuffer.byteLength}`);
        
        if (!(arrayBuffer instanceof ArrayBuffer) && !(ArrayBuffer.isView(arrayBuffer))) {
          throw new Error(`Invalid data type. Expected ArrayBuffer, got ${Object.prototype.toString.call(arrayBuffer)}`);
        }
        
        // Clean up any existing file
        if (FS.analyzePath(filename).exists) {
          debugLog('save', `Removing existing ${filename}`);
          FS.unlink(filename);
        }
        
        const data = ArrayBuffer.isView(arrayBuffer) ? arrayBuffer : new Uint8Array(arrayBuffer);
        FS.createDataFile('/', filename, data, true, false);
        
        debugLog('save', `Successfully saved ${filename}`);
        postMessage({
          type: 'saved',
          filename
        });
      } catch (error) {
        debugLog('save', 'Error saving file:', error);
        postMessage({
          type: 'error',
          message: error.message
        });
      }
      break;
    }

    case 'load': {
      const filename = message.filename;
      try {
        debugLog('load', `Loading ${filename}`);
        const data = getFileData(filename);
        if (data) {
          debugLog('load', `Successfully loaded ${filename}`);
          postMessage({
            type: 'loaded',
            filename,
            arrayBuffer: data
          });
        } else {
          throw new Error(`Failed to load ${filename}`);
        }
      } catch (error) {
        debugLog('load', 'Error loading file:', error);
        postMessage({
          type: 'error',
          message: error.message
        });
      }
      break;
    }

    case 'execute': {
      const args = message.args;
      try {
        debugLog('execute', `Running qpdf with args: ${args.join(' ')}`);
        
        let exitStatus = undefined;
        let exitMessage = undefined;
        try {
          exitStatus = callMain(args);
        } catch (e) {
          const decoded = decodeQPDFExc(e);
          if (decoded) {
            stdout('QPDFExc thrown');
            stdout('file: ' + decoded.filename);
            stdout('message: ' + decoded.message);
            stdout('dump:');
            stdout(decoded.dump);
            exitMessage = decoded.message;
          }
          else if (e > 100 && e < HEAP8.length) {
            stdout(hexdump(HEAP8.slice(e, e + 512), e));
          }
          else {
            stdout('Error thrown: ' + e);
          }
        }

        debugLog('execute', `Command completed with status: ${exitStatus === undefined ? EXITSTATUS : exitStatus}`);

        if (exitStatus !== 0) {
          postMessage({
            type: 'executed',
            status: exitStatus,
            error: exitMessage || errorOrWarning || 'Command failed',
          });
        } else {
          postMessage({
            type: 'executed',
            status: exitStatus,
          });
        }
      } catch (error) {
        debugLog('execute', 'Error during execution:', error);
        postMessage({
          type: 'executed',
          status: 1,
          error: error.message || String(error)
        });
      }
      break;
    }
  }
};

function decodeQPDFExc(addr) {
    try {
        if (typeof addr !== 'number') {
            debugLog('QPDFExc', 'Invalid address type:', typeof addr);
            return null;
        }

        debugLog('QPDFExc', `Attempting to decode exception at address 0x${addr.toString(16)}`);

        // Helper to read a null-terminated string
        const readCString = (offset) => {
            let result = '';
            let i = 0;
            while (i < 1000) { // Safety limit
                const char = HEAP8[offset + i] & 0xFF;
                if (char === 0) break;
                result += String.fromCharCode(char);
                i++;
            }
            if (result) {
                debugLog('QPDFExc', `Read C string at 0x${offset.toString(16)}: "${result}"`);
            }
            return result;
        };

        // Helper to read a 32-bit little endian value
        const readU32 = (offset) => {
            const value = HEAP8[offset] & 0xFF |
                ((HEAP8[offset + 1] & 0xFF) << 8) |
                ((HEAP8[offset + 2] & 0xFF) << 16) |
                ((HEAP8[offset + 3] & 0xFF) << 24);
            debugLog('QPDFExc', `Read U32 at 0x${offset.toString(16)}: 0x${value.toString(16)}`);
            return value;
        };

        // First field might be vtable
        const vtable = readU32(addr);
        debugLog('QPDFExc', `Potential vtable pointer: 0x${vtable.toString(16)}`);

        // Second field
        const field2 = readU32(addr + 4);
        debugLog('QPDFExc', `Second field: 0x${field2.toString(16)}`);

        // Next fields appear to be lengths or flags
        const field3 = readU32(addr + 8);  // 0 in arg error case
        const field4 = readU32(addr + 12); // 0x3b in arg error case
        const field5 = readU32(addr + 16); // 0x22 in arg error case
        const field6 = readU32(addr + 20); // 0x22 in arg error case
        const field7 = readU32(addr + 24); // 0 in arg error case

        debugLog('QPDFExc', `Additional fields: ${field3}, ${field4}, ${field5}, ${field6}, ${field7}`);

        // Look for error patterns in a more structured way
        const errorPatterns = [
            // File errors
            {
                type: 'file_error',
                markers: ["can't find", "error", "failed", "unrecognized", "invalid"]
            },
            // Object errors (new pattern)
            {
                type: 'object_error',
                markers: ["expected", "object", "offset"]
            }
        ];

        let errorInfo = {
            filename: null,
            object: null,
            offset: null,
            message: null
        };

        // Look for structured error components
        for (let i = 0; i < 200; i++) {
            const testStr = readCString(addr + i);
            if (!testStr) continue;

            // Find filename
            if (testStr.endsWith('.pdf')) {
                errorInfo.filename = testStr;
                debugLog('QPDFExc', `Found filename: "${testStr}"`);
                i += testStr.length;
            }

            // Find object reference (e.g., "object 2 0")
            const objectMatch = testStr.match(/object (\d+) (\d+)/);
            if (objectMatch) {
                errorInfo.object = `${objectMatch[1]} ${objectMatch[2]}`;
                debugLog('QPDFExc', `Found object reference: "${errorInfo.object}"`);
            }

            // Find offset (e.g., "offset 60")
            const offsetMatch = testStr.match(/offset (\d+)/);
            if (offsetMatch) {
                errorInfo.offset = parseInt(offsetMatch[1]);
                debugLog('QPDFExc', `Found offset: ${errorInfo.offset}`);
            }

            // Look for error message
            for (const pattern of errorPatterns) {
                if (pattern.markers.some(marker => testStr.includes(marker))) {
                    errorInfo.message = testStr;
                    debugLog('QPDFExc', `Found error message (${pattern.type}): "${testStr}"`);
                    i += testStr.length;
                    break;
                }
            }
        }

        // Format the complete error message
        let formattedMessage = '';
        if (errorInfo.filename) {
            formattedMessage += errorInfo.filename;
            if (errorInfo.object || errorInfo.offset) {
                formattedMessage += ' (';
                if (errorInfo.object) formattedMessage += `object ${errorInfo.object}`;
                if (errorInfo.object && errorInfo.offset) formattedMessage += ', ';
                if (errorInfo.offset) formattedMessage += `offset ${errorInfo.offset}`;
                formattedMessage += ')';
            }
            formattedMessage += ': ';
        }
        formattedMessage += errorInfo.message || 'Unknown error';

        const result = {
            type: 'QPDFExc',
            addr: '0x' + addr.toString(16),
            vtable: '0x' + readU32(addr).toString(16),
            field2: '0x' + readU32(addr + 4).toString(16),
            lengths: [
                readU32(addr + 8),
                readU32(addr + 12),
                readU32(addr + 16),
                readU32(addr + 20),
                readU32(addr + 24)
            ],
            ...errorInfo,
            formattedMessage,
            dump: hexdump(HEAP8.slice(addr, addr + 128), addr)
        };

        debugLog('QPDFExc', 'Successfully decoded exception:', result);
        return result;

    } catch (e) {
        debugLog('QPDFExc', 'Error while decoding:', e);
        return null;
    }
}

function hexdump(array, offset = 0, length = array.length) {
  const bytesPerLine = 16;
  let output = '';
  
  for (let i = 0; i < length; i += bytesPerLine) {
    // Address column
    output += (offset + i).toString(16).padStart(8, '0');
    output += '  ';
    
    // Hex columns
    for (let j = 0; j < bytesPerLine; j++) {
      if (j === 8) output += ' ';
      if (i + j < length) {
        // Convert signed byte to unsigned byte (0-255)
        const unsignedByte = array[i + j] & 0xFF;
        output += unsignedByte.toString(16).padStart(2, '0');
        output += ' ';
      } else {
        output += '   ';
      }
    }
    
    // ASCII column
    output += ' |';
    for (let j = 0; j < bytesPerLine && i + j < length; j++) {
      // Convert signed byte to unsigned byte (0-255)
      const unsignedByte = array[i + j] & 0xFF;
      // Print as ASCII if printable, otherwise print a dot
      output += (unsignedByte >= 32 && unsignedByte <= 126) ? 
                String.fromCharCode(unsignedByte) : '.';
    }
    output += '|\n';
  }
  return output;
}
