/**
 * @file    base\byte_order.h
 * @brief   Encapsulation for byte-order operations
 * @author  Nik Yan
 * @version 1.0     2014-05-10
 */

#ifndef _LITE_BYTE_ORDER_H_
#define _LITE_BYTE_ORDER_H_

#include "lite_base.h"

namespace lite {

typedef enum BYTEORDER
{
    LITTLE_ENDIAN = 0,                  ///< LSB
    BIG_ENDIAN    = 1,                  ///< MSB
};

#ifdef OS_WIN
    #define HOST_IS_LITTLE_ENDIAN
    #define HOST_BYTEORDER     LITTLE_ENDIAN   ///< host order
    #define NETWORK_BYTEORDER  BIG_ENDIAN      ///< network order
#elif defined(OS_LINUX)
    #define HOST_BYTEORDER     BIG_ENDIAN      ///< host order
    #define NETWORK_BYTEORDER  BIG_ENDIAN      ///< network order
#endif

/**
 * @brief   Reverse Short Type
 */
inline uint16_t ReverseShort(uint16_t d) 
{ 
    return static_cast<uint16_t>(((d & 0x00ff) << 8) | static_cast<uint16_t>((d & 0xff00) >> 8)); 
}

/**
 * @brief   Reverse Int Type
 */
inline uint32_t ReverseInt(uint32_t d) 
{ 
    return (((d & 0x000000ff) << 24) | ((d & 0x0000ff00) << 8) | ((d & 0x00ff0000) >> 8) | ((d & 0xff000000) >> 24)); 
}

/**
 * @brief   Reverse Long Type
 */
inline uint64_t ReverseLog(uint64_t d) 
{ 
    return (((d & 0xff00000000000000) >> 56) |
            ((d & 0x00ff000000000000) >> 40) |
            ((d & 0x0000ff0000000000) >> 24) |
            ((d & 0x000000ff00000000) >> 8)  |
            ((d & 0x00000000ff000000) << 8)  |
            ((d & 0x0000000000ff0000) << 24) |
            ((d & 0x000000000000ff00) << 40) |
            ((d & 0x00000000000000ff) << 56)); 
}

/**
 * @brief   Converts ushort data in host format to network format
 */
inline uint16_t HtonUint16(uint16_t d)
{
#ifdef HOST_IS_LITTLE_ENDIAN
    return ReverseShort(d);
#else
    return d;
#endif
}

/**
 * @brief   Converts ushort data in network format to host format
 */
inline uint16_t NtohUint16(uint16_t d)
{
#ifdef HOST_IS_LITTLE_ENDIAN
    return ReverseShort(d);
#else
    return d;
#endif
}

/**
 * @brief   Converts integer data in host format to network format
 */
inline uint32_t HtonUint32(uint32_t d)
{
#ifdef HOST_IS_LITTLE_ENDIAN
    return ReverseInt(d);
#else
    return d;
#endif
}

/**
 * @brief   Converts integer data in network format to host format
 */
inline uint32_t NtohUint32(uint32_t d)
{
#ifdef HOST_IS_LITTLE_ENDIAN
    return ReverseInt(d);
#else
    return d;
#endif
}

/**
 * @brief   Converts long integer data in host format to network format
 */
inline uint64_t HtonUint64(uint64_t d)
{
#ifdef HOST_IS_LITTLE_ENDIAN
    return ReverseLong(d);
#else
    return d;
#endif
}

/**
 * @brief   Converts long integer data in network format to host format
 */
inline uint64_t NtohUint64(uint64_t d)
{
#ifdef HOST_IS_LITTLE_ENDIAN
    return ReverseLong(d);
#else
    return d;
#endif
}

} // end of namespace lite

using namespace lite;

#endif // ifndef _LITE_BYTE_ORDER_H_