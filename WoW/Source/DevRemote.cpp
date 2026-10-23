#include "DevRemote.h"

#if defined(WOWDECOMP_DEV_REMOTE)

#include <windows.h>
#include <sddl.h>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include "Engine/Source/ConsoleBase/IConsole.h"
#include "Engine/Source/Event/Event.h"
#include "Engine/Source/Frame/CSimpleTop.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"

// One pipe per client process so wowctl can drive several clients at once.
static const char* const PIPE_PREFIX = "\\\\.\\pipe\\wowdecomp-dev-";
static const DWORD PIPE_BUFFER_SIZE = 64 * 1024;

// One request in flight at a time: the pipe thread parks it here and the main thread answers it on the next poll.
static std::mutex s_lock;
static std::condition_variable s_answered;
static std::string s_request;
static std::string s_response;
static bool s_pending;

static int32_t CollectPrint(lua_State* L) {
    std::string line;
    int32_t count = lua_gettop(L);

    for (int32_t i = 1; i <= count; i++) {
        lua_getglobal(L, "tostring");
        lua_pushvalue(L, i);
        lua_call(L, 1, 1);

        if (i > 1) {
            line += '\t';
        }

        const char* text = lua_tostring(L, -1);
        line += text ? text : "?";
        lua_pop(L, 1);
    }

    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushstring(L, line.c_str());
    lua_rawseti(L, -2, static_cast<int32_t>(lua_objlen(L, -2)) + 1);
    lua_pop(L, 1);
    return 0;
}

struct LuaJob {
    const std::string* code;
    std::string out;
};

static void FormatResults(lua_State* L, int32_t first, int32_t printed, std::string& out) {
    size_t lines = lua_objlen(L, printed);

    for (size_t i = 1; i <= lines; i++) {
        lua_rawgeti(L, printed, static_cast<int32_t>(i));
        out += lua_tostring(L, -1);
        out += '\n';
        lua_pop(L, 1);
    }

    int32_t last = lua_gettop(L);

    if (last >= first) {
        out += "=> ";

        for (int32_t i = first; i <= last; i++) {
            lua_getglobal(L, "tostring");
            lua_pushvalue(L, i);
            lua_call(L, 1, 1);
            out += i > first ? ", " : "";
            out += lua_tostring(L, -1);
            lua_pop(L, 1);
        }

        out += '\n';
    }
}

// Runs under lua_cpcall so an error anywhere, even in a __tostring while formatting, comes back as text.
static int32_t RunLuaProtected(lua_State* L) {
    LuaJob* job = static_cast<LuaJob*>(lua_touserdata(L, 1));
    const std::string& code = *job->code;
    lua_settop(L, 0);
    lua_newtable(L);
    const int32_t printed = 1;
    lua_pushvalue(L, printed);
    lua_pushcclosure(L, CollectPrint, 1);
    lua_setglobal(L, "print");

    std::string expression = "return " + code;
    int32_t status = luaL_loadbuffer(L, expression.c_str(), expression.size(), "=wowctl");

    if (status) {
        lua_pop(L, 1);
        status = luaL_loadbuffer(L, code.c_str(), code.size(), "=wowctl");
    }

    if (!status) {
        status = lua_pcall(L, 0, LUA_MULTRET, 0);
    }

    if (status) {
        const char* message = lua_tostring(L, -1);
        job->out += "error: ";
        job->out += message ? message : "unknown";
        job->out += '\n';
        lua_settop(L, printed);
    }

    FormatResults(L, printed + 1, printed, job->out);
    return 0;
}

// Runs like a REPL: tries the code as an expression first so `wowctl lua UnitName("player")` returns a value,
// and swaps in a print that collects into the response instead of the chat frame.
static std::string RunLua(const std::string& code) {
    lua_State* L = FrameScript_GetContext();

    if (!L) {
        return "error: Lua is not initialized yet\n";
    }

    int32_t top = lua_gettop(L);
    lua_getglobal(L, "print");
    LuaJob job = { &code, "" };

    if (lua_cpcall(L, RunLuaProtected, &job)) {
        const char* message = lua_tostring(L, -1);
        job.out += "error: ";
        job.out += message ? message : "unknown";
        job.out += '\n';
    }

    lua_pushvalue(L, top + 1);
    lua_setglobal(L, "print");
    lua_settop(L, top);
    return job.out.empty() ? "ok\n" : job.out;
}

