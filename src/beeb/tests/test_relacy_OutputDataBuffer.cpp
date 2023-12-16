#include <shared/system.h>
#include <shared/testing.h>
#include <thread>
#include <shared/debug.h>

#define USE_RELACY 1
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4456) //4456: declaration of IDENTIFIER hides previous local declaration
#pragma warning(disable : 4595) //C4595: FUNCTION: non-member operator new or delete functions may not be declared inline
#pragma warning(disable : 4311) //C4311: OPERATION: pointer truncation from TYPE to TYPE
#pragma warning(disable : 4312) //C4312: OPERATION: conversion from TYPE to TYPE of greater size
#pragma warning(disable : 4302) //C4302: OPERATION: truncation from TYPE to TYPE
#pragma warning(disable : 4267) //C4267: THING: conversion from TYPE to TYPE, possible loss of data
#pragma warning(disable : 4800) //C4800: Implicit conversion from TYPE to bool. Possible information loss
#define RL_MSVC_OUTPUT
#elif defined __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <relacy/relacy.hpp>
#include <relacy/relacy_std.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#elif defined __GNUC__
#pragma GCC diagnostic pop
#endif

#include <beeb/OutputData.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct RelacyOutputDataBufferTest : rl::test_suite<RelacyOutputDataBufferTest, 2> {
    OutputDataBuffer<uint64_t> buf{5};

    void thread(unsigned thread_index) {
        int num_iterations = 10;
        int next = 0;

        if (thread_index == 0) {
            // producer
            for (;;) {
                uint64_t *pa, *pb;
                size_t na, nb;
                if (buf.GetProducerBuffers(&pa, &na, &pb, &nb)) {
                    for (size_t j = 0; j < na; ++j) {
                        pa[j] = next++;
                    }

                    for (size_t j = 0; j < nb; ++j) {
                        pb[j] = next++;
                    }

                    buf.Produce(na + nb);

                    if (next >= num_iterations) {
                        break;
                    }
                }
            }
        } else {
            for (;;) {
                const uint64_t *pa, *pb;
                size_t na, nb;
                if (buf.GetConsumerBuffers(&pa, &na, &pb, &nb)) {
                    for (size_t j = 0; j < na; ++j) {
                        RL_ASSERT(pa[j] == next);
                        ++next;
                    }
                    for (size_t j = 0; j < nb; ++j) {
                        RL_ASSERT(pb[j] == next);
                        ++next;
                    }

                    buf.Consume(na + nb);

                    if (next >= num_iterations) {
                        break;
                    }
                }
            }
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

template <typename T>
class nonblocking_spsc_queue {
  public:
    nonblocking_spsc_queue() {
        node *n = new node();
        VAR(head) = n;
        VAR(tail) = n;
    }

    ~nonblocking_spsc_queue() {
        RL_ASSERT(VAR(head) == VAR(tail));
        delete (node *)VAR(head);
    }

    void enqueue(T data) {
        node *n = new node(data);
        VAR(head)->next.store(n, std::memory_order_release);
        VAR(head) = n;
    }

    bool dequeue(T &data) {
        node *t = VAR(tail);
        node *n = t->next.load(std::memory_order_acquire);
        if (0 == n)
            return false;
        data = n->VAR(data);
        delete t;
        VAR(tail) = n;
        return true;
    }

  private:
    struct node {
        std::atomic<node *> next;
        VAR_T(T)
        data;

        node(T data = T())
            : next(0)
            , data(data) {
        }
    };

    VAR_T(node *)
    head;
    VAR_T(node *)
    tail;
};

struct nonblocking_spsc_queue_test : rl::test_suite<nonblocking_spsc_queue_test, 2> {
    nonblocking_spsc_queue<int> q;

    void thread(unsigned thread_index) {
        int num_iterations = 10;

        if (0 == thread_index) {
            for (int i = 0; i < num_iterations; ++i) {
                q.enqueue(i);
            }
        } else {
            int i = 0;
            for (;;) {
                int x;
                if (q.dequeue(x)) {
                    RL_ASSERT(x == i);
                    ++i;
                    if (i == num_iterations) {
                        break;
                    }
                }
            }
            //int data=0;
            //while(false==q.dequeue(data))
            //{
            //}
            //RL_ASSERT(11==data);
        }
    }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

int main() {
    TEST_TRUE(rl::simulate<nonblocking_spsc_queue_test>());
    TEST_TRUE(rl::simulate<RelacyOutputDataBufferTest>());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
