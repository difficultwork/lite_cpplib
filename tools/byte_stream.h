/**
 * @file    tools\byte_stream.h
 * @brief   Encapsulation that operations on byte stream
 * @author  Nik Yan
 * @version 1.0     2014-06-29
 */

#ifndef _LITE_BYTE_STREAM_H
#define _LITE_BYTE_STREAM_H

#include "base/lite_base.h"
#include "base/byte_order.h"

namespace lite {

class ByteStream
{
public:

    ByteStream(uint32_t size = 0)
        : read_idx_(0)
        , write_idx_(0)
        , byte_order_(HOST_BYTEORDER)
    {
        if (size > 0)
        {
            mallocated_  = true;
            stream_size_ = size;
            data_        = new uint8_t[uSize];
        } 
        else
        {
            mallocated_  = false;
            stream_size_ = 0;
            data_        = NULL;
        }
    }

    ByteStream(const uint8_t* data, uint32_t size)
    {
        mallocated_    = false;
        data_          = const_cast<uint8_t*>(data);
        stream_size_   = size;
        read_idx_      = 0;
        write_idx_     = size;
        byte_order_    = HOST_BYTEORDER;
    }

    /**
     * @brief   Copy constructor
     */
    ByteStream(const ByteStream &c)
    {
        mallocated_    = false;
        stream_size_   = 0;
        data_          = NULL;
        write_idx_     = 0;
        read_idx_      = 0;
        byte_order_    = HOST_BYTEORDER;

        *this = c;
    }

    ByteStream& operator=(const ByteStream &c)
    {
        _Free();
        
        if (c.m_uStreamSize > 0)
        {
            data_        = new uint8_t[c.stream_size_];
            stream_size_ = c.stream_size_;
            read_idx_    = c.read_idx_;
            write_idx_   = c.write_idx_;
            byte_order_  = c.byte_order_;
            mallocated_  = true;

            memcpy(data_, c.data_, write_idx_);
        }

        return *this;
    }

    virtual ~ByteStream()
    {
        _Free();
    }

    /**
     * @brief   Check whether the read pointer reaches the end of the byte stream
     */
    bool Eof()
    {
        return GetReadPtr() == GetWritePtr();
    }
    
    void SetByteOrder(BYTEORDER byte_order)
    {
        byte_order_ = byte_order;
    }

    /**
     * @brief   Add data to byte stream
     */
    ByteStream& Add(const void* data, uint32_t size)
    {
        if (data == NULL || size == 0)
        {
            return *this;
        }

        _Reserve(write_idx_ + size);
        memcpy(&data_[write_idx_], data, size);
        write_idx_ += size;
        return *this;
    }

    /**
     * @brief   Add a string to byte stream(exclude '\0')
     */
    ByteStream& Add(const char* append_str)
    {
        if (append_str == NULL || strlen(append_str) == 0)
        {
            return *this;
        }

        return Add(append_str, (uint32_t)strlen(append_str));
    }

    /**
     * @brief   Add a string to byte stream(exclude '\0')
     */
    ByteStream& Add(string& append_str)
    {
        return Add(append_str.c_str(), (uint32_t)append_str.length());
    }

    /**
     * @brief   Add byte stream to byte stream
     */
    ByteStream& Add(const ByteStream& append_str)
    {
        Add(append_str.GetBuffer(), append_str.GetWritePtr());
        return *this;
    }

    /**
     * @brief   Reading data from byte stream
     * @param   data        The starting pointer
     * @param   read_size   Read size
     * @return  Returns the byte stream instance object reference
     */
    ByteStream& Get(void* data, uint32_t read_size)
    {
        if (data_ == NULL || read_size + read_idx_ > write_idx_)
        {
            throw access_violation_exception("byte stream overflow");         
        }
        memcpy(data, &data_[read_idx_], read_size);
        read_idx_ += read_size;
        return *this;
    }

    uint32_t GetBufferSize() const 
    { 
        return stream_size_; 
    }

    /**
     * @brief   Get the byte stream internal buffer
     */
    const uint8_t* GetBuffer() const 
    { 
        return data_; 
    }

