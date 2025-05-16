set(QT_MINIMUM_VERSION "5.7.1")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Qt5Core "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Gui "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Widgets "${QT_MINIMUM_VERSION}" REQUIRED)
find_package(Qt5Network "${QT_MINIMUM_VERSION}" REQUIRED)

# optional localization
find_package(Qt5LinguistTools QUIET)

# X11 for Linux/Unix
if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  find_package(Qt5X11Extras "${QT_MINIMUM_VERSION}" REQUIRED)
  find_package(X11 REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# for the old code; not needed anymore
#find_package(MAGIC_INCLUDE_DIR magic.h)
#find_library(MAGIC_LIBRARY magic $ENV{MAGIC_LIB_PATH})

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt5Core_INCLUDE_DIRS}
                      ${Qt5Gui_INCLUDE_DIRS}
                      ${Qt5Widgets_INCLUDE_DIRS}
                      ${Qt5X11Extras_INCLUDE_DIRS}
                      ${Qt5Network_INCLUDE_DIRS}
                      ${Qt5PrintSupport_INCLUDE_DIRS}
                      ${X11_INCLUDE_DIR})
  add_definitions(-DHAS_X11)
else()
  include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
                      ${Qt5Core_INCLUDE_DIRS}
                      ${Qt5Gui_INCLUDE_DIRS}
                      ${Qt5Widgets_INCLUDE_DIRS}
                      ${Qt5Network_INCLUDE_DIRS}
                      ${Qt5PrintSupport_INCLUDE_DIRS})
endif()

set(fpad_SRCS
    main.cc
    singleton.cc
    fpwin.cc
    encoding.cc
    tabwidget.cc
    lineedit.cc
    textedit.cc
    tabbar.cc
    find.cc
    replace.cc
    pref.cc
    config.cc
    vscrollbar.cc
    loading.cc
    tabpage.cc
    searchbar.cc
    session.cc
    fontDialog.cc)

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  set(fpad_SRCS ${fpad_SRCS} x11.cc)
endif()

if(CMAKE_VERSION VERSION_LESS "3.10.0") # CMP0071 in 3.10: Let AUTOMOC and AUTOUIC process GENERATED files
  qt5_wrap_cc(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h)
else()
  set(EXTRA_HEADERS warningbar.h messagebox.h filedialog.h)
endif()

add_executable(fpad ${fpad_SRCS} ${fpad_RESOURCES} ${EXTRA_HEADERS})

if(NOT WITHOUT_X11 AND UNIX AND NOT APPLE AND NOT HAIKU)
  target_link_libraries(fpad ${Qt5Core_LIBRARIES}
                                   ${Qt5Gui_LIBRARIES}
                                   ${Qt5Widgets_LIBRARIES}
                                   ${Qt5X11Extras_LIBRARIES}
                                   ${Qt5Network_LIBRARIES}
                                   ${Qt5PrintSupport_LIBRARIES}
                                   ${X11_LIBRARIES})
else()
  target_link_libraries(fpad ${Qt5Core_LIBRARIES}
                                   ${Qt5Gui_LIBRARIES}
                                   ${Qt5Widgets_LIBRARIES}
                                   ${Qt5Network_LIBRARIES}
                                   ${Qt5PrintSupport_LIBRARIES})
endif()

# installation
if(HAIKU)
  install(TARGETS fpad RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
elseif(APPLE)
  SET_TARGET_PROPERTIES(fpad PROPERTIES MACOSX_BUNDLE TRUE)
  INSTALL(TARGETS fpad DESTINATION ${CMAKE_INSTALL_PREFIX})
elseif(UNIX)
  install(TARGETS fpad RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
