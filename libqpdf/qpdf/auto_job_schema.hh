static constexpr char const* JOB_SCHEMA_DATA = R"({
  "inputFile": "input filename",
  "password": "password for encrypted file",
  "passwordFile": "read password from a file",
  "empty": "use empty file as input",
  "outputFile": "output filename",
  "replaceInput": "overwrite input with output",
  "qdf": "enable viewing PDF code in a text editor",
  "preserveUnreferenced": "preserve unreferenced objects",
  "newlineBeforeEndstream": "force a newline before endstream",
  "normalizeContent": "fix newlines in content streams",
  "streamData": "control stream compression",
  "compressStreams": "compress uncompressed streams",
  "recompressFlate": "uncompress and recompress flate",
  "decodeLevel": "control which streams to uncompress",
  "decrypt": "remove encryption from input file",
  "deterministicId": "generate ID deterministically",
  "staticAesIv": "use a fixed AES vector",
  "staticId": "use a fixed document ID",
  "noOriginalObjectIds": "omit original object IDs in qdf",
  "copyEncryption": "copy another file's encryption details",
  "encryptionFilePassword": "supply password for copyEncryption",
  "linearize": "linearize (web-optimize) output",
  "linearizePass1": "save pass 1 of linearization",
  "objectStreams": "control use of object streams",
  "minVersion": "set minimum PDF version",
  "forceVersion": "set output PDF version",
  "progress": "show progress when writing",
  "splitPages": "write pages to separate files",
  "encrypt": {
    "userPassword": "user password",
    "ownerPassword": "owner password",
    "40bit": {
      "annotate": "restrict document annotation",
      "extract": "restrict text/graphic extraction",
      "modify": "restrict document modification",
      "print": "restrict printing"
    },
    "128bit": {
      "accessibility": "restrict document accessibility",
      "annotate": "restrict document annotation",
      "assemble": "restrict document assembly",
      "cleartextMetadata": "don't encrypt metadata",
      "extract": "restrict text/graphic extraction",
      "form": "restrict form filling",
      "modifyOther": "restrict other modifications",
      "modify": "restrict document modification",
      "print": "restrict printing",
      "forceV4": "force V=4 in encryption dictionary",
      "useAes": "use AES with 128-bit encryption"
    },
    "256bit": {
      "accessibility": "restrict document accessibility",
      "annotate": "restrict document annotation",
      "assemble": "restrict document assembly",
      "cleartextMetadata": "don't encrypt metadata",
      "extract": "restrict text/graphic extraction",
      "form": "restrict form filling",
      "modifyOther": "restrict other modifications",
      "modify": "restrict document modification",
      "print": "restrict printing",
      "allowInsecure": "allow empty owner passwords",
      "forceR5": "use unsupported R=5 encryption"
    }
  },
  "check": "partially check whether PDF is valid",
  "checkLinearization": "check linearization tables",
  "filteredStreamData": "show filtered stream data",
  "rawStreamData": "show raw stream data",
  "showEncryption": "information about encrypted files",
  "showEncryptionKey": "show key with showEncryption",
  "showLinearization": "show linearization hint tables",
  "showNpages": "show number of pages",
  "showObject": "show contents of an object",
  "showPages": "display page dictionary information",
  "showXref": "show cross reference data",
  "withImages": "include image details with showPages",
  "listAttachments": "list embedded files",
  "showAttachment": "export an embedded file",
  "json": "show file in JSON format",
  "jsonKey": [
    "limit which keys are in JSON output"
  ],
  "jsonObject": [
    "limit which objects are in JSON"
  ],
  "jsonStreamData": "how to handle streams in json output",
  "jsonStreamPrefix": "prefix for json stream data files",
  "allowWeakCrypto": "allow insecure cryptographic algorithms",
  "keepFilesOpen": "manage keeping multiple files open",
  "keepFilesOpenThreshold": "set threshold for keepFilesOpen",
  "noWarn": "suppress printing of warning messages",
  "verbose": "print additional information",
  "testJsonSchema": "test generated json against schema",
  "ignoreXrefStreams": "use xref tables rather than streams",
  "passwordIsHexKey": "provide hex-encoded encryption key",
  "passwordMode": "tweak how qpdf encodes passwords",
  "suppressPasswordRecovery": "don't try different password encodings",
  "suppressRecovery": "suppress error recovery",
  "coalesceContents": "combine content streams",
  "compressionLevel": "set compression level for flate",
  "externalizeInlineImages": "convert inline to regular images",
  "iiMinBytes": "set minimum size for externalizeInlineImages",
  "removeUnreferencedResources": "remove unreferenced page resources",
  "addAttachment": [
    {
      "file": "file to attach",
      "creationdate": "set attachment's creation date",
      "description": "set attachment's description",
      "filename": "set attachment's displayed filename",
      "key": "specify attachment key",
      "mimetype": "attachment mime type, e.g. application/pdf",
      "moddate": "set attachment's modification date",
      "replace": "replace attachment with same key"
    }
  ],
  "removeAttachment": [
    "remove an embedded file"
  ],
  "copyAttachmentsFrom": [
    {
      "file": "file to copy attachments from",
      "password": "password for encrypted file",
      "prefix": "key prefix for copying attachments"
    }
  ],
  "collate": "collate with pages",
  "flattenAnnotations": "push annotations into content",
  "flattenRotation": "remove rotation from page dictionary",
  "generateAppearances": "generate appearances for form fields",
  "keepInlineImages": "exclude inline images from optimization",
  "oiMinArea": "minimum area for optimizeImages",
  "oiMinHeight": "minimum height for optimizeImages",
  "oiMinWidth": "minimum width for optimizeImages",
  "optimizeImages": "use efficient compression for images",
  "pages": [
    {
      "file": "source for for pages",
      "password": "password for encrypted file",
      "range": "page range"
    }
  ],
  "removePageLabels": "remove explicit page numbers",
  "rotate": "rotate pages",
  "overlay": {
    "file": "source file for overlay",
    "password": "password for encrypted file",
    "from": "source pages for underlay/overlay",
    "repeat": "overlay/underlay pages to repeat",
    "to": "destination pages for underlay/overlay"
  },
  "underlay": {
    "file": "source file for underlay",
    "password": "password for encrypted file",
    "from": "source pages for underlay/overlay",
    "repeat": "overlay/underlay pages to repeat",
    "to": "destination pages for underlay/overlay"
  },
  "warningExit0": "exit 0 even with warnings",
  "jobJsonFile": "job JSON file",
  "preserveUnreferencedResources": "use removeUnreferencedResources=no",
  "requiresPassword": "silently test a file's password",
  "isEncrypted": "silently test whether a file is encrypted"
})";