    /**
     * @brief   Get the position of the current byte stream's internal read pointer
     */
    uint32_t GetReadPtr() const 
    {
        return read_idx_;
    }

    /**
     * @brief   Set the position of the current byte stream's internal read pointer
     */
    void SetReadPtr(uint32_t read_idx) 
    {
        if (read_idx > wirte_idx_)
        {
            throw -1;         
        }
        read_idx_ = read_idx;
    }

    /**
     * @brief   Get the position of the current byte stream's internal write pointer
     */
    uint32_t GetWritePtr() const 
    {
        return write_idx_;
    }

    /**
     * @brief   Set the position of the current byte stream's internal write pointer
     * @caution If the write pointer is greater than the maximum length of the byte stream,
     *          it is set to the maximum length of the byte stream.
     */
    void SetWritePtr(uint32_t write_idx) 
    {
        if (write_idx > stream_size_)
        {
            write_idx = stream_size_;
        }
        write_idx_ = write_idx;
    }

    /**
     * @brief   Clear the byte stream to read out the data, the newest read pointer position points to 0
     */
    void FlushReadPtr() 
    {
        if (GetBuffer() == NULL)
        {
            return;
        }

        memcpy((char*)GetBuffer(), (char*)(GetBuffer() + GetReadPtr()), (GetBufferSize() - GetReadPtr()));
        SetWritePtr(GetWritePtr() - GetReadPtr());
        SetReadPtr(0);
    }

    /**
     * @brief   Type conversion operator(void*)
     */
    operator const void*() const
    {
        if (stream_size_ > 0)
        {
            return data_;
        }
        else
        {
            return NULL;
        }
    }

    /**
     * @brief   Overloaded subscript operator([idx])
     * @return  Return the byte data corresponding to the specified subscript position
     */
    uint8_t operator[](int idx) const
    {
        return data_[idx];
    }

    uint8_t &operator[](int idx)
    {
        return data_[idx];
    }


    /**
     * @brief   Read an 8-bit integer from byte stream
     */
    int8_t GetInt8()
    {
        int8_t value;
        (*this) >> value;
        return value;
    }
    
    /**
     * @brief   Read an unsigned 8-bit integer from byte stream
     */
    uint8_t GetUint8()
    {
        uint8_t value;
        (*this) >> value;
        return value;
    }

    /**
     * @brief   Put an 8-bit integer to byte stream
     */
    void PutInt8(int8_t value)
    {
        (*this) << value;
    }

    /**
     * @brief   Put an unsigned 8-bit integer to byte stream
     */
    void PutUint8(uint8_t value)
    {
        (*this) << value;
    }
    
    /**
     * @brief   Read a 16-bit integer from byte stream
     */
    int16_t GetInt16()
    {
        int16_t value;
        (*this) >> value;
        return value;
    }
    
    /**
     * @brief   Read an unsigned 16-bit integer from byte stream
     */
    uint16_t GetUint16()
    {
        uint16_t value;

        (*this) >> value;
        return value;
    }

    /**
     * @brief   Put a 16-bit integer to byte stream
     */
    void PutInt16(int16_t value)
    {
        (*this) << value;
    }

    /**
     * @brief   Put an unsigned 16-bit integer to byte stream
     */
    void PutUint16(uint16_t value)
    {
        (*this) << value;
    }
    
    /**
     * @brief   Read a 32-bit integer from byte stream
     */
    int32_t GetInt32()
    {
        int32_t value;
        (*this) >> value;
        return value;
    }
    
    /**
     * @brief   Read an unsigned 32-bit integer from byte stream
     */
    uint32_t GetUint32()
    {
        uint32_t value;
        (*this) >> value;
        return value;
    }

    /**
     * @brief   Put a 32-bit integer to byte stream
     */
    void PutInt32(int32_t value)
    {
        (*this) << value;
    }

    /**
     * @brief   Put an unsigned 32-bit integer to byte stream
     */
    void PutUint32(uint32_t value)
    {
        (*this) << value;
    }
    
    /**
     * @brief   Read a 64-bit integer from byte stream
     */
    int64_t GetInt64()
    {
        int64_t value;
        (*this) >> value;
        return value;
    }
    
