#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
: "${ANDROID_NDK_ROOT:?Set ANDROID_NDK_ROOT to Android NDK 26.1.10909125}"
: "${QT_ROOT_DIR:?Set QT_ROOT_DIR to the Qt 6.8.3 android_arm64_v8a installation}"
work="$(pwd)/build-android-deps"
mkdir -p "$work"
# Exact upstream OpenSSL 3.5.7 commit, verified against the official HTTPS Git remote.
openssl_commit=8cf17aaeb4599f8af87fefd810b5b5fee90fe69e
if [ ! -d "$work/openssl/.git" ]; then
  git clone --branch openssl-3.5.7 --depth 1 https://github.com/openssl/openssl.git "$work/openssl"
fi
actual_commit="$(git -C "$work/openssl" rev-parse HEAD)"
[ "$actual_commit" = "$openssl_commit" ] || { echo 'Unexpected OpenSSL source revision' >&2; exit 1; }
export PATH="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH"
export ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
if [ ! -f "$work/install/lib/libcrypto_3.so" ]; then
  # OpenSSL's documented target inheritance controls the library name and SONAME.
  cat > "$work/openssl/Configurations/99-quicky.conf" <<'CONFIG'
my %targets = (
  "quicky-android-arm64" => {
    inherit_from => [ "android-arm64" ],
    shared_extension => "_3.so",
  },
);
CONFIG
  pushd "$work/openssl"
  ./Configure quicky-android-arm64 -D__ANDROID_API__=28 shared no-tests --prefix="$work/install" --libdir=lib
  make -j2
  make install_sw
  popd
fi
"$QT_ROOT_DIR/bin/qt-cmake" -S . -B build-android -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 \
  -DANDROID_NDK="$ANDROID_NDK_ROOT" \
  -DOPENSSL_ROOT_DIR="$work/install" \
  -DOPENSSL_CRYPTO_LIBRARY="$work/install/lib/libcrypto_3.so" \
  -DOPENSSL_SSL_LIBRARY="$work/install/lib/libssl_3.so" \
  -DANDROID_EXTRA_LIBS="$work/install/lib/libcrypto_3.so;$work/install/lib/libssl_3.so"
cmake --build build-android --parallel 2
cmake --build build-android --target apk
# Preview APK uses the Android debug signing identity, never a production key.
find build-android/android-build/build/outputs/apk/debug -name '*.apk' -exec sh -c 'sha256sum "$1" > "$1.sha256"' _ '{}' \;
