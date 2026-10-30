TARGET = backupapp

CONFIG += sailfishapp

QT += network xml

INCLUDEPATH += qwebdavlib/qwebdavlib
DEFINES += QWEBDAV_LIBRARY

SOURCES += src/backupapp.cpp \
    src/backupservice.cpp \
    src/restoreservice.cpp \
    src/settings.cpp \
    src/storageinfoprovider.cpp

# qwebdavlib
SOURCES += qwebdavlib/qwebdavlib/qwebdav.cpp \
    qwebdavlib/qwebdavlib/qwebdavitem.cpp \
    qwebdavlib/qwebdavlib/qwebdavdirparser.cpp \
    qwebdavlib/qwebdavlib/qnaturalsort.cpp

DISTFILES += qml/backupapp.qml \
    qml/cover/CoverPage.qml \
    qml/pages/Main.qml \
    qml/pages/ProfileSettings.qml \
    qml/pages/NewProfileDialog.qml \
    qml/pages/Settings.qml \
    qml/pages/Restore.qml \\
    qml/pages/About.qml \
    rpm/backupapp.spec \
    translations/*.ts \
    backupapp.desktop

#qwebdavlib
HEADERS += qwebdavlib/qwebdavlib/qwebdav.h \
    qwebdavlib/qwebdavlib/qwebdavitem.h \
    qwebdavlib/qwebdavlib/qwebdavdirparser.h \
    qwebdavlib/qwebdavlib/qnaturalsort.h \
    qwebdavlib/qwebdavlib/qwebdav_global.h \
    src/restoreservice.h \
    src/settings.h \
    src/storageinfoprovider.h

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

CONFIG += sailfishapp_i18n

TRANSLATIONS += translations/nanofiles.ts \
                translations/nanofiles-de.ts \
                translations/nanofiles-pl.ts \
                translations/nanofiles-it.ts \
                translations/nanofiles-es.ts

HEADERS += \
    src/backupservice.h

profileconf.files = profile.conf.example full-webdav.conf home-rsync.conf home-webdav.conf
profileconf.path = /etc/backupapp

INSTALLS += profileconf

oneshot.files = backupapp-swap-rootfs.sh
oneshot.path = /usr/lib/oneshot.d

INSTALLS += oneshot

swaphome.files = backupapp-swap-home.sh backupapp-swap-home.service
swaphome.path = /usr/share/$${TARGET}

INSTALLS += swaphome
