#-------------------------------------------------
#
# Project created by QtCreator 2013-01-30T20:09:47
#
#-------------------------------------------------

QT       -= gui

TARGET = XboxInternals
TEMPLATE = lib
DEFINES += XBOXINTERNALS_LIBRARY


unix {
    CONFIG += staticlib app_bundle
}

macx {
    QMAKE_CFLAGS_X86_64 += -mmacosx-version-min=10.7
    QMAKE_CXXFLAGS_X86_64 = $$QMAKE_CFLAGS_X86_64
}

# flags
#QMAKE_CXXFLAGS += -std=c++11
QMAKE_CFLAGS_RELEASE = -O3

# library version
#VERSION = 0.1.0.0
#DEFINES += VERSION=\\\"$$VERSION\\\"

win32 {
    include(Stfs/Botan.pri)
    LIBS += C:/botan/libbotan-1.10.a "C:\\Program Files (x86)\\Microsoft SDKs\\Windows\\v7.0A\\Lib\\ShLwApi.Lib"
    PRE_TARGETDEPS += C:/botan/libbotan-1.10.a
    INCLUDEPATH += C:/botan/include
}
macx|unix {
    INCLUDEPATH += /usr/local/Cellar/botan/2.8.0/include/botan-2/
    # INCLUDEPATH += /usr/local/include/botan-1.10
    # LIBS += /usr/local/lib/libbotan-1.10.a
    LIBS += /usr/local/lib/libbotan-2.dylib
}

SOURCES += \
    AvatarAsset/AssetHelpers.cpp \
    Gpd/XdbfHelpers.cpp \
    AvatarAsset/AvatarAsset.cpp \
    Gpd/AvatarAwardGpd.cpp \
    Account/Account.cpp \
    Disc/Gdfx.cpp \
    Disc/Svod.cpp \
    IO/BaseIO.cpp \
    IO/SvodIO.cpp \
    Stfs/StfsDefinitions.cpp \
    Stfs/StfsPackage.cpp \
    Stfs/XContentHeader.cpp \
    Account/AccountHelpers.cpp \
    Gpd/DashboardGpd.cpp \
    Gpd/GpdBase.cpp \
    Gpd/Xdbf.cpp \
    Gpd/GameGpd.cpp \
    IO/MemoryIO.cpp \
    IO/DeviceIO.cpp \
    Cryptography/XeKeys.cpp \
    Cryptography/XeCrypt.cpp \
    AvatarAsset/Ytgr.cpp \
    Fatx/FatxHelpers.cpp \
    Fatx/FatxDrive.cpp \
    IO/FileIO.cpp \
    IO/FatxIO.cpp \
    Fatx/FatxDriveDetection.cpp \
    IO/StfsIO.cpp \
    Fatx/XContentDevice.cpp \
    Fatx/XContentDeviceProfile.cpp \
    Fatx/XContentDeviceItem.cpp \
    Fatx/XContentDeviceTitle.cpp \
    Fatx/XContentDeviceSharedItem.cpp \
    IO/IndexableMultiFileIO.cpp \
    IO/JoinedMultiFileIO.cpp \
    IO/LocalIndexableMultiFileIO.cpp \
    IO/FatxIndexableMultiFileIO.cpp \
    Utils.cpp \
    Stfs/IXContentHeader.cpp \
    Disc/ISO.cpp \
    IO/BigFileIO.cpp \
    IO/IsoIO.cpp \
    Xex/Xex.cpp \
    Xex/Xuiz.cpp \
    IO/XexZeroBasedCompressionIO.cpp \
    IO/XexAesIO.cpp \
    IO/XexBaseIO.cpp

HEADERS +=\
        XboxInternals_global.h \
    Account/AccountHelpers.h \
    AvatarAsset/AssetHelpers.h \
    AvatarAsset/AvatarAssetDefinintions.h \
    AvatarAsset/AvatarAsset.h \
    Gpd/AvatarAwardGpd.h \
    Gpd/DashboardGpd.h \
    Gpd/XdbfHelpers.h \
    Gpd/XdbfDefininitions.h \
    Account/Account.h \
    winnames.h \
    Gpd/GpdBase.h \
    Gpd/Xdbf.h \
    Disc/Gdfx.h \
    Disc/Svod.h \
    Gpd/GameGpd.h \
    IO/BaseIO.h \
    IO/SvodIO.h \
    Stfs/StfsConstants.h \
    Stfs/StfsDefinitions.h \
    Stfs/StfsPackage.h \
    Stfs/XContentHeader.h \
    Account/AccountDefinitions.h \
    IO/MemoryIO.h \
    IO/DeviceIO.h \
    Cryptography/XeKeys.h \
    Cryptography/XeCrypt.h \
    AvatarAsset/Ytgr.h \
    Fatx/FatxHelpers.h \
    Fatx/FatxDrive.h \
    Fatx/FatxConstants.h \
    IO/FileIO.h \
    IO/FatxIO.h \
    Fatx/FatxDriveDetection.h \
    Fatx/FatxHelpers.h \
    Fatx/FatxDriveDetection.h \
    Fatx/FatxDrive.h \
    Fatx/FatxConstants.h \
    Fatx/XContentDevice.h \
    Fatx/XContentDeviceProfile.h \
    Fatx/XContentDeviceItem.h \
    Fatx/XContentDeviceTitle.h \
    Fatx/XContentDeviceSharedItem.h \
    Stfs/IXContentHeader.h \
    IO/IndexableMultiFileIO.h \
    IO/JoinedMultiFileIO.h \
    IO/LocalIndexableMultiFileIO.h \
    IO/FatxIndexableMultiFileIO.h \
    Utils.h \
    Disc/ISO.h \
    IO/BigFileIO.h \
    IO/IsoIO.h \
    Xex/Xex.h \
    Xex/XexDefinitions.h \
    Xex/Xuiz.h \
    IO/XexZeroBasedCompressionIO.h \
    IO/XexAesIO.h \
    IO/XexBaseIO.h
