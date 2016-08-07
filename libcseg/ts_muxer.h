//
// Created by hyt on 8/2/16.
//

#ifndef FFMPEG_IVR_TS_MUXER_H_H
#define FFMPEG_IVR_TS_MUXER_H_H

#include "stdint.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AV_STREAM_TYPE_AUDIO,
    AV_STREAM_TYPE_VIDEO
}av_stream_type_t;

typedef enum {
    AV_STREAM_CODEC_H264,
    AV_STREAM_CODEC_AAC
}av_stream_codec_t;

typedef struct {
    av_stream_type_t type;
    av_stream_codec_t codec;
    uint32_t audio_sample_rate;
    uint8_t  audio_channel_count;  // https://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Channel_Configurations
}av_stream_t;


typedef struct {
    uint8_t stream_count;
    av_stream_t** streams;
}av_context_t;

#define AV_PACKET_FLAGS_KEY  0x01


/**
 * Undefined timestamp value
 *
 * Usually reported by demuxer that work on containers that do not provide
 * either pts or dts.
 */

#define   NOPTS_VALUE          ((int64_t)-1)

#define   TS_TIME_BASE        ((int64_t)90000)

typedef struct {
    int stream_index;
    uint8_t   flags;  // flag: bit8 for is_sync
    int64_t  pts;   // in 90khz
    int64_t  dts;   // -1 if not present
    uint8_t* data;  // for h264, NALU starts with 0x00000001
    size_t  size;
}av_packet_t;


typedef struct _ts_muxer ts_muxer_t;

typedef int (*avio_write_func)(void* avio_context, const uint8_t* buf, size_t size);

ts_muxer_t* new_ts_muxer(av_context_t*);
void free_ts_muxer(ts_muxer_t*);

// to unset give NULL
int ts_muxer_set_avio_context(ts_muxer_t* ts_muxer, void*, avio_write_func);

// ask to write PAT PMT
int ts_muxer_write_header(ts_muxer_t*);
int ts_muxer_write_packet(ts_muxer_t*, av_packet_t*);
int ts_muxer_write_trailer(ts_muxer_t*);


#ifdef __cplusplus
}
#endif

#endif //FFMPEG_IVR_TS_MUXER_H_H
