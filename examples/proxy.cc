#include <thread>
#include <iostream>
#include <atomic>
#include <csignal>
#include <chrono>

#include <uvgrtp/lib.hh>

constexpr uint16_t RCV_PORT = 8890;
constexpr uint16_t SND_PORT = 8891;

constexpr char RCV_ADDRESS[] = "127.0.0.1";
constexpr char SND_ADDRESS[] = "127.0.0.1";

std::atomic_bool g_shutdown{false};

#if defined(WIN32)
BOOL WINAPI ConsoleControlHandler(DWORD type)
{
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT)
        g_shutdown = true;

    return TRUE;
}
#else
void SignalHandler(int signal)
{
    std::cout << "signal " << signal << " received" << std::endl;
    g_shutdown = true;
}
#endif

struct uvgrtpCtx {
    uvgrtp::context ctx;
    uvgrtp::session* sndSession = nullptr;
    uvgrtp::session* rcvSession = nullptr;
    uvgrtp::media_stream* sndStream = nullptr;
    uvgrtp::media_stream* rcvStream = nullptr;

    uvgrtpCtx() {
        rcvSession = ctx.create_session(RCV_ADDRESS);
        sndSession = ctx.create_session(SND_ADDRESS);
        if (rcvSession && sndSession) {
            rcvStream = rcvSession->create_stream(RCV_PORT, RTP_FORMAT_H264, RCE_RECEIVE_ONLY | RCE_NO_H26X_PREPEND_SC);
            sndStream = sndSession->create_stream(SND_PORT, RTP_FORMAT_H264, RCE_SEND_ONLY);
        }
    }

    ~uvgrtpCtx() {
        if (rcvSession) {
            if (rcvStream)
                rcvSession->destroy_stream(rcvStream);

            ctx.destroy_session(rcvSession);
        }
        if (sndSession) {
            if (sndStream)
                sndSession->destroy_stream(sndStream);

            ctx.destroy_session(sndSession);
        }
    }
};

std::string naluType(uint8_t nal_unit_type) {
    switch (nal_unit_type) {
        case 1: return "Slice";
        case 5: return "IDR slice";
        case 6: return "SEI";
        case 7: return "SPS";
        case 8: return "PPS";
        case 9: return "AUD";
        default: return std::to_string(nal_unit_type);
    }
}

void rtp_receive_hook(void* arg, uvgrtp::frame::rtp_frame* nalu) {
    uvgrtpCtx* ctx = reinterpret_cast<uvgrtpCtx*>(arg);
    if (nalu) {
        if (nalu->payload_len > 0) {
            std::cout << "NALU type " << naluType(nalu->payload[0] & 0x1f) << ", " << nalu->payload_len << " bytes" << std::endl;
            if (ctx->sndStream->push_frame(nalu->payload, nalu->payload_len, RTP_NO_H26X_SCL) != RTP_OK)
                std::cerr << "Failed to send frame\n";
        }
        (void)uvgrtp::frame::dealloc_frame(nalu);
    }
}

int main() {
    uvgrtpCtx ctx;

    if (!ctx.rcvStream || !ctx.sndStream) {
        std::cerr << "Failed to initialize send/recv streams (ports already taken?)\n";
        return -1;
    }

    if (ctx.rcvStream->install_receive_hook(&ctx, rtp_receive_hook) != RTP_OK) {
        std::cerr << "Failed to install RTP recv hook\n";
        return -1;
    }

#if defined(WIN32)
    SetConsoleCtrlHandler(&ConsoleControlHandler, TRUE);
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#else
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
#endif

    while (!g_shutdown)
        std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}

