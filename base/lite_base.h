/**
 * @file    base\lite_base.h
 * @brief   This is the base header file of the lite cpplib
 * @author  Nik Yan
 * @version 1.0     2014-05-10
 */


#ifndef _LITE_BASE_H_
#define _LITE_BASE_H_

#ifdef _MSC_VER
#pragma warning (disable:4101)
#pragma warning (disable:4251)
#pragma warning (disable:4312)
#pragma warning (disable:4786)
#pragma warning (disable:4819)
#pragma warning (disable:4996)
#pragma warning (disable:4267)
#pragma warning (disable:4244)
#if _MSC_VER >= 1300
#pragma warning (disable:4290)
#endif
#if _MSC_VER > 1000
#pragma once
#endif
#endif

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#define OS_WIN
#elif defined(__linux)
#define OS_LINUX
#endif

#ifdef OS_WIN
#ifdef _DEBUG 
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#define new new(_NORMAL_BLOCK,__FILE__,__LINE__)
#endif
#endif

#ifdef OS_WIN
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <iosfwd> 
#include <vector>
#include <memory>
#include <stdint.h>

#elif defined(OS_LINUX)
#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <string>
#include <map>
#include <list>
#include <iostream>
#include <iosfwd> 
#include <vector>
#include <memory>
#include <stdint.h>

#endif

using namespace std;

namespace lite
{
typedef unsigned short      ushort;
typedef unsigned int        uint;
typedef unsigned long long  uint64;
typedef unsigned int        timeout_t;

#define __global
#define __static            static
}

using namespace lite;

#endif // ifndef _LITE_BASE_H_
