#ifndef HEADER_B87499F224C64FE088EA9E1160147313
#define HEADER_B87499F224C64FE088EA9E1160147313

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <new>
#include <shared/debug.h>
#include <shared/relacy.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The buffer is circular, so the returned area can be in two parts,
// referred to as A and B.
//
// Fill/consume A first, then B.

template<class T>
class OutputDataBuffer {
public:
    const size_t SIZE;

    explicit OutputDataBuffer(size_t n):
        SIZE(n),
        m_buf(new T[SIZE])
    {
    }

    ~OutputDataBuffer() {
        delete[] m_buf;
    }

    RDELETED(OutputDataBuffer(const OutputDataBuffer<T> &));
    RDELETED(OutputDataBuffer &operator=(const OutputDataBuffer<T> &));
    RDELETED(OutputDataBuffer(OutputDataBuffer<T> &&));
    RDELETED(OutputDataBuffer &operator=(OutputDataBuffer<T> &&));

    // Grab pointer(s) to storage for up to N items. If any items were
    // allocated at all, fill *PA_PTR/*NA_PTR with pointer & count of
    // first portion, and *PB_PTR/*NB_PTR with pointer & count of
    // second portion, then return true.
    //
    // When there's no second portion, *PB_PTR=nullptr and *NB_PTR=0.
    //
    // If there's no space for any items, return false.
    //
    // When ProducerLock returns true, call ProducerUnlock to commit
    // produced items (even if there were 0 of them). There must be
    // one ProducerUnlock call per ProducerLock.
    bool ProducerLock(T **pa_ptr,size_t *na_ptr,T **pb_ptr,size_t *nb_ptr,size_t n) {
        ASSERT(RVAL(m_last_wn)==0);

        uint64_t rv=RVAL(m_rv).load(RMO_ACQUIRE);
        uint64_t wv=RVAL(m_wv).load(RMO_ACQUIRE);

        ASSERT(wv>=rv);
        size_t used=(size_t)(wv-rv);
        size_t free=SIZE-used;

        if(free==0) {
            return false;
        }

        if(n>free) {
            n=free;
        }

        size_t begin_index=(size_t)(wv%SIZE);
        size_t end_index=begin_index+n;

        *pa_ptr=m_buf+begin_index;

        if(end_index<=SIZE) {
            // No B part.
            *na_ptr=n;
            *pb_ptr=nullptr;
            *nb_ptr=0;
        } else {
            *na_ptr=SIZE-begin_index;

            *pb_ptr=m_buf;
            *nb_ptr=end_index-SIZE;
        }

        RVAL(m_last_wn)=n;
        return true;
    }

    // Commit N items previously allocated with ProducerAllocate.
    void ProducerUnlock(size_t n) {
        ASSERT(n<=RVAL(m_last_wn));

        RVAL(m_wv).fetch_add(n,RMO_ACQ_REL);
        RVAL(m_last_wn)=0;
    }

    // Lock all available items for reading. Returns false if there
    // are no items available, or fills in *PA_PTR/*NA_PTR and
    // *PB_PTR/*NB_PTR with pointer to and count of items
    // respectively in each portion.
    bool ConsumerLock(const T **pa_ptr,size_t *na_ptr,const T **pb_ptr,size_t *nb_ptr) {
        ASSERT(RVAL(m_last_rn)==0);

        uint64_t rv=RVAL(m_rv).load(RMO_ACQUIRE);
        uint64_t wv=RVAL(m_wv).load(RMO_ACQUIRE);

        ASSERT(wv>=rv);
        size_t used=(size_t)(wv-rv);

        if(used==0) {
            return false;
        }

        size_t begin_index=(size_t)(rv%SIZE);
        size_t end_index=begin_index+used;

        *pa_ptr=m_buf+begin_index;
        
        if(end_index<=SIZE) {
            // No B part.
            *na_ptr=used;
            *pb_ptr=nullptr;
            *nb_ptr=0;
        } else {
            *na_ptr=SIZE-begin_index;

            *pb_ptr=m_buf;
            *nb_ptr=end_index-SIZE;
        }

        RVAL(m_last_rn)=used;

        return true;
    }

    void ConsumerUnlock(size_t n) {
        ASSERT(n<=RVAL(m_last_rn));

        RVAL(m_rv).fetch_add(n,RMO_ACQ_REL);
        RVAL(m_last_rn)=0;
    }
protected:
private:
    // Shared part (read-only)
    T *m_buf=nullptr;

    // Consumer part
    std::atomic<uint64_t> m_rv{0};
    RVAR(size_t) m_last_rn=0;

    // Producer part
#include <shared/pushwarn_padded_struct.h>
    alignas(64) std::atomic<uint64_t> m_wv{0};
#include <shared/popwarn.h>
    RVAR(size_t) m_last_wn=0;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
