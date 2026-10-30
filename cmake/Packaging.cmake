# Debian packages (cpack -G DEB) for Debian, Ubuntu and Raspberry Pi OS.
#
# The package is built for the distribution it is built on: its release is
# 1~<codename> (e.g. guitariqx_0.1.0-1~bookworm_arm64.deb) so that the
# packages of several distributions can sit side by side in a release.

set(GUITARIQX_DEB_DISTRO "" CACHE STRING "Distribution codename of the Debian package (default: from /etc/os-release)")
if(NOT GUITARIQX_DEB_DISTRO AND EXISTS /etc/os-release)
    file(STRINGS /etc/os-release _codename REGEX "^VERSION_CODENAME=")
    string(REGEX REPLACE "^VERSION_CODENAME=\"?([^\"]*)\"?$" "\\1" GUITARIQX_DEB_DISTRO "${_codename}")
endif()

set(CPACK_PACKAGE_NAME guitariqx)
set(CPACK_PACKAGE_VENDOR "Gaël de Chalendar")
set(CPACK_PACKAGE_CONTACT "Gaël de Chalendar <kleagg@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt Quick user interface for the guitarix engine")
set(CPACK_PACKAGE_DESCRIPTION
"guitariqx controls a guitarix engine (guitarix -N -p PORT) over its
JSON-RPC interface, on the same computer or over the network. It is made
for touch screens, such as the Raspberry Pi ones, as well as desktops.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/kleag/guitariqx")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/COPYING")
set(CPACK_STRIP_FILES ON)

set(CPACK_DEBIAN_PACKAGE_SECTION sound)
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
if(GUITARIQX_DEB_DISTRO)
    set(CPACK_DEBIAN_PACKAGE_RELEASE "1~${GUITARIQX_DEB_DISTRO}")
endif()
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# the Qt libraries come from dpkg-shlibdeps, the QML modules imported by
# qml/*.qml and the platform plugins do not
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-templates, qml6-module-qtquick-window, qml6-module-qtqml-workerscript, qt6-qpa-plugins")
# Wayland desktops (Raspberry Pi OS on a Pi 5)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "qt6-wayland")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "guitarix")

include(CPack)
