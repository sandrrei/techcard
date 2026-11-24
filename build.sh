#!/usr/bin/env bash
# TechCard build helper: build and install on Android and Ubuntu Touch.
# Usage: ./build.sh <command>   (run without arguments for the list)
set -euo pipefail
cd "$(dirname "$0")"

QT_ANDROID="${QT_ANDROID:-$HOME/Qt/5.15.2/android}"
SDK="${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}"
NDK="$SDK/ndk/21.4.7075529"
JDK11=/usr/lib/jvm/java-11-openjdk-amd64
BUILD=build-android
APK="$BUILD/android-build/build/outputs/apk/debug/android-build-debug.apk"
PKG=com.sandrrei.techcard
OLD_PKG=org.qtproject.example.techcard   # package id used up to version 0.2.x
APP_NAME=TechCard
MANIFEST=android/AndroidManifest.xml

say()  { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }
fail() { printf '\n\033[1;31mERROR: %s\033[0m\n' "$*" >&2; exit 1; }

# Serial of the first connected Android phone (Ubuntu Touch phones have no getprop)
android_serial() {
    local s
    for s in $(adb devices | awk 'NR>1 && $2=="device" {print $1}'); do
        if [ -n "$(adb -s "$s" shell getprop ro.build.version.release 2>/dev/null | tr -d '\r')" ]; then
            echo "$s"; return 0
        fi
    done
    return 1
}

# Create android/AndroidManifest.xml from Qt's template (first time only),
# then set the app name, package id, icon and version on every build.
android_prepare() {
    if [ ! -f "$MANIFEST" ]; then
        local template="$QT_ANDROID/src/android/templates/AndroidManifest.xml"
        [ -f "$template" ] || fail "Qt Android manifest template not found: $template"
        say "Creating $MANIFEST from the Qt template"
        mkdir -p android
        cp "$template" "$MANIFEST"
    fi

    local version code
    version=$(sed -n 's/^project(techcard VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
    IFS=. read -r a b c <<<"$version"
    code=$(( a * 10000 + b * 100 + c ))

    sed -i \
        -e "s/package=\"[^\"]*\"/package=\"$PKG\"/" \
        -e "s/android:label=\"[^\"]*\"/android:label=\"$APP_NAME\"/g" \
        -e "s/android:versionName=\"[^\"]*\"/android:versionName=\"$version\"/" \
        -e "s/android:versionCode=\"[^\"]*\"/android:versionCode=\"$code\"/" \
        "$MANIFEST"
    if grep -q 'android:icon=' "$MANIFEST"; then
        sed -i 's|android:icon="[^"]*"|android:icon="@mipmap/ic_launcher"|' "$MANIFEST"
    else
        sed -i 's|<application|<application android:icon="@mipmap/ic_launcher"|' "$MANIFEST"
    fi
    grep -q "package=\"$PKG\"" "$MANIFEST" || fail "Could not set the package id in $MANIFEST"
}

android_build() {
    [ -d "$QT_ANDROID" ] || fail "Qt for Android not found in $QT_ANDROID"
    [ -d "$NDK" ]        || fail "NDK 21.4.7075529 not found in $NDK"
    [ -d "$JDK11" ]      || fail "JDK 11 not found: sudo apt install openjdk-11-jdk"
    export JAVA_HOME="$JDK11"
    android_prepare

    if [ ! -f "$BUILD/build.ninja" ]; then
        say "Configuring Android build"
        cmake -S . -B "$BUILD" -G Ninja \
            -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI=arm64-v8a -DANDROID_NATIVE_API_LEVEL=21 -DANDROID_STL=c++_shared \
            -DANDROID_SDK="$SDK" \
            -DCMAKE_PREFIX_PATH="$QT_ANDROID" -DCMAKE_FIND_ROOT_PATH="$QT_ANDROID" \
            -DCMAKE_BUILD_TYPE=Debug
    fi

    say "Compiling"
    cmake --build "$BUILD"
    say "Packaging APK"
    cmake --build "$BUILD" --target apk

    # (list first: with pipefail, "unzip | grep -q" can fail on SIGPIPE)
    local contents
    contents=$(unzip -l "$APK")
    if grep -q libssl_1_1.so <<<"$contents"; then
        say "APK ready (with OpenSSL): $APK"
    else
        printf '\n\033[1;33mWARNING: OpenSSL is not in the APK, HTTPS will fail\033[0m\n'
    fi
}

android_install() {
    [ -f "$APK" ] || fail "No APK yet: run ./build.sh android"
    local serial
    serial=$(android_serial) || fail "No Android phone found: connect it, enable USB debugging and accept the prompt"
    say "Installing on $(adb -s "$serial" shell getprop ro.product.model | tr -d '\r') ($serial)"
    adb -s "$serial" install -r "$APK"
    adb -s "$serial" shell monkey -p "$PKG" -c android.intent.category.LAUNCHER 1 >/dev/null
    say "Started TechCard on the phone"
}

android_uninstall_old() {
    local serial
    serial=$(android_serial) || fail "No Android phone found"
    say "Removing the old app ($OLD_PKG)"
    adb -s "$serial" uninstall "$OLD_PKG" || echo "(not installed)"
}

android_screenshot() {
    local name="${1:-}" serial
    [ -n "$name" ] || fail "Usage: ./build.sh screenshot <name>   (e.g. android-card)"
    serial=$(android_serial) || fail "No Android phone found"
    mkdir -p docs/screenshots
    adb -s "$serial" exec-out screencap -p > "docs/screenshots/$name.png"
    say "Saved docs/screenshots/$name.png"
}

android_logs() {
    local serial
    serial=$(android_serial) || fail "No Android phone found"
    say "Logs from the Android phone (Ctrl+C to stop)"
    adb -s "$serial" logcat -c
    adb -s "$serial" logcat | grep --line-buffered -iE "techcard|qml|ssl|Fatal signal"
}

usage() {
    cat <<EOF
TechCard build helper

  Android (Samsung Z Fold)
    ./build.sh android          build the APK and install it on the phone
    ./build.sh android-build    only build the APK
    ./build.sh android-install  only install the last APK
    ./build.sh android-logs     show the app logs
    ./build.sh screenshot NAME  save the Android screen to docs/screenshots/NAME.png
    ./build.sh android-uninstall-old
                                remove the old "techcard" app (versions up to 0.2.x)

  Ubuntu Touch (Pixel 3 XL)
    ./build.sh ut               build, install and start on the phone
    ./build.sh ut-logs          show the app logs
    ./build.sh desktop          run the Ubuntu Touch version on this PC

  Other
    ./build.sh clean            delete all build folders (next build starts from scratch)
EOF
}

case "${1:-}" in
    android)         android_build; android_install ;;
    android-build)   android_build ;;
    android-install) android_install ;;
    android-logs)    android_logs ;;
    android-uninstall-old) android_uninstall_old ;;
    screenshot)      android_screenshot "${2:-}" ;;
    ut)              clickable --arch arm64 ;;
    ut-logs)         clickable logs ;;
    desktop)         clickable desktop ;;
    clean)           say "Removing build folders"; rm -rf "$BUILD" build ;;
    *)               usage ;;
esac
