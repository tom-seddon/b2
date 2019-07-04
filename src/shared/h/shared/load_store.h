#ifdef HEADER_A7E017ED6D8541DAA8D3AE20C4272A9A
#error Multiple inclusion
#else
#define HEADER_A7E017ED6D8541DAA8D3AE20C4272A9A

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <string.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline uint8_t Load8LE(const unsigned char *src) {
    return (uint8_t)src[0];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline uint16_t Load16LE(const unsigned char *src) {
#if CPU_LITTLE_ENDIAN
    uint16_t value;
    memcpy(&value,src,2);
    
    return value;
#else
    return (uint16_t)((src[0])|
                      src[1]<<8);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline uint32_t Load32LE(const unsigned char *src) {
#if CPU_LITTLE_ENDIAN
    uint32_t value;
    memcpy(&value,src,4);
    
    return value;
#else
    return (uint32_t)(src[0]|
                      (src[1]<<8)|
                      (src[2]<<16)|
                      (src[3]<<24));
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline uint64_t Load64LE(const unsigned char *src) {
    
#if CPU_LITTLE_ENDIAN
    uint64_t value;
    memcpy(&value,src,8);
    
    return value;
#else
    return (uint64_t)(src[0]|
                      ((uint32_t)src[1]<<8)|
                      ((uint32_t)src[2]<<16)|
                      ((uint32_t)src[3]<<24)|
                      ((uint64_t)src[4]<<32)|
                      ((uint64_t)src[5]<<40)|
                      ((uint64_t)src[6]<<48)|
                      ((uint64_t)src[7]<<56));
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline uint8_t Load8BE(const unsigned char *src) {
    return (uint8_t)src[0];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline uint16_t Load16BE(const unsigned char *src) {
#if CPU_LITTLE_ENDIAN
    return (uint16_t)((src[0]<<8)|
                      src[1]);
#else
    uint16_t value;
    memcpy(&value,src,2);
    
    return value;
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline uint32_t Load32BE(const unsigned char *src) {
#if CPU_LITTLE_ENDIAN
    return (uint32_t)((src[0]<<24)|
                      (src[1]<<16)|
                      (src[2]<<8)|
                      src[3]);
#else
    uint32_t value;
    memcpy(&value,src,4);
    
    return value;
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline uint64_t Load64BE(const unsigned char *src) {
#if CPU_LITTLE_ENDIAN
    return (uint64_t)(((uint64_t)src[0]<<56)|
                      ((uint64_t)src[1]<<48)|
                      ((uint64_t)src[2]<<40)|
                      ((uint64_t)src[3]<<32)|
                      ((uint64_t)src[4]<<24)|
                      ((uint64_t)src[5]<<16)|
                      ((uint64_t)src[6]<<8)|
                      src[7]);
#else
    uint64_t value;
    memcpy(&value,src,8);
    
    return value;
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void Store8LE(unsigned char *dest,uint8_t value) {
    *dest=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void Store16LE(unsigned char *dest,uint16_t value) {
#if CPU_LITTLE_ENDIAN
    memcpy(dest,&value,2);
#else
    dest[0]=(uint8_t)value;
    dest[1]=(uint8_t)(value>>8);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void Store32LE(unsigned char *dest,uint32_t value) {
#if CPU_LITTLE_ENDIAN
    memcpy(dest,&value,4);
#else
    dest[0]=(uint8_t)value;
    dest[1]=(uint8_t)(value>>8);
    dest[2]=(uint8_t)(value>>16);
    dest[3]=(uint8_t)(value>>24);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void Store64LE(unsigned char *dest,uint64_t value) {
#if CPU_LITTLE_ENDIAN
    memcpy(dest,&value,8);
#else
    dest[0]=(uint8_t)value;
    dest[1]=(uint8_t)(value>>8);
    dest[2]=(uint8_t)(value>>16);
    dest[3]=(uint8_t)(value>>24);
    dest[4]=(uint8_t)(value>>32);
    dest[5]=(uint8_t)(value>>40);
    dest[6]=(uint8_t)(value>>48);
    dest[7]=(uint8_t)(value>>56);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void Store8BE(unsigned char *dest,uint8_t value) {
    dest[0]=value;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void Store16BE(unsigned char *dest,uint16_t value) {
#if CPU_LITTLE_ENDIAN
    dest[0]=(uint8_t)(value>>8);
    dest[1]=(uint8_t)value;
#else
    memcpy(dest,&value,2);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void Store32BE(unsigned char *dest,uint32_t value) {
#if CPU_LITTLE_ENDIAN
    dest[0]=(uint8_t)(value>>24);
    dest[1]=(uint8_t)(value>>16);
    dest[2]=(uint8_t)(value>>8);
    dest[3]=(uint8_t)value;
#else
    memcpy(dest,&value,4);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void Store64BE(unsigned char *dest,uint64_t value) {
#if CPU_LITTLE_ENDIAN
    dest[0]=(uint8_t)(value>>56);
    dest[1]=(uint8_t)(value>>48);
    dest[2]=(uint8_t)(value>>40);
    dest[3]=(uint8_t)(value>>32);
    dest[4]=(uint8_t)(value>>24);
    dest[5]=(uint8_t)(value>>16);
    dest[6]=(uint8_t)(value>>8);
    dest[7]=(uint8_t)value;
#else
    memcpy(dest,&value,8);
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void ByteSwap16(uint16_t *value) {
    uint8_t t,*p=(uint8_t *)value;

    t=p[0];
    p[0]=p[1];
    p[1]=t;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void ByteSwap32(uint32_t *value) {
    uint8_t t,*p=(uint8_t *)value;

    t=p[0];
    p[0]=p[3];
    p[3]=t;

    t=p[1];
    p[1]=p[2];
    p[2]=t;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static inline void ByteSwap64(uint64_t *value) {
    uint8_t t,*p=(uint8_t *)value;

    t=p[0];
    p[0]=p[7];
    p[7]=t;
    
    t=p[1];
    p[1]=p[6];
    p[6]=t;

    t=p[2];
    p[2]=p[5];
    p[5]=t;

    t=p[3];
    p[3]=p[4];
    p[4]=t;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
