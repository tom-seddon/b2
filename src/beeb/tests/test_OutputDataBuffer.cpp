#include <shared/system.h>
#include <shared/testing.h>
#include <thread>
#include <shared/debug.h>
#include <beeb/OutputData.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef OutputDataBuffer<uint64_t> Test0Buf;

static void Test0() {
    Test0Buf buf(100);

    uint64_t *wa,*wb;
    const uint64_t *ra,*rb;
    size_t na,nb;
    uint64_t next_write=0,next_read=0;

    // produce N-1 (total N-1)
    TEST_TRUE(buf.GetProducerBuffers(&wa,&na,&wb,&nb));
    TEST_EQ_UU(na,buf.SIZE);
    TEST_EQ_UU(nb,0);
    TEST_NULL(wb);
    for(size_t i=0;i<buf.SIZE-1;++i) {
        wa[i]=next_write++;
    }
    buf.Produce(buf.SIZE-1);

    // produce 1 (total N)
    TEST_TRUE(buf.GetProducerBuffers(&wa,&na,&wb,&nb));
    TEST_EQ_UU(na,1);
    TEST_EQ_UU(nb,0);
    TEST_NULL(wb);
    wa[0]=next_write++;
    buf.Produce(1);

    TEST_FALSE(buf.GetProducerBuffers(&wa,&na,&wb,&nb));

    // consume 0 (total N)
    TEST_TRUE(buf.GetConsumerBuffers(&ra,&na,&rb,&nb));
    TEST_EQ_UU(na,buf.SIZE);
    TEST_EQ_UU(nb,0);
    TEST_NULL(rb);
    for(size_t i=0;i<na;++i) {
        TEST_EQ_UU(ra[i],next_read+i);
    }
    buf.Consume(0);

    // consume N-5 (total 5)
    TEST_TRUE(buf.GetConsumerBuffers(&ra,&na,&rb,&nb));
    TEST_EQ_UU(na,buf.SIZE);
    TEST_EQ_UU(nb,0);
    TEST_NULL(rb);
    buf.Consume(buf.SIZE-5);
    next_read+=buf.SIZE-5;

    // produce 10 (total 15)
    TEST_TRUE(buf.GetProducerBuffers(&wa,&na,&wb,&nb));
    TEST_EQ_UU(na,buf.SIZE-5);
    TEST_EQ_UU(nb,0);
    for(size_t i=0;i<10;++i) {
        wa[i]=next_write++;
    }
    buf.Produce(10);

    // consume 15 (total 0)
    TEST_TRUE(buf.GetConsumerBuffers(&ra,&na,&rb,&nb));
    TEST_EQ_UU(na,5);
    TEST_EQ_UU(nb,10);
    for(size_t i=0;i<na;++i) {
        TEST_EQ_UU(ra[i],next_read++);
    }
    for(size_t i=0;i<nb;++i) {
        TEST_EQ_UU(rb[i],next_read++);
    }
    buf.Consume(na+nb);

    // produce N (total N)
    TEST_TRUE(buf.GetProducerBuffers(&wa,&na,&wb,&nb));
    TEST_EQ_UU(na,buf.SIZE-10);
    TEST_EQ_UU(nb,10);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main() {
    Test0();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
