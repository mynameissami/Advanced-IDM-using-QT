cmake_minimum_required(VERSION 3.16)

project(InternetDownloadManager VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/ui)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Concurrent)

set(PROJECT_SOURCES
    src/core/main.cpp
    src/core/mainwindow.cpp
    src/core/mainwindow.h
    ui/mainwindow.ui
    ui/PreferencesDialog.ui
    ui/about.ui
    ui/connectionsettingsdialog.ui
    ui/downloadconfirmationdialog.ui
    ui/downloaddetailsdialog.ui
    ui/newdownloaddialog.ui
    ui/speedlimiterdialog.ui
)

set(DIALOG_SOURCES
    src/dialogs/PreferencesDialog.cpp
    src/dialogs/PreferencesDialog.h
    ui/PreferencesDialog.ui
    src/dialogs/about.h
    src/dialogs/aboutdialog.cpp
    ui/about.ui
    src/dialogs/connectionsettingsdialog.cpp
    src/dialogs/connectionsettingsdialog.h
    ui/connectionsettingsdialog.ui
    src/dialogs/downloadconfirmationdialog.cpp
    src/dialogs/downloadconfirmationdialog.h
    ui/downloadconfirmationdialog.ui
    src/dialogs/downloaddetailsdialog.cpp
    src/dialogs/downloaddetailsdialog.h
    ui/downloaddetailsdialog.ui
    src/dialogs/newdownloaddialog.cpp
    src/dialogs/newdownloaddialog.h
    ui/newdownloaddialog.ui
    src/dialogs/speedlimiterdialog.cpp
    src/dialogs/speedlimiterdialog.h
    ui/speedlimiterdialog.ui
    src/dialogs/youtubedownloaddialog.cpp
    src/dialogs/youtubedownloaddialog.h
)

set(NETWORK_SOURCES
    src/network/downloaditem.cpp
    src/network/downloaditem.h
    src/network/downloadmanager.cpp
    src/network/downloadmanager.h
)

set(UTILS_SOURCES
    src/utils/utils.h
)

set(PROJECT_SOURCES
    ${PROJECT_SOURCES}
    ${DIALOG_SOURCES}
    ${NETWORK_SOURCES}
    ${UTILS_SOURCES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(InternetDownloadManager
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(InternetDownloadManager SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(InternetDownloadManager
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(InternetDownloadManager PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Concurrent
)

target_include_directories(InternetDownloadManager PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.InternetDownloadManager)
endif()
set_target_properties(InternetDownloadManager PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS InternetDownloadManager
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(InternetDownloadManager)
endif()
