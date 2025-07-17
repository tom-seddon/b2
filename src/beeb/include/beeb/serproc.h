#ifndef HEADER_E81A9A399B9F4CEC9C472D77C905F64C // -*- mode:c++ -*-
#define HEADER_E81A9A399B9F4CEC9C472D77C905F64C

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>

#include <shared/enum_decl.h>
#include "serproc.inl"
#include <shared/enum_end.h>

union M6502Word;
struct MC6850;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Transmit = ACIA->SERPROC->Device (Sink)

// Receive = Device (Source)->SERPROC->ACIA

// TODO: maybe the base classes are overkill, and could be replaced with
// std::function...?

// Serial data sources and sinks will may to bear in mind thread safety.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SerialDataSource : public std::enable_shared_from_this<SerialDataSource> {
  public:
    SerialDataSource() = default;
    virtual ~SerialDataSource();

    SerialDataSource(const SerialDataSource &) = delete;
    SerialDataSource &operator=(const SerialDataSource &) = delete;
    SerialDataSource(SerialDataSource &&) = delete;
    SerialDataSource &operator=(SerialDataSource &&) = delete;

    virtual uint8_t GetNextByte() = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SerialDataSink : public std::enable_shared_from_this<SerialDataSink> {
  public:
    SerialDataSink() = default;
    virtual ~SerialDataSink();

    SerialDataSink(const SerialDataSink &) = delete;
    SerialDataSink &operator=(const SerialDataSink &) = delete;
    SerialDataSink(SerialDataSink &&) = delete;
    SerialDataSink &operator=(SerialDataSink &&) = delete;

    virtual void AddByte(uint8_t value) = 0;

  protected:
  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SERPROC {
    struct ControlRegisterBits {
        SERPROCBaudRate tx_baud : 3;
        SERPROCBaudRate rx_baud : 3;
        uint8_t rs423 : 1;
        uint8_t motor : 1;
    };

    union ControlRegister {
        ControlRegisterBits bits;
        uint8_t value;
    };
    CHECK_SIZEOF(ControlRegister, 1);

    ControlRegister control = {};
    uint8_t clock = 0;
    uint8_t tx_clock_mask = 0;
    uint8_t rx_clock_mask = 0;

    uint8_t tx_byte = 0;

    std::shared_ptr<SerialDataSource> source;
    std::shared_ptr<SerialDataSink> sink;

    MC6850 *acia = nullptr;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Indexed by SERPROCBaudRate.
extern const unsigned SERPROC_BAUD_RATES[8];

void WriteSERPROC(void *serproc, M6502Word addr, uint8_t value);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void UpdateSERPROC(SERPROC *serproc, MC6850 *mc6850);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
