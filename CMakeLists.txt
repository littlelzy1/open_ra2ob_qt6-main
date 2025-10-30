cmake_minimum_required(VERSION 3.5)

project(ra2_ob VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Network WebSockets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Network WebSockets)

set(TS_FILES open_ra2ob_qt6_zh_CN.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        logo.rc
        ${TS_FILES}
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Ra2ob)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/QHotkey)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ra2_ob
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        resources.qrc
        ob.h ob.cpp
        obteam.h obteam.cpp
        ob3.h ob3.cpp
        ob1.h ob1.cpp
        configmanager.h configmanager.cpp
        unitblock.h unitblock.cpp unitblock.ui
        playerinfo.h playerinfo.cpp playerinfo.ui
        layoutsetting.h
        messagebox.h
        globalsetting.h globalsetting.cpp
        producingblock.h producingblock.cpp producingblock.ui
        qoutlinelabel.h qoutlinelabel.cpp
        hotkeymanager.h hotkeymanager.cpp
        updatechecker.h updatechecker.cpp
        updatedialog.h updatedialog.cpp
        updatedownloader.h updatedownloader.cpp
        authmanager.h authmanager.cpp
        membermanager.h membermanager.cpp
        playerstatustracker.h playerstatustracker.cpp
        buildingdetector.h buildingdetector.cpp
        theftalertmanager.h theftalertmanager.cpp
        mainwindow_ext.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET ra2ob APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(ra2_ob SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(ra2_ob
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(ra2_ob PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::WebSockets)

# 对于Windows平台，添加COM相关库
if(WIN32)
    target_link_libraries(ra2_ob PRIVATE wbemuuid oleaut32 ole32)
    if(MSVC)
        target_link_libraries(ra2_ob PRIVATE comsuppw)
    endif()
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.ra2_ob)
endif()
set_target_properties(ra2_ob PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    #LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS"
)

include(GNUInstallDirs)
install(TARGETS ra2_ob
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(ra2_ob)
endif()
