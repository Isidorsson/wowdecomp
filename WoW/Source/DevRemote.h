#ifndef WOW_SOURCE_DEVREMOTE_H
#define WOW_SOURCE_DEVREMOTE_H

// Local development aid, not part of the original client: a named pipe (\\.\pipe\wowdecomp-dev) that runs Lua or
// console commands on the main thread so tools/wowctl can drive the client. Only built with WOWDECOMP_DEV_REMOTE.
void DevRemoteInitialize();

#endif
