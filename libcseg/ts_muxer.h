//
// Created by hyt on 8/2/16.
//

#ifndef TS_MUXER_H
#define TS_MUXER_H

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "libcseg.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _ts_muxer ts_muxer_t;

typedef int (*avio_write_func)(void* avio_context, const uint8_t* buf, size_t size);

ts_muxer_t* new_ts_muxer(av_stream_t* av_streams, int av_stream_count);
void free_ts_muxer(ts_muxer_t* ts_muxer);

// to unset give NULL
int ts_muxer_set_avio_context(ts_muxer_t* ts_muxer, void* avio_context, avio_write_func write);

// ask to write PAT PMT
int ts_muxer_write_header(ts_muxer_t* ts_muxer);
int ts_muxer_write_packet(ts_muxer_t* ts_muxer, av_packet_t* av_packet);


#ifdef __cplusplus
}
#endif

#endif //FFMPEG_IVR_TS_MUXER_H_H
