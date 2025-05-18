QT += core gui \
      widgets \
      network

haiku|macx {
  TARGET = fpad
}
else {
  TARGET = fpad
}

TEMPLATE = app
CONFIG += c++11

SOURCES += main.cc \
           singleton.cc \
           fpwin.cc \
           encoding.cc \
           tabwidget.cc \
           lineedit.cc \
           textedit.cc \
           tabbar.cc \
           find.cc \
           replace.cc \
           pref.cc \
           config.cc \
           vscrollbar.cc \
           loading.cc \
           tabpage.cc \
           searchbar.cc \
           fontDialog.cc

HEADERS += singleton.h \
           fpwin.h \
           encoding.h \
           tabwidget.h \
           lineedit.h \
           textedit.h \
           tabbar.h \
           vscrollbar.h \
           filedialog.h \
           config.h \
           pref.h \
           loading.h \
           messagebox.h \
           tabpage.h \
           searchbar.h \
           fontDialog.h \
           warningbar.h

FORMS += fp.ui \
         prefDialog.ui \
         fontDialog.ui

unix:!haiku:!macx {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr/local
  }
  BINDIR = $$PREFIX/bin

  #MAKE INSTALL

  target.path =$$BINDIR

  INSTALLS += target
}
else:haiku {
  isEmpty(PREFIX) {
    PREFIX = /boot/home/config/non-packaged/apps/fpad
  }
  BINDIR = $$PREFIX

  target.path =$$BINDIR

  INSTALLS += target
}
else:macx{
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /Applications
  }
  BINDIR = $$PREFIX

  #MAKE INSTALL

  target.path =$$BINDIR

  INSTALLS += target
}
