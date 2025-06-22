cmake_minimum_required(VERSION 3.16)

project(InternetDownloadManager VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Concurrent)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(InternetDownloadManager
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        downloaditem.h
        connectionsettingsdialog.h
        connectionsettingsdialog.cpp
        connectionsettingsdialog.ui
        downloaditem.cpp
        downloadmanager.h
        downloadmanager.cpp
        speedlimiterdialog.ui
        speedlimiterdialog.h
        speedlimiterdialog.cpp
        PreferencesDialog.h
        PreferencesDialog.ui
        PreferencesDialog.cpp
        about.ui
        about.h
        aboutdialog.cpp
        youtubedownloaddialog.h
        youtubedownloaddialog.cpp
        downloadconfirmationdialog.h
        downloadconfirmationdialog.cpp
        downloadconfirmationdialog.ui
        downloaddetailsdialog.ui
        downloaddetailsdialog.h
        downloaddetailsdialog.cpp
        utils.h
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
