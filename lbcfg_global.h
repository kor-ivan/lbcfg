#ifndef LBCFG_GLOBAL_H
#define LBCFG_GLOBAL_H
#include <QtCore/qglobal.h>

#if defined(MY_SHARED_LIB_LIBRARY)
#  define LBCFG_CORE_EXPORT Q_DECL_EXPORT
#else
#  define LBCFG_CORE_EXPORT Q_DECL_IMPORT
#endif

#endif // LBCFG_GLOBAL_H
