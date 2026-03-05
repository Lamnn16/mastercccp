/**
 * @file state_machines.cpp
 * @brief Phase 4 — State Machines: The Backbone of Embedded Control Flow
 *
 * State machines are how embedded firmware handles complex sequences:
 *   - Protocol framing (waiting for SOF, reading length, reading data, CRC check)
 *   - Motor control (idle, accelerating, running, braking, fault)
 *   - Connectivity (disconnected, connecting, authenticating, connected, error)
 *
 * Three implementations, from simple to industrial:
 *   1. switch/case FSM (simple, readable for small machines)
 *   2. Table-driven FSM (scalable, easy to add states)
 *   3. Hierarchical State Machine (HSM) preview
 *
 * BUILD:  cmake --build build --target p4_state_machines
 */

#include "embedded_types.h"
#include <cstdio>
#include <array>

/* ═══════════════════════════════════════════════════════════════════════════
 * UART FRAME PARSER — demonstrates a practical protocol FSM
 *
 * Frame format: [SOF=0xAA][LEN=1byte][PAYLOAD=LEN bytes][CRC=1byte]
 * ═══════════════════════════════════════════════════════════════════════════ */
class UartFrameParser {
public:
    enum class State : uint8_t {
        WAIT_SOF,
        WAIT_LEN,
        WAIT_PAYLOAD,
        WAIT_CRC,
        FRAME_DONE,
        FRAME_ERROR,
    };

    static constexpr uint8_t SOF_BYTE   = 0xAAU;
    static constexpr uint8_t MAX_PAYLOAD = 32U;

    struct Frame {
        uint8_t payload[MAX_PAYLOAD];
        uint8_t len;
        bool    valid;
    };

    UartFrameParser() { reset(); }

    /* Feed one byte at a time — mirrors how an ISR would call this */
    void feed(uint8_t byte)
    {
        switch (state_) {
        case State::WAIT_SOF:
            if (byte == SOF_BYTE) { state_ = State::WAIT_LEN; crc_ = 0U; }
            break;

        case State::WAIT_LEN:
            if (byte == 0U || byte > MAX_PAYLOAD) { state_ = State::FRAME_ERROR; break; }
            expected_len_ = byte; rx_pos_ = 0; crc_ ^= byte;
            state_ = State::WAIT_PAYLOAD;
            break;

        case State::WAIT_PAYLOAD:
            payload_[rx_pos_++] = byte;
            crc_ ^= byte;
            if (rx_pos_ == expected_len_) { state_ = State::WAIT_CRC; }
            break;

        case State::WAIT_CRC:
            state_ = (byte == crc_) ? State::FRAME_DONE : State::FRAME_ERROR;
            break;

        case State::FRAME_DONE:
        case State::FRAME_ERROR:
            break;   /* stay until reset() */
        }
    }

    bool done()  const { return state_ == State::FRAME_DONE; }
    bool error() const { return state_ == State::FRAME_ERROR; }

    Frame get_frame() const
    {
        Frame f{};
        for (uint8_t i = 0; i < expected_len_; i++) f.payload[i] = payload_[i];
        f.len   = expected_len_;
        f.valid = done();
        return f;
    }

    void reset()
    {
        state_ = State::WAIT_SOF; rx_pos_ = 0;
        expected_len_ = 0; crc_ = 0;
    }

