#include <shared/system.h>
#include <shared/testing.h>
#include <shared/load_store.h>

#define BUF_SIZE (9)

int main(void) {
    {
        uint8_t data[]={0xf0,0xde,0xbc,0x9a,0x78,0x56,0x34,0x12};
        
        TEST_EQ_UU(Load8LE(data),0xf0);
        TEST_EQ_UU(Load16LE(data),0xdef0);
        TEST_EQ_UU(Load32LE(data),0x9abcdef0);
        TEST_EQ_UU(Load64LE(data),0x123456789abcdef0);
        
        TEST_EQ_UU(Load8BE(data),0xf0);
        TEST_EQ_UU(Load16BE(data),0xf0de);
        TEST_EQ_UU(Load32BE(data),0xf0debc9a);
        TEST_EQ_UU(Load64BE(data),0xf0debc9a78563412);
    }

    static const uint8_t ZERO[BUF_SIZE]={0,};
    
    {
        uint8_t buf[BUF_SIZE]={0,};
        
        Store8LE(buf,0xf0);
        TEST_EQ_UU(buf[0],0xf0);
        TEST_EQ_AA(buf+1,ZERO,BUF_SIZE-1);
        
        Store8BE(buf,0xf0);
        TEST_EQ_UU(buf[0],0xf0);
        TEST_EQ_AA(buf+1,ZERO,BUF_SIZE-1);
    }
    
    {
        uint8_t buf[BUF_SIZE]={0,};
        
        Store16LE(buf,0x1234);
        TEST_EQ_UU(buf[0],0x34);
        TEST_EQ_UU(buf[1],0x12);
        TEST_EQ_AA(buf+2,ZERO,BUF_SIZE-2);
        
        Store16BE(buf,0x1234);
        TEST_EQ_UU(buf[0],0x12);
        TEST_EQ_UU(buf[1],0x34);
        TEST_EQ_AA(buf+2,ZERO,BUF_SIZE-2);
    }

    {
        uint8_t buf[BUF_SIZE]={0,};
        
        Store32LE(buf,0x12345678);
        TEST_EQ_UU(buf[0],0x78);
        TEST_EQ_UU(buf[1],0x56);
        TEST_EQ_UU(buf[2],0x34);
        TEST_EQ_UU(buf[3],0x12);
        TEST_EQ_AA(buf+4,ZERO,BUF_SIZE-4);
        
        Store32BE(buf,0x12345678);
        TEST_EQ_UU(buf[0],0x12);
        TEST_EQ_UU(buf[1],0x34);
        TEST_EQ_UU(buf[2],0x56);
        TEST_EQ_UU(buf[3],0x78);
        TEST_EQ_AA(buf+4,ZERO,BUF_SIZE-4);
    }

    {
        uint8_t buf[BUF_SIZE]={0,};
        
        Store64LE(buf,0x123456789abcdef0);
        TEST_EQ_UU(buf[0],0xf0);
        TEST_EQ_UU(buf[1],0xde);
        TEST_EQ_UU(buf[2],0xbc);
        TEST_EQ_UU(buf[3],0x9a);
        TEST_EQ_UU(buf[4],0x78);
        TEST_EQ_UU(buf[5],0x56);
        TEST_EQ_UU(buf[6],0x34);
        TEST_EQ_UU(buf[7],0x12);
        TEST_EQ_AA(buf+8,ZERO,BUF_SIZE-8);
        
        Store64BE(buf,0x123456789abcdef0);
        TEST_EQ_UU(buf[0],0x12);
        TEST_EQ_UU(buf[1],0x34);
        TEST_EQ_UU(buf[2],0x56);
        TEST_EQ_UU(buf[3],0x78);
        TEST_EQ_UU(buf[4],0x9a);
        TEST_EQ_UU(buf[5],0xbc);
        TEST_EQ_UU(buf[6],0xde);
        TEST_EQ_UU(buf[7],0xf0);
        TEST_EQ_AA(buf+8,ZERO,BUF_SIZE-8);
    }
}
