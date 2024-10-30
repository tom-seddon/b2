#include <shared/system.h>
#include <shared/debug.h>
#include <shared/log.h>
#include <beeb/PCD8572.h>
#include <beeb/Trace.h>
#include <6502/6502.h>
#include <unordered_map>

#include <shared/enum_def.h>
#include <beeb/PCD8572.inl>
#include <shared/enum_end.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LOG_TAGGED_DEFINE(EEPROM, "nvram", "EEPROM", &log_printer_stdout_and_debugger, true);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// https://github.com/stardot/b-em/blob/master/src/compactcmos.c

// https://github.com/bitshifters/bbc-documents/blob/master/ICs/PCD8572%201%20Kbit%20EEPROM/PCD8572-Microchip.pdf

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if BBCMICRO_TRACE
void SetPCD8572Trace(PCD8572 *p, Trace *t) {
    p->t = t;
}
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if 0 //BUILD_TYPE_Debug
#define ELOG_LOGF LOGF
#else
#define ELOG_LOGF(FMT, ...) ((void)0)
#endif

#define ELOG(...)                              \
    BEGIN_MACRO {                              \
        ELOG_LOGF(EEPROM, __VA_ARGS__);        \
        TRACEF(p->t, "EEPROM - " __VA_ARGS__); \
    }                                          \
    END_MACRO

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#if PCD8572_MOS510_DEBUG
static const std::unordered_map<uint32_t, std::string> g_mos510_names = {
    {0x9ea9, "i2cStartDataTransfer"},
    {0x9eeb, "i2cStopDataTransfer"},
    {0x9e92, "i2cSetClockHigh"},
    {0x9e9d, "i2cSetClockLow"},
    {0x9ef8, "i2cSetDataHigh"},
    {0x9ec3, "i2cSetDataLow"},
    {0x9fa1, "i2cTransmitByteAndReceiveBit - set data line as input"},
    {0x9fb4, "i2cTransmitByteAndReceiveBit - set data line as output"},
    {0x9f76, "i2cReadEEPROMByte - restore old DDRB"},
};
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// If outputting data, return the desired output. If inputting, return data.

static bool ReceivedValue(PCD8572 *p, bool clk, bool data) {
    if (p->oclk && !clk) {
        if (p->value_mask == 0) {
            p->value_mask = 0x80;
        }

        ELOG("Received bit: %d (mask=$%02x)", data, p->value_mask);

        if (data) {
            p->value |= p->value_mask;
        } else {
            p->value &= ~p->value_mask;
        }

        p->value_mask >>= 1;
        if (p->value_mask == 0) {
            ELOG("Received value: %-3u $%02X %%%s %s", p->value, p->value, BINARY_BYTE_STRINGS[p->value], ASCII_BYTE_STRINGS[p->value]);
            return true;
        }
    }

    return false;
}

static void SendAcknowledge(PCD8572 *p, PCD8572State next_state) {
    p->state = PCD8572State_SendAcknowledge;
    p->next_state = next_state;
}

void UpdatePCD8572(PCD8572 *p, bool clk, bool data) {
#if PCD8572_MOS510_DEBUG
    if (clk != p->oclk || data != p->odata) {
        uint16_t pc = 0;
        const char *symbol = "?";
        if (p->cpu) {
            pc = p->cpu->opcode_pc.w;
            auto &&it = g_mos510_names.find(pc);
            if (it != g_mos510_names.end()) {
                symbol = it->second.c_str();
            }
        }

        ELOG("%s: clk=%d->%d data=%d->%d: pc=$%04x (%s)", GetPCD8572StateEnumName(p->state), p->oclk, clk, p->odata, data, pc, symbol);
    }
#endif

    if (p->oclk && clk) {
        if (p->odata && !data) {
            ELOG("Got START condition");
            p->value_mask = 0;
            p->state = PCD8572State_StartReceiveSlaveAddress;
        } else if (!p->odata && data) {
            ELOG("Got STOP condition");
            p->state = PCD8572State_Idle;
        }
    }

    switch (p->state) {
    default:
        ASSERT(false);
        break;

    case PCD8572State_Idle:
        //// Enter Receive mode on START condition.
        break;

    case PCD8572State_StartReceiveSlaveAddress:
        if (!clk) {
            p->state = PCD8572State_ReceiveSlaveAddress;
        }
        break;

    case PCD8572State_ReceiveSlaveAddress:
        if (ReceivedValue(p, clk, data)) {
            if ((p->value & 0xe) != 0) {
                // Wrong address. Ignore.
                p->state = PCD8572State_Idle;
                break;
            }

            if (p->value & 1) {
                // Alternate read mode.
                SendAcknowledge(p, PCD8572State_SendData);
            } else {
                // Read mode/erase+write mode.
                SendAcknowledge(p, PCD8572State_ReceiveWordAddress);
            }
        }
        break;

    case PCD8572State_SendAcknowledge:
        if (clk) {
            p->data_output = false;
        } else if (p->oclk && !clk) {
            p->state = p->next_state;
            p->next_state = PCD8572State_Idle;
        }
        break;

    case PCD8572State_ReceiveWordAddress:
        if (ReceivedValue(p, clk, data)) {
            p->addr = p->value;
            ELOG("received address: %-3u ($%02x)", p->addr, p->addr);
            SendAcknowledge(p, PCD8572State_ReceiveData);
        }
        break;

    case PCD8572State_ReceiveData:
        if (ReceivedValue(p, clk, data)) {
            ELOG("write: address: %-3u ($%02x); value: %-3u $%02x %%%s %s", p->addr, p->addr, p->value, p->value, BINARY_BYTE_STRINGS[p->value], ASCII_BYTE_STRINGS[p->value]);
            p->ram[p->addr & 0x7f] = p->value;
            ++p->addr;

            SendAcknowledge(p, PCD8572State_ReceiveData);
        }
        break;

    case PCD8572State_SendData:
        if (p->value_mask == 0) {
            p->value = p->ram[p->addr & 0x7f];
            ELOG("read: address: %-3u ($%02x); value: %-3u $%02x %%%s %s", p->addr, p->addr, p->value, p->value, BINARY_BYTE_STRINGS[p->value], ASCII_BYTE_STRINGS[p->value]);

            p->value_mask = 0x80;
        }

        if (!p->oclk && clk) {
            p->data_output = !!(p->value & p->value_mask);
            ELOG("Sent bit: %d (mask=$%02x)", p->data_output, p->value_mask);
        }

        if (p->oclk && !clk) {
            p->value_mask >>= 1;
            if (p->value_mask == 0) {
                p->state = PCD8572State_ReceiveAcknowledge;
            }
        }
        break;

    case PCD8572State_ReceiveAcknowledge:
        if (p->oclk && clk && !data) {
            ++p->addr;
            p->state = PCD8572State_SendData;
        }
        break;
    }

    p->oclk = clk;
    p->odata = data;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
