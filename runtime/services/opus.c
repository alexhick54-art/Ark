// hwopus: the Switch's Opus decoder service (nn::codec::HardwareOpusDecoder talks to it), on libopus.
// Layouts as documented on switchbrew (Audio services: hwopus). Ark's own.
//
// IHardwareOpusDecoderManager
//   0 OpenHardwareOpusDecoder(OpusParameters, u32 work size, handle) -> IHardwareOpusDecoder
//   1 GetWorkBufferSize(OpusParameters) -> u32
//   2 OpenHardwareOpusDecoderForMultiStream(u32 work size, handle, buffer: OpusMultiStreamParameters)
//   3 GetWorkBufferSizeForMultiStream(buffer: OpusMultiStreamParameters) -> u32
//   4 / 5 / 6 / 7: the same with the "Ex" parameters (+ use_large_frame_size); 8 / 9: GetWorkBufferSize*ExEx
// IHardwareOpusDecoder
//   0 / 2 DecodeInterleaved[ForMultiStream]Old(in A buffer, out B buffer) -> u32 consumed, u32 samples
//   1 / 3 SetContext[ForMultiStream](in buffer)
//   4 / 5 ...WithPerfOld -> + u64 time taken; 6 / 7 ...WithPerfAndResetOld(bool reset) -> same
//   8 / 9 DecodeInterleaved[ForMultiStream](bool reset) -> u32 consumed, u32 samples, u64 time taken
// Input: packets of {u32 size (big-endian), u32 final range (big-endian)} + `size` bytes of Opus data.
// Output: interleaved s16 PCM; `samples` counts per channel.
#include <opus/opus.h>
#include <opus/opus_multistream.h>
#include <stdlib.h>
#include <string.h>

#include "../hle.h"
#include "../ipc.h"

#define R_OPUS(desc) MAKE_RESULT(111, desc)  // (module 111: codec)

typedef struct {
    OpusDecoder *dec;
    OpusMSDecoder *ms;
    int channels, sample_rate;
} OpusState;

typedef struct {
    int32_t sample_rate, channel_count, total_stream_count, stereo_stream_count;
    uint8_t mappings[256];
} MsParams;