    const char *state_name() const
    {
        static constexpr const char *names[] = {
            "WAIT_SOF","WAIT_LEN","WAIT_PAYLOAD","WAIT_CRC","DONE","ERROR"
        };
        return names[static_cast<uint8_t>(state_)];
    }

private:
    State   state_;
    uint8_t payload_[MAX_PAYLOAD];
    uint8_t rx_pos_;
    uint8_t expected_len_;
    uint8_t crc_;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * TABLE-DRIVEN FSM — scales to 50+ states without giant switch
 * ═══════════════════════════════════════════════════════════════════════════ */
enum class ConnState : uint8_t { IDLE, CONNECTING, AUTH, CONNECTED, ERROR, COUNT };
enum class ConnEvent : uint8_t { CONNECT, SUCCESS, FAIL, DISCONNECT, TIMEOUT, COUNT };

typedef ConnState (*trans_fn_t)();

static ConnState on_idle_connect()    { printf("  FSM: IDLE→CONNECTING\n");    return ConnState::CONNECTING; }
static ConnState on_conn_success()    { printf("  FSM: CONNECTING→AUTH\n");    return ConnState::AUTH;       }
static ConnState on_auth_success()    { printf("  FSM: AUTH→CONNECTED\n");     return ConnState::CONNECTED;  }
static ConnState on_any_fail()        { printf("  FSM: →ERROR\n");             return ConnState::ERROR;      }
static ConnState on_error_timeout()   { printf("  FSM: ERROR→IDLE\n");         return ConnState::IDLE;       }
static ConnState on_connected_disc()  { printf("  FSM: CONNECTED→IDLE\n");     return ConnState::IDLE;       }
static ConnState on_noop()            { return ConnState::IDLE; /* placeholder */ }

/* Transition table [state][event] → handler */
static const trans_fn_t TRANS_TABLE
    [static_cast<uint8_t>(ConnState::COUNT)]
    [static_cast<uint8_t>(ConnEvent::COUNT)] =
{
    /* IDLE */       { on_idle_connect, on_noop,        on_noop,     on_noop,           on_noop         },
    /* CONNECTING */ { on_noop,         on_conn_success, on_any_fail, on_noop,           on_any_fail     },
    /* AUTH */       { on_noop,         on_auth_success, on_any_fail, on_noop,           on_any_fail     },
    /* CONNECTED */  { on_noop,         on_noop,         on_noop,     on_connected_disc, on_noop         },
    /* ERROR */      { on_noop,         on_noop,         on_noop,     on_noop,           on_error_timeout},
};

class ConnectionFsm {
public:
    void dispatch(ConnEvent ev)
    {
        uint8_t s = static_cast<uint8_t>(state_);
        uint8_t e = static_cast<uint8_t>(ev);
        state_ = TRANS_TABLE[s][e]();
    }

    ConnState state() const { return state_; }

private:
    ConnState state_ = ConnState::IDLE;
};

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 4 — Lesson 3: State Machines         ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    printf("── UART Frame Parser FSM ──\n");
    UartFrameParser parser;

    /* Build a valid frame: [AA][03][01 02 03][CRC] */
    uint8_t payload[] = { 0x01U, 0x02U, 0x03U };
    uint8_t crc = 0x03U ^ 0x01U ^ 0x02U ^ 0x03U;  /* len ^ payload XOR */
    uint8_t stream[] = { 0xAAU, 0x03U, 0x01U, 0x02U, 0x03U, crc };

    for (size_t i = 0; i < sizeof(stream); i++) {
        parser.feed(stream[i]);
        printf("  byte[%zu]=0x%02X → state: %s\n", i, stream[i], parser.state_name());
    }

    if (parser.done()) {
        auto f = parser.get_frame();
        printf("  Frame valid! payload (%u bytes): ", f.len);
        for (uint8_t i = 0; i < f.len; i++) printf("0x%02X ", f.payload[i]);
        printf("\n");
    }
    printf("\n");

    printf("── Table-driven Connection FSM ──\n");
    ConnectionFsm fsm;
    fsm.dispatch(ConnEvent::CONNECT);
    fsm.dispatch(ConnEvent::SUCCESS);
    fsm.dispatch(ConnEvent::SUCCESS);
    fsm.dispatch(ConnEvent::DISCONNECT);
    printf("\n");

    printf("EXERCISES:\n");
    printf("  1. Add a WAIT_SOF2 state that expects a second sync byte (0x55)\n");
    printf("     after 0xAA. This double-SOF is used in HDLC-like protocols.\n");
    printf("  2. Add a checksum-failed counter to UartFrameParser.\n");
    printf("     After 3 consecutive CRC failures, enter a LOCKED_OUT state.\n");
    printf("  3. Implement the ConnectionFsm with entry/exit actions:\n");
    printf("     on entering CONNECTED, start a keepalive timer.\n");
    printf("     On exiting, cancel it.\n");
    return 0;
}
