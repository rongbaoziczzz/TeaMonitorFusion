# Third-party notices for the Aravis runtime

The Windows MinGW64 runtime in `runtime/` was assembled from the official [MSYS2 package repository](https://packages.msys2.org/) on 2026-08-05. It is not a Pleora SDK or an unofficial SDK mirror.

Primary package:

- `mingw-w64-x86_64-aravis 0.8.33-4`, LGPL-2.1-or-later
- Official package: <https://packages.msys2.org/packages/mingw-w64-x86_64-aravis>

Runtime/development dependencies were taken from these official MSYS2 packages:

- `mingw-w64-x86_64-glib2 2.88.3-1`
- `mingw-w64-x86_64-libusb 1.0.30-1`
- `mingw-w64-x86_64-libxml2 2.15.3-1`
- `mingw-w64-x86_64-zlib 1.3.2-2`
- `mingw-w64-x86_64-libffi 3.7.1-1`
- `mingw-w64-x86_64-libiconv 1.19-1`
- `mingw-w64-x86_64-pcre2 10.47-1`
- `mingw-w64-x86_64-gettext-runtime 1.0-1`

Each component remains subject to its own upstream license. The Aravis source license is available at `aravis-0.8.36/COPYING`; package metadata and upstream license files should be retained when redistributing a binary release.