static uint32_t work_size(int channels, bool multistream) {
    // the game allocates this much transfer memory for the decoder; we keep our state on the host, so any size works
    uint32_t base = multistream ? 0x10000 : 0x4800;
    return base + 0x2000 * (uint32_t)(channels > 0 ? channels : 1);
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static IpcBuf *first_in(IpcReq *req) {
    IpcBuf *b = in_buf(req, 0);
    return b && b->size ? b : NULL;
}

static void decoder_on_close(Service *s) {
    OpusState *st = s->state;
    if (!st) return;
    if (st->dec) opus_decoder_destroy(st->dec);
    if (st->ms) opus_multistream_decoder_destroy(st->ms);
    free(st);
    s->state = NULL;
}

static void decode(Service *s, IpcReq *req, IpcResp *resp, bool reset, bool perf) {
    OpusState *st = s->state;
    IpcBuf *in = first_in(req), *out = out_buf(req, 0);
    if (!st || !in || !out) { resp->result = R_OPUS(6); return; }
    if (reset) {
        if (st->dec) opus_decoder_ctl(st->dec, OPUS_RESET_STATE);
        if (st->ms) opus_multistream_decoder_ctl(st->ms, OPUS_RESET_STATE);
    }
    const uint8_t *p = (const uint8_t *)(uintptr_t)in->addr;
    if (in->size < 8) { resp->result = R_OPUS(6); return; }
    uint32_t size = be32(p);
    if (8ull + size > in->size) { resp->result = R_OPUS(6); return; }
    int max_frames = (int)(out->size / (2u * (uint32_t)st->channels));
    uint64_t t0 = plat_now_ns();
    int n = st->ms ? opus_multistream_decode(st->ms, p + 8, (opus_int32)size, (opus_int16 *)(uintptr_t)out->addr, max_frames, 0)
                   : opus_decode(st->dec, p + 8, (opus_int32)size, (opus_int16 *)(uintptr_t)out->addr, max_frames, 0);
    if (n < 0) {
        static int warned;
        if (warned++ < 10) LOG("[opus] decode failed: %s (packet %u bytes)\n", opus_strerror(n), size);
        resp->result = R_OPUS(9);
        return;
    }
    OUT(uint32_t, 0) = 8 + size;      // consumed
    OUT(uint32_t, 4) = (uint32_t)n;   // samples per channel
    if (perf) { OUT(uint64_t, 8) = (plat_now_ns() - t0) / 1000; resp_size(resp, 16); }
    else resp_size(resp, 8);
}

static void decoder(Service *s, IpcReq *req, IpcResp *resp) {
    switch (req->cmd) {
    case 0: case 2: decode(s, req, resp, false, false); break;           // DecodeInterleaved[ForMultiStream]Old
    case 1: case 3: break;                                                // SetContext: nothing to restore
    case 4: case 5: decode(s, req, resp, false, true); break;            // ...WithPerfOld
    case 6: case 7: case 8: case 9:                                       // ...WithPerfAndResetOld / DecodeInterleaved
        decode(s, req, resp, IN(uint8_t, 0) != 0, true);
        break;
    default: LOG("[opus] IHardwareOpusDecoder cmd %u (ok)\n", req->cmd);
    }
}

static Service *open_single(int sample_rate, int channels, IpcResp *resp) {
    int err = 0;
    OpusDecoder *d = opus_decoder_create(sample_rate, channels, &err);
    if (!d) {
        LOG("[opus] decoder %d Hz x%d: %s\n", sample_rate, channels, opus_strerror(err));
        resp->result = R_OPUS(1);
        return NULL;
    }
    OpusState *st = calloc(1, sizeof *st);
    st->dec = d, st->channels = channels, st->sample_rate = sample_rate;
    Service *svc = service_new("IHardwareOpusDecoder", decoder, st);
    svc->on_close = decoder_on_close;
    return svc;
}

static Service *open_multi(const MsParams *p, IpcResp *resp) {
    int err = 0;
    OpusMSDecoder *d = opus_multistream_decoder_create(p->sample_rate, p->channel_count, p->total_stream_count,
                                                       p->stereo_stream_count, p->mappings, &err);
    if (!d) {
        LOG("[opus] multistream decoder %d Hz x%d (%d streams): %s\n", p->sample_rate, p->channel_count,
            p->total_stream_count, opus_strerror(err));
        resp->result = R_OPUS(1);
        return NULL;
    }
    OpusState *st = calloc(1, sizeof *st);
    st->ms = d, st->channels = p->channel_count, st->sample_rate = p->sample_rate;
    Service *svc = service_new("IHardwareOpusDecoder(multistream)", decoder, st);
    svc->on_close = decoder_on_close;
    return svc;
}

// Multistream parameters arrive in a buffer; the Ex form has 8 more bytes (use_large_frame_size + padding) before
// the channel mapping.
static bool ms_params(IpcReq *req, bool ex, MsParams *out) {
    IpcBuf *b = first_in(req);
    if (!b || b->size < 16) return false;
    const uint8_t *p = (const uint8_t *)(uintptr_t)b->addr;
    memcpy(out, p, 16);
    size_t moff = ex ? 24 : 16, n = b->size > moff ? b->size - moff : 0;
    memset(out->mappings, 0, sizeof out->mappings);
    memcpy(out->mappings, p + moff, n < 256 ? n : 256);
    return true;
}

SERVICE("hwopus", hwopus_manager) {
    (void)s;
    switch (req->cmd) {
    case 0: case 4: {  // OpenHardwareOpusDecoder[Ex]: params (8 or 16 bytes), u32 work size, transfer memory handle
        Service *d = open_single(IN(int32_t, 0), IN(int32_t, 4), resp);
        if (d) ipc_return_object(resp, d);
        break;
    }
    case 1: case 5: case 8: OUT(uint32_t, 0) = work_size(IN(int32_t, 4), false); resp_size(resp, 4); break;
    case 2: case 6: {
        MsParams p;
        if (!ms_params(req, req->cmd == 6, &p)) { resp->result = R_OPUS(6); break; }
        Service *d = open_multi(&p, resp);
        if (d) ipc_return_object(resp, d);
        break;
    }
    case 3: case 7: case 9: {
        MsParams p;
        int ch = ms_params(req, req->cmd != 3, &p) ? p.channel_count : 2;
        OUT(uint32_t, 0) = work_size(ch, true);
        resp_size(resp, 4);
        break;
    }
    default: LOG("[opus] hwopus cmd %u (ok)\n", req->cmd);
    }
}
