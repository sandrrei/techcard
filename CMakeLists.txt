cmake_minimum_required(VERSION 3.16)
project(techcard VERSION 0.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Android only: bundle OpenSSL 1.1 (needed for HTTPS with Qt 5.15).
# Must come BEFORE find_package(Qt5), which reads ANDROID_EXTRA_LIBS.
if(ANDROID)
    set(OPENSSL_DIR "$ENV{HOME}/Android/Sdk/android_openssl/ssl_1.1/${ANDROID_ABI}")
    if(EXISTS "${OPENSSL_DIR}/libssl_1_1.so")
        set(ANDROID_EXTRA_LIBS
            "${OPENSSL_DIR}/libcrypto_1_1.so;${OPENSSL_DIR}/libssl_1_1.so"
            CACHE INTERNAL "")
    else()
        message(WARNING "OpenSSL for Android not found in ${OPENSSL_DIR}: HTTPS will fail")
    endif()

    # Optional: android/ with a custom AndroidManifest.xml (Qt Creator: "Create Templates")
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/android)
        set(ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android CACHE INTERNAL "")
    endif()
endif()

# Qt 5.15 on both Ubuntu Touch (Clickable SDK) and Android
find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Qml Quick Network)

option(CLICK_MODE "Install as an Ubuntu Touch click package" OFF)

set(SOURCES
    src/main.cpp
    src/datastore.cpp
    src/datastore.h
    src/qrcodeitem.cpp
    src/qrcodeitem.h
    src/third_party/qrcodegen.cpp
    src/third_party/qrcodegen.hpp
    qml/qml.qrc
)

if(ANDROID)
    # Qt 5.15 Android apps are a shared library loaded by the Java activity
    add_library(techcard SHARED ${SOURCES})
else()
    add_executable(techcard ${SOURCES})
endif()

target_include_directories(techcard PRIVATE src src/third_party)

target_link_libraries(techcard PRIVATE
    Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick Qt5::Network)

if(CLICK_MODE)
    # Ubuntu Touch: everything goes to the root of the click package
    install(TARGETS techcard RUNTIME DESTINATION .)
    install(FILES manifest.json techcard.apparmor techcard.desktop
            DESTINATION .)
    install(FILES assets/logo.svg DESTINATION assets)
elseif(NOT ANDROID)
    install(TARGETS techcard RUNTIME DESTINATION bin)
endif()
