QT += core gui \
      widgets \
      printsupport \
      network \
      svg

haiku|macx {
  TARGET = FeatherPad
}
else {
  TARGET = featherpad
}

TEMPLATE = app
CONFIG += c++11

SOURCES += main.cpp \
           singleton.cpp \
           fpwin.cpp \
           encoding.cpp \
           tabwidget.cpp \
           lineedit.cpp \
           textedit.cpp \
           tabbar.cpp \
           highlighter.cpp \
           find.cpp \
           replace.cpp \
           pref.cpp \
           config.cpp \
           brackets.cpp \
           syntax.cpp \
           highlighter-sh.cpp \
           highlighter-css.cpp \
           highlighter-html.cpp \
           highlighter-patterns.cpp \
           highlighter-regex.cpp \
           highlighter-perl-regex.cpp \
           vscrollbar.cpp \
           loading.cpp \
           tabpage.cpp \
           searchbar.cpp \
           session.cpp \
           fontDialog.cpp \
           svgicons.cpp

HEADERS += singleton.h \
           fpwin.h \
           encoding.h \
           tabwidget.h \
           lineedit.h \
           textedit.h \
           tabbar.h \
           highlighter.h \
           vscrollbar.h \
           filedialog.h \
           config.h \
           pref.h \
           loading.h \
           messagebox.h \
           tabpage.h \
           searchbar.h \
           session.h \
           fontDialog.h \
           warningbar.h \
           svgicons.h

FORMS += fp.ui \
         prefDialog.ui \
         sessionDialog.ui \
         fontDialog.ui

RESOURCES += data/fp.qrc

contains(WITHOUT_X11, YES) {
  message("Compiling without X11...")
}
else:unix:!macx:!haiku {
  QT += x11extras
  SOURCES += x11.cpp
  HEADERS += x11.h
  LIBS += -lX11
  DEFINES += HAS_X11
}

unix:!haiku:!macx {
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /usr
  }
  BINDIR = $$PREFIX/bin
  DATADIR =$$PREFIX/share

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  #MAKE INSTALL

  target.path =$$BINDIR

  # add the fpad symlink
  slink.path = $$BINDIR
  slink.extra += ln -sf $${TARGET} fpad && cp -P fpad $(INSTALL_ROOT)$$BINDIR

  desktop.path = $$DATADIR/applications
  desktop.files += ./data/$${TARGET}.desktop

  iconsvg.path = $$DATADIR/icons/hicolor/scalable/apps
  iconsvg.files += ./data/$${TARGET}.svg

  INSTALLS += target slink desktop iconsvg
}
else:haiku {
  isEmpty(PREFIX) {
    PREFIX = /boot/home/config/non-packaged/apps/FeatherPad
  }
  BINDIR = $$PREFIX
  DATADIR =$$PREFIX/data

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  target.path =$$BINDIR

  INSTALLS += target
}
else:macx{
  #VARIABLES
  isEmpty(PREFIX) {
    PREFIX = /Applications
  }
  BINDIR = $$PREFIX
  DATADIR = "$$BINDIR/$$TARGET".app

  DEFINES += DATADIR=\\\"$$DATADIR\\\" PKGDATADIR=\\\"$$PKGDATADIR\\\"

  #MAKE INSTALL

  target.path =$$BINDIR

  INSTALLS += target
}
