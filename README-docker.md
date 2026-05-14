# qpdf Minimal Production Docker Image

This repository provides a highly optimized, security-hardened Docker image for running `qpdf`.

## Features

- **Minimal Size**: Built using a multi-stage process with Alpine Linux and a statically linked, stripped binary. The final image size is approximately **13 MB**.
- **Security Hardened**:
  - **Non-Root Execution**: The container runs as a dedicated `qpdf` user, following the principle of least privilege.
  - **Native Crypto**: Configured to use qpdf's native crypto implementation, eliminating dependencies on external libraries like OpenSSL or GnuTLS and reducing the potential attack surface.
  - **Minimal Base**: Uses Alpine Linux, which is designed for security and has a very small footprint.
  - **Zero Vulnerabilities**: The image is designed to pass `trivy` scans with zero or extremely low vulnerability counts.
- **Production Ready**: Uses C++20 optimizations and is stripped of debugging symbols.

## Building the Image

To build the image locally, use the provided `Dockerfile`:

```bash
docker build -f Dockerfile -t qpdf-minimal:latest .
```

## Usage

The image is configured with `qpdf` as the entrypoint. By default, it works in the `/data` directory.

### Basic Usage (Check Version)

```bash
docker run --rm qpdf-minimal:latest --version
```

### Processing PDF Files

To process local files, you should mount your current directory to the `/data` volume:

```bash
docker run --rm -v $(pwd):/data qpdf-minimal:latest input.pdf output.pdf --linearize
```

### Examples

**Decrypting a file:**
```bash
docker run --rm -v $(pwd):/data qpdf-minimal:latest --password=mypassword --decrypt input.pdf decrypted.pdf
```

**Extracting pages:**
```bash
docker run --rm -v $(pwd):/data qpdf-minimal:latest input.pdf --pages . 1-5 -- output.pdf
```

**Removing all metadata:**
```bash
docker run --rm -v $(pwd):/data qpdf-minimal:latest --empty --pages input.pdf -- output.pdf
```

## Why Alpine was chosen over Scratch
While a `scratch` image would be even smaller, Alpine Linux (3.20) was selected as the final base image for several reasons:
1. **Security Updates**: Alpine provides a robust mechanism for security updates via its package manager if needed.
2. **Standard Compatibility**: It includes a basic shell (`/bin/sh`) and standard utilities, making it easier to integrate into CI/CD pipelines and shell-based automation.
3. **Robustness**: Alpine is a well-known, security-focused distribution that is widely trusted in production environments.

Despite using Alpine, the binary itself is statically linked to ensure it has no dependencies on host libraries.