    /**
     * @brief   Read a unsigned 64-bit integer from byte stream
     */
    uint64_t GetUint64()
    {
        uint64_t value;
        (*this) >> value;
        return value;
    }

    /**
     * @brief   Put a 64-bit integer from byte stream
     */
    void PutInt64(int64_t value)
    {
        (*this) << value;
    }

    /**
     * @brief   Put an unsigned 64-bit integer from byte stream
     */
    void PutUint64(uint64_t value)
    {
        (*this) << value;
    }
    
    /**
     * @brief   Read a string from byte stream until '\0'
     */
    string GetString() 
    { 
        ByteStream bs;
        while (true)
        {
            if (Eof())
            {
                bs.PutUint8(0);
                break;
            }

            uint8_t c = GetUint8();
            bs.PutUint8(c);
            if (c == 0)
            {
                break;
            }
        }
        string str = string((char*)bs.GetBuffer());
        return str;
    }

    /**
     * @brief   Add a string to byte stream
     */
    void PutString(string& str) 
    { 
        (*this) << str;
    }

protected:
    /**
     * @brief   Flow operator: Put an 8-bit integer to byte stream
     *          Flow operator: Put a 16-bit integer to byte stream
     *          Flow operator: Put a 32-bit integer to byte stream
     *          Flow operator: Put a 64-bit integer to byte stream
     *          Flow operator: Put an unsigned 8-bit integer to byte stream
     *          Flow operator: Put an unsigned 16-bit integer to byte stream
     *          Flow operator: Put an unsigned 32-bit integer to byte stream
     *          Flow operator: Put an unsigned 64-bit integer to byte stream
     */
    ByteStream& operator <<(int8_t b)   { return Add(&b,                  1); }
    ByteStream& operator <<(int16_t b)  { return Add(&_WriteByteorder(b), 2); }
    ByteStream& operator <<(int32_t b)  { return Add(&_WriteByteorder(b), 4); }
    ByteStream& operator <<(int64_t b)  { return Add(&_WriteByteorder(b), 8); }
    ByteStream& operator <<(uint8_t b)  { return Add(&b,                  1); }
    ByteStream& operator <<(uint16_t b) { return Add(&_WriteByteorder(b), 2); }
    ByteStream& operator <<(uint32_t b) { return Add(&_WriteByteorder(b), 4); }
    ByteStream& operator <<(uint64_t b) { return Add(&_WriteByteorder(b), 8); }

    /**
     * @brief   Flow operator: Add a string to byte stream(end of '\0')
     */
    ByteStream& operator <<(char* str) 
    { 
        if (str == NULL)
        {
            throw null_ptr_exception();
        }
        return Add(str, (uint32_t)(strlen(str)+1)); 
    }

    ByteStream& operator <<(unsigned char* str) 
    { 
        if (str == NULL)
        {
            throw null_ptr_exception();
        }
        return (*this) << (char*)str; 
    }

    ByteStream& operator <<(string& str) 
    { 
        return (*this) << (char*)str.c_str(); 
    }

    ByteStream& operator <<(const ByteStream& bs)
    {
        return Add(bs.GetBuffer(), (uint32_t)bs.GetWritePtr());
    }

    /**
     * @brief   Flow operator: Get an 8-bit integer from byte stream
     *          Flow operator: Get a 16-bit integer from byte stream
     *          Flow operator: Get a 32-bit integer from byte stream
     *          Flow operator: Get a 64-bit integer from byte stream
     *          Flow operator: Get an unsigned 8-bit integer from byte stream
     *          Flow operator: Get an unsigned 16-bit integer from byte stream
     *          Flow operator: Get an unsigned 32-bit integer from byte stream
     *          Flow operator: Get an unsigned 64-bit integer from byte stream
     */
    ByteStream& operator >>(int8_t& b)   { Get(&b, 1);                    return *this; }
    ByteStream& operator >>(int16_t& b)  { Get(&b, 2); _ReadByteorder(b); return *this; }
    ByteStream& operator >>(int32_t& b)  { Get(&b, 4); _ReadByteorder(b); return *this; }
    ByteStream& operator >>(int64_t& b)  { Get(&b, 8); _ReadByteorder(b); return *this; }
    ByteStream& operator >>(uint8_t& b)  { Get(&b, 1);                    return *this; }
    ByteStream& operator >>(uint16_t& b) { Get(&b, 2); _ReadByteorder(b); return *this; }
    ByteStream& operator >>(uint32_t& b) { Get(&b, 4); _ReadByteorder(b); return *this; }
    ByteStream& operator >>(uint64_t& b) { Get(&b, 8); _ReadByteorder(b); return *this; }

