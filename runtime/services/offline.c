// Ark: services a single-player, offline console still answers. Ark's own.
//   friend:u  IServiceCreator: 0 CreateFriendService, 1 CreateNotificationService, 2 CreateDaemonSuspendSessionService
//             IFriendService: every query succeeds with nothing (no friends, no presence, empty lists)
//             INotificationService: 0 GetEvent (never signalled), 1 Clear, 2 Pop (no notifications)
//   csrng     0 GetRandomBytes(out buffer)
// Layouts per switchbrew (Friend services, SPL services).
#include <stdlib.h>
#include <string.h>

#include "../hle.h"
#include "../ipc.h"

#define R_FRIENDS_NO_NOTIFICATION MAKE_RESULT(121, 304)

static void zero_out_buffers(IpcReq *req) {
    for (int i = 0; i < req->nb; i++) if (req->b[i].size) memset((void *)(uintptr_t)req->b[i].addr, 0, req->b[i].size);
    for (int i = 0; i < req->nc; i++) if (req->c[i].size) memset((void *)(uintptr_t)req->c[i].addr, 0, req->c[i].size);
}

static void friend_service(Service *s, IpcReq *req, IpcResp *resp) {
    (void)s;
    // Counts and flags come back as 0 and output buffers are cleared: "no friends", "nothing blocked", "offline".
    zero_out_buffers(req);
    memset(resp->data, 0, 0x20);
    resp_size(resp, 8);
    static int logged;
    if (logged++ < 20) LOG("[friends] IFriendService cmd %u (offline: empty)\n", req->cmd);
}

static void notification_service(Service *s, IpcReq *req, IpcResp *resp) {
    (void)s;
    switch (req->cmd) {
    case 0: {  // GetEvent
        uint32_t w, r;
        kernel_create_event_pair(&w, &r);
        resp->copy_handles[resp->ncopy++] = r;
        break;
    }
    case 1: break;                                          // Clear
    case 2: resp->result = R_FRIENDS_NO_NOTIFICATION; break;  // Pop: nothing to pop
    default: LOG("[friends] INotificationService cmd %u (ok)\n", req->cmd);
    }
}

static void friend_creator(Service *s, IpcReq *req, IpcResp *resp) {
    (void)s;
    switch (req->cmd) {
    case 0: ipc_return_object(resp, service_new("IFriendService", friend_service, NULL)); break;
    case 1: ipc_return_object(resp, service_new("INotificationService", notification_service, NULL)); break;
    case 2: ipc_return_object(resp, service_new("IDaemonSuspendSessionService", friend_service, NULL)); break;
    default: LOG("[friends] IServiceCreator cmd %u (ok)\n", req->cmd);
    }
}

SERVICE("friend:u", friend_u) { friend_creator(s, req, resp); }
SERVICE("friend:v", friend_v) { friend_creator(s, req, resp); }
SERVICE("friend:m", friend_m) { friend_creator(s, req, resp); }
SERVICE("friend:s", friend_s) { friend_creator(s, req, resp); }
SERVICE("friend:a", friend_a) { friend_creator(s, req, resp); }

SERVICE("csrng", csrng) {
    (void)s;
    if (req->cmd != 0) { LOG("[csrng] cmd %u (ok)\n", req->cmd); return; }
    IpcBuf *b = out_buf(req, 0);
    if (!b) return;
    uint8_t *p = (uint8_t *)(uintptr_t)b->addr;
    for (uint64_t i = 0; i < b->size; i += 4) {
        uint32_t v = arc4random();
        memcpy(p + i, &v, b->size - i < 4 ? b->size - i : 4);
    }
}
