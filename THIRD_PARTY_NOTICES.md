# Third-party components

- Qt 6: dynamically linked, LGPL-3.0 (or a separately acquired commercial license).
  Source and licensing: https://www.qt.io/licensing/open-source-lgpl-obligations
  Qt source: https://download.qt.io/archive/qt/6.8/6.8.3/submodules/
  LGPL-3.0 and GPL-3.0 texts are included under `licenses/`. Users may replace
  LGPL libraries and reverse engineer the application to debug modifications
  to those libraries. No restriction in this distribution overrides that right.
- OpenSSL 3.5: Apache-2.0, https://github.com/openssl/openssl
  Source for Android builds is pinned in `packaging/android/build.sh`.
- QR Code generator v1.8.0 by Project Nayuki: MIT license, copyright and license
  are retained in `third_party/qrcodegen/qrcodegen.hpp` and `.cpp`.
  Source: https://github.com/nayuki/QR-Code-generator/tree/v1.8.0/cpp

Quicky's own source license has not yet been selected by the repository owner.
Before a public production distribution, archive the exact Qt/OpenSSL sources,
notices and build instructions matching each shipped dependency version.