    /**
     * @brief   Flow operator: Read a string from byte stream(end of '\0')   
     */
    ByteStream& operator >>(string& str) 
    { 
        str = GetString();
        return *this; 
    }
    
    /**
     * @brief   Flow operator: Read a stream from current byte stream, and put to another byte stream
     */
    ByteStream& operator >>(ByteStream& bs) 
    { 
        uint32_t len = GetUint32();
        
        if (len == 0)
        {
            return *this;
        }
        bs._Reserve(len);
        Get(bs.data_, len);
        bs.SetWritePtr(len);
        return *this;
    }

private:

    void _Free()
    {
        if (mallocated_ && data_ )
        {
            delete data_;
        }
        data_          = NULL;
        stream_size_   = 0;
        read_idx_      = 0;
        write_idx_     = 0;
        mallocated_    = false;
    }

    /**
     * @brief   Resize the inner buffer of byte stream
     */
    void _Reserve(uint32_t new_size)
    {
        uint8_t* data = data_;
        if (new_size <= stream_size_ && data_ != NULL && mallocated_)
        {
            return;
        }
        else
        {
            if (new_size < stream_size_ + 1024)
            {
                new_size = stream_size_ + 1024;
            }
            if (new_size < stream_size_ + (stream_size_ / 16))
            {
                new_size = stream_size_ + (stream_size_ / 16);
            }
        }

        data_ = new uint8_t[new_size];

        if (data != NULL)
        {
            memcpy(data_, data, write_idx_);
            if (mallocated_)
            {
                delete data;
            }
        }

        stream_size_ = new_size;
        mallocated_  = true;
    }

    /**
     * @brief   write byte-order conversion function
     */
    int16_t& _WriteByteorder(int16_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? static_cast<int16_t>(HtonUint16(static_cast<uint16_t>(d))) : d;
        return d; 
    }

    uint16_t& _WriteByteorder(uint16_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? HtonUint16(d) : d;
        return d; 
    }

    int32_t& _WriteByteorder(int32_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? static_cast<int32_t>(HtonUint32(static_cast<uint32_t>(d))) : d;
        return d; 
    }

    uint32_t& _WriteByteorder(uint32_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? HtonUint32(d) : d;
        return d; 
    }

    int64_t& _WriteByteorder(int64_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? static_cast<int64_t>(HtonUint64(static_cast<uint64_t>(d))) : d;
        return d; 
    }

    uint64_t& _WriteByteorder(uint64_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? HtonUint64(d) : d;
        return d; 
    }

    /**
     * @brief   read byte-order conversion function
     */
    void _ReadByteorder(int16_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? static_cast<int16_t>(NtohUint16(static_cast<uint16_t>(d))) : (d);
        return d; 
    }

    void _ReadByteorder(uint16_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? NtohUint16(d) : (d);
        return d; 
    }

    void _ReadByteorder(int32_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? static_cast<int32_t>(NtohUint32(static_cast<uint32_t>(d))) : (d);
        return d; 
    }

    void _ReadByteorder(uint32_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? NtohUint32(d) : (d);
        return d; 
    }

    void _ReadByteorder(int64_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? static_cast<int64_t>(NtohUint64(static_cast<uint64_t>(d))) : (d);
        return d; 
    }

    void _ReadByteorder(uint64_t& d)
    {
        d = byte_order_ == NETWORK_BYTEORDER ? NtohUint64(d) : (d);
        return d; 
    }

    uint8_t*    data_;
    uint32_t    read_idx_;
    uint32_t    write_idx_;
    uint32_t    stream_size_;
    bool        mallocated_;
    BYTEORDER   byte_order_;   

};

} // end of namespace lite

using namespace lite;

#endif // ifndef _LITE_BYTE_STREAM_H
