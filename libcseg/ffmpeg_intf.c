/**
 * This file is part of ffmpeg_ivr
 * 
 * Copyright (C) 2016  OpenSight (www.opensight.cn)
 * 
 * ffmpeg_ivr is an extension of ffmpeg to implements the new feature for IVR
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/



    
#include "min_cached_segment.h"




static int cseg_write_header(AVFormatContext *s)
{
    return 0;
}




static int cseg_write_packet(AVFormatContext *s, AVPacket *pkt)
{
    return 0;
}





static int cseg_write_trailer(struct AVFormatContext *s)
{
    return 0;
}





#define OFFSET(x) offsetof(CachedSegmentContext, x)
#define E AV_OPT_FLAG_ENCODING_PARAM
static const AVOption options[] = {
    {"start_number",  "set first number in the sequence",        OFFSET(start_sequence),AV_OPT_TYPE_INT64,  {.i64 = 0},     0, INT64_MAX, E},
    {"cseg_time",      "set segment length in seconds",           OFFSET(time),    AV_OPT_TYPE_DOUBLE,  {.dbl = 10},     0, FLT_MAX, E},
    {"cseg_list_size", "set maximum number of the cache list",  OFFSET(max_nb_segments),    AV_OPT_TYPE_INT,    {.i64 = 3},     1, INT_MAX, E},
    {"cseg_ts_options","set hls mpegts list of options for the container format used for hls", OFFSET(format_options_str), AV_OPT_TYPE_STRING, {.str = NULL},  0, 0,    E},
    {"cseg_seg_size",  "set maximum segment size in bytes",        OFFSET(max_seg_size),AV_OPT_TYPE_INT,  {.i64 = 10485760},     0, INT_MAX, E},
    {"start_ts",      "set start timestamp (in seconds) for the first segment", OFFSET(start_ts),    AV_OPT_TYPE_DOUBLE,  {.dbl = -1.0},     -1.0, DBL_MAX, E},
    {"cseg_cache_time", "set min cache time in seconds for writer pause", OFFSET(pre_recoding_time),    AV_OPT_TYPE_DOUBLE,  {.dbl = 0},     0, DBL_MAX, E},
    {"writer_timeout",     "set timeout (in seconds) of writer I/O operations", OFFSET(writer_timeout),     AV_OPT_TYPE_INT, { .i64 = 30 },         -1, INT_MAX, .flags = E },
    {"cseg_flags",     "set flags affecting cached segement working policy", OFFSET(flags), AV_OPT_TYPE_FLAGS, {.i64 = 0 }, 0, UINT_MAX, E, "flags"},
    {"nonblock",   "never blocking in the write_packet() when the cached list is full, instead, dicard the eariest segment", 0, AV_OPT_TYPE_CONST, {.i64 = CSEG_FLAG_NONBLOCK }, 0, UINT_MAX,   E, "flags"},

    { NULL },
};

static const AVClass cseg_class = {
    .class_name = "cached_segment muxer",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};


AVOutputFormat ff_cached_segment_muxer = {
    .name           = "cseg",
    .long_name      = "OpenSight cached segment muxer",
    .priv_data_size = sizeof(CachedSegmentContext),
    .audio_codec    = AV_CODEC_ID_AAC,
    .video_codec    = AV_CODEC_ID_H264,
    .flags          = AVFMT_NOFILE,
    .write_header   = cseg_write_header,
    .write_packet   = cseg_write_packet,
    .write_trailer  = cseg_write_trailer,
    .priv_class     = &cseg_class,
};






#define REGISTER_MUXER(x)                                            \
    {                                                                   \
        extern AVOutputFormat ff_##x##_muxer;                           \
        av_register_output_format(&ff_##x##_muxer);                 \
    }
    

#define REGISTER_CSEG_WRITER(x)                                            \
    {                                                                   \
        extern CachedSegmentWriter cseg_##x##_writer;                           \
        register_segment_writer(&cseg_##x##_writer);                 \
    }




void ffmpeg_libcseg_register(void)
{
    static int initialized = 0;

    if (initialized)
        return;
    initialized = 1;
    
    REGISTER_CSEG_WRITER(file);
    REGISTER_CSEG_WRITER(dummy);
    REGISTER_CSEG_WRITER(ivr);    
    
    REGISTER_MUXER(cached_segment);

}