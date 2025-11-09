# Native Compression Dependencies (zlib only)

This project uses zlib (zlib/DEFLATE) for native compression. Please ensure the zlib development
headers and libraries are available on your system.

Windows (Visual Studio / vcpkg):

```powershell
vcpkg install zlib:x64-windows
```

Linux (Ubuntu/Debian):

```bash
sudo apt-get update
sudo apt-get install zlib1g-dev
```

If zlib is installed in a custom location, pass the appropriate CMake variables or set
CPATH/LIBRARY_PATH environment variables so CMake can find zlib.

## Build Configuration

The CMake configuration will look for these libraries in standard system locations.
If installed in non-standard locations, you may need to set:

- ZLIB_ROOT

Example:
```bash
cmake -DZLIB_ROOT=/path/to/zlib ..
```
