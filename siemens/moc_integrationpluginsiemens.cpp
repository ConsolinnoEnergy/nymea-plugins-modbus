/****************************************************************************
** Meta object code from reading C++ file 'integrationpluginsiemens.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "integrationpluginsiemens.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qplugin.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'integrationpluginsiemens.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_IntegrationPluginSiemens_t {
    QByteArrayData data[1];
    char stringdata0[25];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_IntegrationPluginSiemens_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_IntegrationPluginSiemens_t qt_meta_stringdata_IntegrationPluginSiemens = {
    {
QT_MOC_LITERAL(0, 0, 24) // "IntegrationPluginSiemens"

    },
    "IntegrationPluginSiemens"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_IntegrationPluginSiemens[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       0,    0, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

       0        // eod
};

void IntegrationPluginSiemens::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    Q_UNUSED(_o);
    Q_UNUSED(_id);
    Q_UNUSED(_c);
    Q_UNUSED(_a);
}

QT_INIT_METAOBJECT const QMetaObject IntegrationPluginSiemens::staticMetaObject = { {
    QMetaObject::SuperData::link<IntegrationPlugin::staticMetaObject>(),
    qt_meta_stringdata_IntegrationPluginSiemens.data,
    qt_meta_data_IntegrationPluginSiemens,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *IntegrationPluginSiemens::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *IntegrationPluginSiemens::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_IntegrationPluginSiemens.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "io.nymea.IntegrationPlugin"))
        return static_cast< IntegrationPlugin*>(this);
    return IntegrationPlugin::qt_metacast(_clname);
}

int IntegrationPluginSiemens::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = IntegrationPlugin::qt_metacall(_c, _id, _a);
    return _id;
}

QT_PLUGIN_METADATA_SECTION
static constexpr unsigned char qt_pluginMetaData[] = {
    'Q', 'T', 'M', 'E', 'T', 'A', 'D', 'A', 'T', 'A', ' ', '!',
    // metadata version, Qt version, architectural requirements
    0, QT_VERSION_MAJOR, QT_VERSION_MINOR, qPluginArchRequirements(),
    0xbf, 
    // "IID"
    0x02,  0x78,  0x1a,  'i',  'o',  '.',  'n',  'y', 
    'm',  'e',  'a',  '.',  'I',  'n',  't',  'e', 
    'g',  'r',  'a',  't',  'i',  'o',  'n',  'P', 
    'l',  'u',  'g',  'i',  'n', 
    // "className"
    0x03,  0x78,  0x18,  'I',  'n',  't',  'e',  'g', 
    'r',  'a',  't',  'i',  'o',  'n',  'P',  'l', 
    'u',  'g',  'i',  'n',  'S',  'i',  'e',  'm', 
    'e',  'n',  's', 
    // "MetaData"
    0x04,  0xa4,  0x6b,  'd',  'i',  's',  'p',  'l', 
    'a',  'y',  'N',  'a',  'm',  'e',  0x67,  'S', 
    'i',  'e',  'm',  'e',  'n',  's',  0x62,  'i', 
    'd',  0x78,  0x24,  '2',  'a',  '7',  'b',  '1', 
    'c',  '5',  '6',  '-',  'd',  '5',  'c',  'd', 
    '-',  '4',  'e',  '3',  'b',  '-',  '9',  'a', 
    '8',  'f',  '-',  '5',  'f',  '3',  'c',  '2', 
    'd',  '9',  'e',  '8',  'f',  'b',  'a',  0x64, 
    'n',  'a',  'm',  'e',  0x67,  's',  'i',  'e', 
    'm',  'e',  'n',  's',  0x67,  'v',  'e',  'n', 
    'd',  'o',  'r',  's',  0x81,  0xa4,  0x6b,  'd', 
    'i',  's',  'p',  'l',  'a',  'y',  'N',  'a', 
    'm',  'e',  0x67,  'S',  'i',  'e',  'm',  'e', 
    'n',  's',  0x62,  'i',  'd',  0x78,  0x24,  '3', 
    'f',  '8',  'c',  '5',  'b',  '9',  '2',  '-', 
    '1',  'a',  '8',  '3',  '-',  '4',  'd',  'b', 
    '4',  '-',  'b',  'c',  '2',  '9',  '-',  '1', 
    '5',  'f',  'e',  '0',  '5',  'e',  '7',  '3', 
    '5',  '4',  '2',  0x64,  'n',  'a',  'm',  'e', 
    0x67,  's',  'i',  'e',  'm',  'e',  'n',  's', 
    0x6c,  't',  'h',  'i',  'n',  'g',  'C',  'l', 
    'a',  's',  's',  'e',  's',  0x81,  0xa8,  0x6d, 
    'c',  'r',  'e',  'a',  't',  'e',  'M',  'e', 
    't',  'h',  'o',  'd',  's',  0x81,  0x69,  'd', 
    'i',  's',  'c',  'o',  'v',  'e',  'r',  'y', 
    0x73,  'd',  'i',  's',  'c',  'o',  'v',  'e', 
    'r',  'y',  'P',  'a',  'r',  'a',  'm',  'T', 
    'y',  'p',  'e',  's',  0x81,  0xa5,  0x6c,  'd', 
    'e',  'f',  'a',  'u',  'l',  't',  'V',  'a', 
    'l',  'u',  'e',  0x01,  0x6b,  'd',  'i',  's', 
    'p',  'l',  'a',  'y',  'N',  'a',  'm',  'e', 
    0x6d,  'S',  'l',  'a',  'v',  'e',  ' ',  'a', 
    'd',  'd',  'r',  'e',  's',  's',  0x62,  'i', 
    'd',  0x78,  0x24,  'b',  'f',  '8',  '3',  '4', 
    'd',  'd',  '7',  '-',  'f',  'f',  '4',  'c', 
    '-',  '4',  'f',  '4',  '9',  '-',  'b',  '9', 
    '9',  '7',  '-',  '7',  '9',  'b',  '8',  '6', 
    '0',  'a',  'f',  '1',  '0',  'a',  'd',  0x64, 
    'n',  'a',  'm',  'e',  0x6c,  's',  'l',  'a', 
    'v',  'e',  'A',  'd',  'd',  'r',  'e',  's', 
    's',  0x64,  't',  'y',  'p',  'e',  0x63,  'i', 
    'n',  't',  0x6b,  'd',  'i',  's',  'p',  'l', 
    'a',  'y',  'N',  'a',  'm',  'e',  0x77,  'S', 
    'i',  'e',  'm',  'e',  'n',  's',  ' ',  'P', 
    'A',  'C',  ' ',  'E',  'n',  'e',  'r',  'g', 
    'y',  'm',  'e',  't',  'e',  'r',  0x62,  'i', 
    'd',  0x78,  0x24,  '5',  'a',  '1',  'e',  '7', 
    '7',  'd',  'd',  '-',  '2',  'd',  '4',  'e', 
    '-',  '4',  '7',  '1',  'e',  '-',  '8',  'e', 
    'f',  '8',  '-',  'e',  'e',  '2',  '5',  '5', 
    'a',  '0',  '9',  'a',  '7',  'd',  'b',  0x6a, 
    'i',  'n',  't',  'e',  'r',  'f',  'a',  'c', 
    'e',  's',  0x82,  0x6b,  'e',  'n',  'e',  'r', 
    'g',  'y',  'm',  'e',  't',  'e',  'r',  0x6b, 
    'c',  'o',  'n',  'n',  'e',  'c',  't',  'a', 
    'b',  'l',  'e',  0x64,  'n',  'a',  'm',  'e', 
    0x6b,  'e',  'n',  'e',  'r',  'g',  'y',  'm', 
    'e',  't',  'e',  'r',  0x6a,  'p',  'a',  'r', 
    'a',  'm',  'T',  'y',  'p',  'e',  's',  0x82, 
    0xa5,  0x6c,  'd',  'e',  'f',  'a',  'u',  'l', 
    't',  'V',  'a',  'l',  'u',  'e',  0x01,  0x6b, 
    'd',  'i',  's',  'p',  'l',  'a',  'y',  'N', 
    'a',  'm',  'e',  0x74,  'M',  'o',  'd',  'b', 
    'u',  's',  ' ',  's',  'l',  'a',  'v',  'e', 
    ' ',  'a',  'd',  'd',  'r',  'e',  's',  's', 
    0x62,  'i',  'd',  0x78,  0x24,  '4',  '3',  'c', 
    'd',  '8',  '8',  '7',  'e',  '-',  'd',  'd', 
    '1',  'e',  '-',  '4',  '6',  '2',  'f',  '-', 
    '9',  '8',  '7',  '2',  '-',  'f',  '3',  '5', 
    '8',  'd',  '5',  '7',  'f',  '9',  'd',  '5', 
    '3',  0x64,  'n',  'a',  'm',  'e',  0x6c,  's', 
    'l',  'a',  'v',  'e',  'A',  'd',  'd',  'r', 
    'e',  's',  's',  0x64,  't',  'y',  'p',  'e', 
    0x64,  'u',  'i',  'n',  't',  0xa6,  0x6c,  'd', 
    'e',  'f',  'a',  'u',  'l',  't',  'V',  'a', 
    'l',  'u',  'e',  0x60,  0x6b,  'd',  'i',  's', 
    'p',  'l',  'a',  'y',  'N',  'a',  'm',  'e', 
    0x71,  'M',  'o',  'd',  'b',  'u',  's',  ' ', 
    'R',  'T',  'U',  ' ',  'm',  'a',  's',  't', 
    'e',  'r',  0x62,  'i',  'd',  0x78,  0x24,  'e', 
    '8',  'f',  '9',  '6',  '5',  '5',  '4',  '-', 
    'f',  '5',  '9',  '4',  '-',  '4',  '9',  '6', 
    '6',  '-',  'b',  'f',  'd',  '8',  '-',  'f', 
    'e',  '9',  'b',  '7',  'a',  'd',  '2',  'b', 
    'c',  '9',  '6',  0x64,  'n',  'a',  'm',  'e', 
    0x70,  'm',  'o',  'd',  'b',  'u',  's',  'M', 
    'a',  's',  't',  'e',  'r',  'U',  'u',  'i', 
    'd',  0x68,  'r',  'e',  'a',  'd',  'O',  'n', 
    'l',  'y',  0xf5,  0x64,  't',  'y',  'p',  'e', 
    0x65,  'Q',  'U',  'u',  'i',  'd',  0x6a,  's', 
    't',  'a',  't',  'e',  'T',  'y',  'p',  'e', 
    's',  0x8b,  0xa7,  0x66,  'c',  'a',  'c',  'h', 
    'e',  'd',  0xf4,  0x6c,  'd',  'e',  'f',  'a', 
    'u',  'l',  't',  'V',  'a',  'l',  'u',  'e', 
    0xf4,  0x6b,  'd',  'i',  's',  'p',  'l',  'a', 
    'y',  'N',  'a',  'm',  'e',  0x69,  'C',  'o', 
    'n',  'n',  'e',  'c',  't',  'e',  'd',  0x70, 
    'd',  'i',  's',  'p',  'l',  'a',  'y',  'N', 
    'a',  'm',  'e',  'E',  'v',  'e',  'n',  't', 
    0x71,  'C',  'o',  'n',  'n',  'e',  'c',  't', 
    'e',  'd',  ' ',  'c',  'h',  'a',  'n',  'g', 
    'e',  'd',  0x62,  'i',  'd',  0x78,  0x24,  '5', 
    '9',  '7',  'f',  '2',  'f',  '1',  '6',  '-', 
    '3',  '8',  'f',  '6',  '-',  '4',  'a',  '0', 
    '2',  '-',  '9',  '7',  'c',  '5',  '-',  '5', 
    'b',  '6',  '1',  '1',  'e',  '6',  '7',  '3', 
    '8',  'f',  '8',  0x64,  'n',  'a',  'm',  'e', 
    0x69,  'c',  'o',  'n',  'n',  'e',  'c',  't', 
    'e',  'd',  0x64,  't',  'y',  'p',  'e',  0x64, 
    'b',  'o',  'o',  'l',  0xa7,  0x6c,  'd',  'e', 
    'f',  'a',  'u',  'l',  't',  'V',  'a',  'l', 
    'u',  'e',  0x00,  0x6b,  'd',  'i',  's',  'p', 
    'l',  'a',  'y',  'N',  'a',  'm',  'e',  0x6f, 
    'V',  'o',  'l',  't',  'a',  'g',  'e',  ' ', 
    'p',  'h',  'a',  's',  'e',  ' ',  'A',  0x70, 
    'd',  'i',  's',  'p',  'l',  'a',  'y',  'N', 
    'a',  'm',  'e',  'E',  'v',  'e',  'n',  't', 
    0x77,  'V',  'o',  'l',  't',  'a',  'g',  'e', 
    ' ',  'p',  'h',  'a',  's',  'e',  ' ',  'A', 
    ' ',  'c',  'h',  'a',  'n',  'g',  'e',  'd', 
    0x62,  'i',  'd',  0x78,  0x24,  '9',  '9',  'd', 
    '8',  '3',  'e',  '9',  'f',  '-',  'e',  '5', 
    '7',  '2',  '-',  '4',  'b',  '9',  'c',  '-', 
    '9',  'f',  '7',  'd',  '-',  '5',  'c',  '6', 
    'e',  '2',  'f',  '8',  'a',  '9',  'c',  '7', 
    'e',  0x64,  'n',  'a',  'm',  'e',  0x6d,  'v', 
    'o',  'l',  't',  'a',  'g',  'e',  'P',  'h', 
    'a',  's',  'e',  'A',  0x64,  't',  'y',  'p', 
    'e',  0x66,  'd',  'o',  'u',  'b',  'l',  'e', 
    0x64,  'u',  'n',  'i',  't',  0x64,  'V',  'o', 
    'l',  't',  0xa7,  0x6c,  'd',  'e',  'f',  'a', 
    'u',  'l',  't',  'V',  'a',  'l',  'u',  'e', 
    0x00,  0x6b,  'd',  'i',  's',  'p',  'l',  'a', 
    'y',  'N',  'a',  'm',  'e',  0x6f,  'V',  'o', 
    'l',  't',  'a',  'g',  'e',  ' ',  'p',  'h', 
    'a',  's',  'e',  ' ',  'B',  0x70,  'd',  'i', 
    's',  'p',  'l',  'a',  'y',  'N',  'a',  'm', 
    'e',  'E',  'v',  'e',  'n',  't',  0x77,  'V', 
    'o',  'l',  't',  'a',  'g',  'e',  ' ',  'p', 
    'h',  'a',  's',  'e',  ' ',  'B',  ' ',  'c', 
    'h',  'a',  'n',  'g',  'e',  'd',  0x62,  'i', 
    'd',  0x78,  0x24,  'a',  '1',  'f',  '4',  'c', 
    '9',  '9',  '9',  '-',  'b',  'd',  '1',  '9', 
    '-',  '4',  'b',  '8',  'd',  '-',  '9',  'c', 
    '8',  'f',  '-',  '6',  'd',  '7',  'f',  '3', 
    'e',  '9',  'a',  '8',  'c',  '8',  'f',  0x64, 
    'n',  'a',  'm',  'e',  0x6d,  'v',  'o',  'l', 
    't',  'a',  'g',  'e',  'P',  'h',  'a',  's', 
    'e',  'B',  0x64,  't',  'y',  'p',  'e',  0x66, 
    'd',  'o',  'u',  'b',  'l',  'e',  0x64,  'u', 
    'n',  'i',  't',  0x64,  'V',  'o',  'l',  't', 
    0xa7,  0x6c,  'd',  'e',  'f',  'a',  'u',  'l', 
    't',  'V',  'a',  'l',  'u',  'e',  0x00,  0x6b, 
    'd',  'i',  's',  'p',  'l',  'a',  'y',  'N', 
    'a',  'm',  'e',  0x6f,  'V',  'o',  'l',  't', 
    'a',  'g',  'e',  ' ',  'p',  'h',  'a',  's', 
    'e',  ' ',  'C',  0x70,  'd',  'i',  's',  'p', 
    'l',  'a',  'y',  'N',  'a',  'm',  'e',  'E', 
    'v',  'e',  'n',  't',  0x77,  'V',  'o',  'l', 
    't',  'a',  'g',  'e',  ' ',  'p',  'h',  'a', 
    's',  'e',  ' ',  'C',  ' ',  'c',  'h',  'a', 
    'n',  'g',  'e',  'd',  0x62,  'i',  'd',  0x78, 
    0x24,  'b',  '2',  'f',  '5',  'd',  '1',  '1', 
    '1',  '-',  'c',  'e',  '2',  'a',  '-',  '4', 
    'c',  '9',  'e',  '-',  '9',  'd',  '9',  'f', 
    '-',  '7',  'e',  '8',  'f',  '4',  'e',  '9', 
    'b',  '9',  'd',  '9',  'f',  0x64,  'n',  'a', 
    'm',  'e',  0x6d,  'v',  'o',  'l',  't',  'a', 
    'g',  'e',  'P',  'h',  'a',  's',  'e',  'C', 
    0x64,  't',  'y',  'p',  'e',  0x66,  'd',  'o', 
    'u',  'b',  'l',  'e',  0x64,  'u',  'n',  'i', 
    't',  0x64,  'V',  'o',  'l',  't',  0xa7,  0x6c, 
    'd',  'e',  'f',  'a',  'u',  'l',  't',  'V', 
    'a',  'l',  'u',  'e',  0x00,  0x6b,  'd',  'i', 
    's',  'p',  'l',  'a',  'y',  'N',  'a',  'm', 
    'e',  0x6f,  'C',  'u',  'r',  'r',  'e',  'n', 
    't',  ' ',  'p',  'h',  'a',  's',  'e',  ' ', 
    'A',  0x70,  'd',  'i',  's',  'p',  'l',  'a', 
    'y',  'N',  'a',  'm',  'e',  'E',  'v',  'e', 
    'n',  't',  0x77,  'C',  'u',  'r',  'r',  'e', 
    'n',  't',  ' ',  'p',  'h',  'a',  's',  'e', 
    ' ',  'A',  ' ',  'c',  'h',  'a',  'n',  'g', 
    'e',  'd',  0x62,  'i',  'd',  0x78,  0x24,  'c', 
    '3',  'f',  '6',  'e',  '2',  '2',  '2',  '-', 
    'd',  'f',  '3',  'b',  '-',  '4',  'd',  'a', 
    '0',  '-',  '9',  'e',  'a',  'f',  '-',  '8', 
    'f',  '9',  'f',  '5',  'e',  '9',  'c',  '9', 
    'e',  'a',  'f',  0x64,  'n',  'a',  'm',  'e', 
    0x6d,  'c',  'u',  'r',  'r',  'e',  'n',  't', 
    'P',  'h',  'a',  's',  'e',  'A',  0x64,  't', 
    'y',  'p',  'e',  0x66,  'd',  'o',  'u',  'b', 
    'l',  'e',  0x64,  'u',  'n',  'i',  't',  0x66, 
    'A',  'm',  'p',  'e',  'r',  'e',  0xa7,  0x6c, 
    'd',  'e',  'f',  'a',  'u',  'l',  't',  'V', 
    'a',  'l',  'u',  'e',  0x00,  0x6b,  'd',  'i', 
    's',  'p',  'l',  'a',  'y',  'N',  'a',  'm', 
    'e',  0x6f,  'C',  'u',  'r',  'r',  'e',  'n', 
    't',  ' ',  'p',  'h',  'a',  's',  'e',  ' ', 
    'B',  0x70,  'd',  'i',  's',  'p',  'l',  'a', 
    'y',  'N',  'a',  'm',  'e',  'E',  'v',  'e', 
    'n',  't',  0x77,  'C',  'u',  'r',  'r',  'e', 
    'n',  't',  ' ',  'p',  'h',  'a',  's',  'e', 
    ' ',  'B',  ' ',  'c',  'h',  'a',  'n',  'g', 
    'e',  'd',  0x62,  'i',  'd',  0x78,  0x24,  'd', 
    '4',  'f',  '7',  'f',  '3',  '3',  '3',  '-', 
    'e',  'f',  '4',  'c',  '-',  '4',  'e',  'b', 
    '1',  '-',  '9',  'f',  'b',  'f',  '-',  '9', 
    'f',  '0',  'f',  '6',  'f',  '9',  'd',  '9', 
    'f',  'b',  'f',  0x64,  'n',  'a',  'm',  'e', 
    0x6d,  'c',  'u',  'r',  'r',  'e',  'n',  't', 
    'P',  'h',  'a',  's',  'e',  'B',  0x64,  't', 
    'y',  'p',  'e',  0x66,  'd',  'o',  'u',  'b', 
    'l',  'e',  0x64,  'u',  'n',  'i',  't',  0x66, 
    'A',  'm',  'p',  'e',  'r',  'e',  0xa7,  0x6c, 
    'd',  'e',  'f',  'a',  'u',  'l',  't',  'V', 
    'a',  'l',  'u',  'e',  0x00,  0x6b,  'd',  'i', 
    's',  'p',  'l',  'a',  'y',  'N',  'a',  'm', 
    'e',  0x6f,  'C',  'u',  'r',  'r',  'e',  'n', 
    't',  ' ',  'p',  'h',  'a',  's',  'e',  ' ', 
    'C',  0x70,  'd',  'i',  's',  'p',  'l',  'a', 
    'y',  'N',  'a',  'm',  'e',  'E',  'v',  'e', 
    'n',  't',  0x77,  'C',  'u',  'r',  'r',  'e', 
    'n',  't',  ' ',  'p',  'h',  'a',  's',  'e', 
    ' ',  'C',  ' ',  'c',  'h',  'a',  'n',  'g', 
    'e',  'd',  0x62,  'i',  'd',  0x78,  0x24,  'e', 
    '5',  'f',  '8',  'f',  '4',  '4',  '4',  '-', 
    'f',  '0',  '5',  'd',  '-',  '4',  'f',  'c', 
    '2',  '-',  '9',  'f',  'c',  'f',  '-',  'a', 
    'f',  '1',  'f',  '7',  'f',  '0',  'f',  '9', 
    'f',  'c',  'f',  0x64,  'n',  'a',  'm',  'e', 
    0x6d,  'c',  'u',  'r',  'r',  'e',  'n',  't', 
    'P',  'h',  'a',  's',  'e',  'C',  0x64,  't', 
    'y',  'p',  'e',  0x66,  'd',  'o',  'u',  'b', 
    'l',  'e',  0x64,  'u',  'n',  'i',  't',  0x66, 
    'A',  'm',  'p',  'e',  'r',  'e',  0xa7,  0x6c, 
    'd',  'e',  'f',  'a',  'u',  'l',  't',  'V', 
    'a',  'l',  'u',  'e',  0x00,  0x6b,  'd',  'i', 
    's',  'p',  'l',  'a',  'y',  'N',  'a',  'm', 
    'e',  0x6d,  'C',  'u',  'r',  'r',  'e',  'n', 
    't',  ' ',  'p',  'o',  'w',  'e',  'r',  0x70, 
    'd',  'i',  's',  'p',  'l',  'a',  'y',  'N', 
    'a',  'm',  'e',  'E',  'v',  'e',  'n',  't', 
    0x75,  'C',  'u',  'r',  'r',  'e',  'n',  't', 
    ' ',  'p',  'o',  'w',  'e',  'r',  ' ',  'c', 
    'h',  'a',  'n',  'g',  'e',  'd',  0x62,  'i', 
    'd',  0x78,  0x24,  'f',  '6',  'f',  '9',  'f', 
    '5',  '5',  '5',  '-',  '0',  '1',  '5',  'e', 
    '-',  '5',  '0',  'd',  '3',  '-',  '9',  'f', 
    'd',  'f',  '-',  'b',  'f',  '2',  'f',  '8', 
    'f',  '1',  'f',  '9',  'f',  'd',  'f',  0x64, 
    'n',  'a',  'm',  'e',  0x6c,  'c',  'u',  'r', 
    'r',  'e',  'n',  't',  'P',  'o',  'w',  'e', 
    'r',  0x64,  't',  'y',  'p',  'e',  0x66,  'd', 
    'o',  'u',  'b',  'l',  'e',  0x64,  'u',  'n', 
    'i',  't',  0x64,  'W',  'a',  't',  't',  0xa7, 
    0x6c,  'd',  'e',  'f',  'a',  'u',  'l',  't', 
    'V',  'a',  'l',  'u',  'e',  0x00,  0x6b,  'd', 
    'i',  's',  'p',  'l',  'a',  'y',  'N',  'a', 
    'm',  'e',  0x69,  'F',  'r',  'e',  'q',  'u', 
    'e',  'n',  'c',  'y',  0x70,  'd',  'i',  's', 
    'p',  'l',  'a',  'y',  'N',  'a',  'm',  'e', 
    'E',  'v',  'e',  'n',  't',  0x71,  'F',  'r', 
    'e',  'q',  'u',  'e',  'n',  'c',  'y',  ' ', 
    'c',  'h',  'a',  'n',  'g',  'e',  'd',  0x62, 
    'i',  'd',  0x78,  0x24,  '0',  '7',  'f',  '0', 
    'f',  '6',  '6',  '6',  '-',  '1',  '2',  '5', 
    'f',  '-',  '6',  '0',  'e',  '4',  '-',  '9', 
    'f',  'e',  'f',  '-',  'c',  'f',  '3',  'f', 
    '9',  'f',  '2',  'f',  '9',  'f',  'e',  'f', 
    0x64,  'n',  'a',  'm',  'e',  0x69,  'f',  'r', 
    'e',  'q',  'u',  'e',  'n',  'c',  'y',  0x64, 
    't',  'y',  'p',  'e',  0x66,  'd',  'o',  'u', 
    'b',  'l',  'e',  0x64,  'u',  'n',  'i',  't', 
    0x65,  'H',  'e',  'r',  't',  'z',  0xa7,  0x6c, 
    'd',  'e',  'f',  'a',  'u',  'l',  't',  'V', 
    'a',  'l',  'u',  'e',  0x00,  0x6b,  'd',  'i', 
    's',  'p',  'l',  'a',  'y',  'N',  'a',  'm', 
    'e',  0x75,  'T',  'o',  't',  'a',  'l',  ' ', 
    'e',  'n',  'e',  'r',  'g',  'y',  ' ',  'c', 
    'o',  'n',  's',  'u',  'm',  'e',  'd',  0x70, 
    'd',  'i',  's',  'p',  'l',  'a',  'y',  'N', 
    'a',  'm',  'e',  'E',  'v',  'e',  'n',  't', 
    0x78,  0x1d,  'T',  'o',  't',  'a',  'l',  ' ', 
    'e',  'n',  'e',  'r',  'g',  'y',  ' ',  'c', 
    'o',  'n',  's',  'u',  'm',  'e',  'd',  ' ', 
    'c',  'h',  'a',  'n',  'g',  'e',  'd',  0x62, 
    'i',  'd',  0x78,  0x24,  '1',  '8',  'f',  '1', 
    'f',  '7',  '7',  '7',  '-',  '2',  '3',  '6', 
    '0',  '-',  '7',  '1',  'f',  '5',  '-',  '9', 
    'f',  'f',  'f',  '-',  'd',  'f',  '4',  'f', 
    '0',  'f',  '3',  'f',  '9',  'f',  'f',  'f', 
    0x64,  'n',  'a',  'm',  'e',  0x73,  't',  'o', 
    't',  'a',  'l',  'E',  'n',  'e',  'r',  'g', 
    'y',  'C',  'o',  'n',  's',  'u',  'm',  'e', 
    'd',  0x64,  't',  'y',  'p',  'e',  0x66,  'd', 
    'o',  'u',  'b',  'l',  'e',  0x64,  'u',  'n', 
    'i',  't',  0x6c,  'K',  'i',  'l',  'o',  'W', 
    'a',  't',  't',  'H',  'o',  'u',  'r',  0xa7, 
    0x6c,  'd',  'e',  'f',  'a',  'u',  'l',  't', 
    'V',  'a',  'l',  'u',  'e',  0x00,  0x6b,  'd', 
    'i',  's',  'p',  'l',  'a',  'y',  'N',  'a', 
    'm',  'e',  0x75,  'T',  'o',  't',  'a',  'l', 
    ' ',  'e',  'n',  'e',  'r',  'g',  'y',  ' ', 
    'p',  'r',  'o',  'd',  'u',  'c',  'e',  'd', 
    0x70,  'd',  'i',  's',  'p',  'l',  'a',  'y', 
    'N',  'a',  'm',  'e',  'E',  'v',  'e',  'n', 
    't',  0x78,  0x1d,  'T',  'o',  't',  'a',  'l', 
    ' ',  'e',  'n',  'e',  'r',  'g',  'y',  ' ', 
    'p',  'r',  'o',  'd',  'u',  'c',  'e',  'd', 
    ' ',  'c',  'h',  'a',  'n',  'g',  'e',  'd', 
    0x62,  'i',  'd',  0x78,  0x24,  '2',  '9',  'f', 
    '2',  'f',  '8',  '8',  '8',  '-',  '3',  '4', 
    '7',  '1',  '-',  '8',  '2',  'f',  '6',  '-', 
    'a',  'f',  '0',  'f',  '-',  'e',  'f',  '5', 
    'f',  '1',  'f',  '4',  'f',  'a',  'f',  'f', 
    'f',  0x64,  'n',  'a',  'm',  'e',  0x73,  't', 
    'o',  't',  'a',  'l',  'E',  'n',  'e',  'r', 
    'g',  'y',  'P',  'r',  'o',  'd',  'u',  'c', 
    'e',  'd',  0x64,  't',  'y',  'p',  'e',  0x66, 
    'd',  'o',  'u',  'b',  'l',  'e',  0x64,  'u', 
    'n',  'i',  't',  0x6c,  'K',  'i',  'l',  'o', 
    'W',  'a',  't',  't',  'H',  'o',  'u',  'r', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN(IntegrationPluginSiemens, IntegrationPluginSiemens)

QT_WARNING_POP
QT_END_MOC_NAMESPACE
