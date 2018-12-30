#ifndef HEADER_85E7DE82F7DF4FF7AD8A330E6261A2E0// -*- mode:c++ -*-
#define HEADER_85E7DE82F7DF4FF7AD8A330E6261A2E0

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <vector>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// "BeebLink" is not two words.
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <shared/enum_decl.h>
#include "BeebLink.inl"
#include <shared/enum_end.h>

class BeebLink {
public:
    BeebLink();
    ~BeebLink();

    // Called when BBC writes a byte with handshaking disabled.
    void Reset();

    // Called when BBC writes a byte with handshaking enabled.
    //
    // Return <0 to indicate the byte was accepted, or a byte value to
    // indicate the AVR immediately wrote that byte in response.
    int HandleWrite(uint8_t beeb_write_value);
protected:
private:
    BeebLinkState m_state;

    uint8_t m_type;
    uint32_t m_size;
    std::vector<uint8_t> m_payload;
    size_t m_index;

    // Return value is first byte of response.
    int HandleBeebToServerPayload();
    void HandleRequestAVR();
    void ErrorResponse(uint8_t code,const char *message);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
