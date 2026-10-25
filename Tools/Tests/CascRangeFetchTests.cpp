#ifdef _WIN32

// Standard headers first: CascLib's strsafe.h poisons the C string functions they import.
#include <winsock2.h>

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include "Tools/Tests/Check.h"

#define __CASCLIB_SELF__
#include "CascLib.h"
#include "CascCommon.h"

// Tests for cmake/casclib-range-fetch.patch: ranged CDN reads in CascLib's HTTP stream.
namespace {

const char* const SLICE = "BLTE0123456789ab";

// One-connection HTTP server on 127.0.0.1 that records the request and sends a canned reply.
class LoopbackServer {
public:
    explicit LoopbackServer(std::string reply)
        : m_reply(std::move(reply)) {
        WSADATA wsd;
        WSAStartup(MAKEWORD(2, 2), &wsd);

        this->m_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address = {};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(this->m_listen, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        listen(this->m_listen, 4);

        int length = sizeof(address);
        getsockname(this->m_listen, reinterpret_cast<sockaddr*>(&address), &length);
        this->m_port = ntohs(address.sin_port);
        this->m_thread = std::thread([this] { this->Serve(); });
    }

    ~LoopbackServer() {
        closesocket(this->m_listen);
        this->m_thread.join();
    }

    std::string Url(const char* path) const { return "127.0.0.1:" + std::to_string(this->m_port) + path; }
    const std::string& Request() const { return this->m_request; }

private:
    void Serve() {
        SOCKET client = accept(this->m_listen, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            return;
        }

        char buffer[1024];
        while (this->m_request.find("\r\n\r\n") == std::string::npos) {
            int received = recv(client, buffer, sizeof(buffer), 0);
            if (received <= 0) {
                break;
            }
            this->m_request.append(buffer, received);
        }

        send(client, this->m_reply.data(), static_cast<int>(this->m_reply.size()), 0);

        // Hold the connection open until the client hangs up, as a kept-alive CDN would.
        while (recv(client, buffer, sizeof(buffer), 0) > 0) {
        }
        closesocket(client);
    }

    std::string m_reply;
    std::string m_request;
    SOCKET m_listen = INVALID_SOCKET;
    unsigned short m_port = 0;
    std::thread m_thread;
};

bool ReadRange(const std::string& url, ULONGLONG offset, char* buffer, DWORD length) {
    TFileStream* stream = FileStream_OpenFile(url.c_str(), BASE_PROVIDER_HTTP | STREAM_PROVIDER_FLAT);
    if (!stream) {
        return false;
    }

    bool ok = FileStream_Read(stream, &offset, buffer, length);
    FileStream_Close(stream);
    return ok;
}

bool RangeRefused(const char* header, size_t expectedLength) {
    CASC_MIME_RESPONSE response;
    response.expected_length = expectedLength;
    response.ParseResponse(header, strlen(header));
    return response.IsRangeRefused();
}

}

TEST(RangeRequestsAskForAnInclusiveByteSpan) {
    char request[0x200];
    ULONGLONG offset = 100;
    size_t length = HttpFormatRequest(request, sizeof(request), "/tpr/wow/data/00/17/archive", "cdn.test", &offset, 16);

    CHECK(length == strlen(request));
    CHECK(std::string(request) == "GET /tpr/wow/data/00/17/archive HTTP/1.1\r\nHost: cdn.test\r\nRange: bytes=100-115\r\nConnection: Keep-Alive\r\n\r\n");
}

TEST(RangeRequestsReachPastFourGigabytes) {
    char request[0x200];
    ULONGLONG offset = 0x100000000ull;

    HttpFormatRequest(request, sizeof(request), "/a", "h", &offset, 1);
    CHECK(std::string(request).find("Range: bytes=4294967296-4294967296\r\n") != std::string::npos);
}

TEST(WholeFileRequestsCarryNoRange) {
    char request[0x200];

    HttpFormatRequest(request, sizeof(request), "/a", "h", nullptr, 0);
    CHECK(std::string(request) == "GET /a HTTP/1.1\r\nHost: h\r\nConnection: Keep-Alive\r\n\r\n");
}

TEST(OnlyAPartialAnswerOfTheAskedLengthSatisfiesARange) {
    CHECK(!RangeRefused("HTTP/1.1 206 Partial Content\r\nContent-Length: 16\r\n\r\n", 16));
    CHECK(RangeRefused("HTTP/1.1 200 OK\r\nContent-Length: 268435419\r\n\r\n", 16));
    CHECK(RangeRefused("HTTP/1.1 206 Partial Content\r\nContent-Length: 32\r\n\r\n", 16));
    CHECK(RangeRefused("HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\n", 16));
}

TEST(RangeVerdictWaitsForTheWholeHeader) {
    CHECK(!RangeRefused("HTTP/1.1 200 OK\r\nContent-Le", 16));
}

TEST(WholeFileAnswersAreNeverRangeRefusals) {
    CHECK(!RangeRefused("HTTP/1.1 200 OK\r\nContent-Length: 16\r\n\r\n", CASC_INVALID_SIZE_T));
}

TEST(RangedReadsFetchOnlyTheAskedBytes) {
    LoopbackServer server(std::string("HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 100-115/268435419\r\nContent-Length: 16\r\n\r\n") + SLICE);
    char buffer[16] = {};

    CHECK(ReadRange(server.Url("/tpr/wow/data/00/17/archive"), 100, buffer, sizeof(buffer)));
    CHECK(memcmp(buffer, SLICE, sizeof(buffer)) == 0);
    CHECK(server.Request().find("GET /tpr/wow/data/00/17/archive HTTP/1.1\r\n") == 0);
    CHECK(server.Request().find("\r\nRange: bytes=100-115\r\n") != std::string::npos);
}

TEST(RangedReadsRefuseAShortPartialAnswer) {
    LoopbackServer server("HTTP/1.1 206 Partial Content\r\nContent-Length: 8\r\n\r\nBLTE0123");
    char buffer[16] = {};

    CHECK(!ReadRange(server.Url("/a"), 0, buffer, sizeof(buffer)));
}

TEST(RangedReadsAbandonAWholeFileAnswerInsteadOfBufferingIt) {
    // Promises 64 MB but sends 1 MB and stalls: buffering it would wait for the receive timeout.
    std::string reply = "HTTP/1.1 200 OK\r\nContent-Length: 67108864\r\n\r\n" + std::string(1 << 20, 'x');
    LoopbackServer server(reply);
    char buffer[16] = {};

    auto start = std::chrono::steady_clock::now();
    CHECK(!ReadRange(server.Url("/a"), 0, buffer, sizeof(buffer)));
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
}

#endif