// Internal unit state that Lua can't see, for checking what this client thinks the player or its target is doing.
static std::string DescribeUnit(const std::string& which) {
    CGUnit_C* unit = nullptr;

    if (which.empty() || which == "player") {
        unit = ClntObjMgrGetActivePlayerObj();
    } else if (which == "target") {
        unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(CGGameUI::m_lockedTarget, TYPE_UNIT, __FILE__, __LINE__));
    } else {
        return "error: state takes 'player' or 'target'\n";
    }

    if (!unit) {
        return "error: no " + (which.empty() ? std::string("player") : which) + "\n";
    }

    char buffer[512];
    snprintf(buffer, sizeof(buffer),
        "{\"standState\":%u,\"clientStandState\":%u,\"animStandState\":%u,\"baseAnim\":%u,\"torsoAnim\":%u,\"pendingAnim\":%d,\"awaitingEndAnim\":%d,\"moveFlags\":\"0x%08X\",\"animationState\":\"0x%08X\",\"castSpellId\":%u}\n",
        unit->m_unit->m_unitFieldBytes1[0],
        static_cast<uint32_t>(unit->GetClientStandState()),
        static_cast<uint32_t>(unit->m_animStandState),
        unit->GetCurrentBaseAnimId(),
        unit->GetCurrentTorsoAnimId(),
        unit->m_pendingAnimId,
        unit->m_awaitingEndAnimId,
        unit->m_movement->m_flags,
        unit->m_animationState,
        unit->m_castSpellId);

    return buffer;
}

static std::string Dispatch(const std::string& request) {
    size_t space = request.find(' ');
    std::string command = request.substr(0, space);
    std::string payload = space == std::string::npos ? "" : request.substr(space + 1);

    if (command == "lua") {
        return RunLua(payload);
    }

    if (command == "console") {
        ConsoleCommandExecute(payload.c_str(), 0);
        return "ok\n";
    }

    if (command == "ping") {
        return "pong\n";
    }

    if (command == "state") {
        return DescribeUnit(payload);
    }

    return "error: unknown command '" + command + "' (expected lua, console, state or ping)\n";
}

static int32_t DevRemotePoll(const void* data, void* param) {
    std::lock_guard<std::mutex> guard(s_lock);

    if (s_pending) {
        // A remote command is user activity. s_eventTime only moves on real input and starts at 0, so a client
        // driven purely over the pipe looks idle for the whole system uptime and the idle check logs it out on
        // entering the world.
        CSimpleTop::s_eventTime = OsGetAsyncTimeMs();
        s_response = Dispatch(s_request);
        s_pending = false;
        s_answered.notify_one();
    }

    return 1;
}

static bool ReadRequest(HANDLE pipe, std::string& request) {
    char buffer[4096];
    request.clear();

    for (;;) {
        DWORD read = 0;
        BOOL ok = ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr);
        request.append(buffer, read);

        if (ok) {
            return true;
        }

        if (GetLastError() != ERROR_MORE_DATA) {
            return false;
        }
    }
}

static void ServePipe() {
    // The default DACL only admits the creating token, so tools running sandboxed or at another integrity level get
    // access denied. Allow local read/write with a low integrity label; PIPE_REJECT_REMOTE_CLIENTS still keeps it local.
    PSECURITY_DESCRIPTOR descriptor = nullptr;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GRGW;;;WD)S:(ML;;NW;;;LW)", SDDL_REVISION_1, &descriptor, nullptr)) {
        return;
    }

    SECURITY_ATTRIBUTES attributes = { sizeof(attributes), descriptor, FALSE };
    std::string name = PIPE_PREFIX + std::to_string(GetCurrentProcessId());

    for (;;) {
        HANDLE pipe = CreateNamedPipeA(name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 0, &attributes);

        if (pipe == INVALID_HANDLE_VALUE) {
            return;
        }

        if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) {
            std::string request;

            if (ReadRequest(pipe, request)) {
                std::unique_lock<std::mutex> guard(s_lock);
                s_request = request;
                s_pending = true;
                s_answered.wait(guard, [] { return !s_pending; });
                std::string response = s_response;
                guard.unlock();

                DWORD written = 0;
                WriteFile(pipe, response.data(), static_cast<DWORD>(response.size()), &written, nullptr);
                FlushFileBuffers(pipe);
            }
        }

        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
    }
}

void DevRemoteInitialize() {
    EventRegister(EVENT_ID_POLL, DevRemotePoll);
    std::thread(ServePipe).detach();
}

#else

void DevRemoteInitialize() {
}

#endif
