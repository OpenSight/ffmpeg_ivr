#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <libavutil/log.h>
#include "ts_muxer.h"


// TS payload type
#define TS_MUXER_PAYLOAD_PAT          0
#define TS_MUXER_PAYLOAD_PMT          1
#define TS_MUXER_PAYLOAD_EMPTY_PES    2
#define TS_MUXER_PAYLOAD_H264_PES     10
#define TS_MUXER_PAYLOAD_AAC_PES      20

#define TS_MUXER_TX_PACKET_SIZE  188


#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


typedef struct {
    uint8_t                len;
    uint16_t             pid;
    uint8_t                payload_unit_start_indicator;
    uint8_t                continuity_count;

    uint64_t             pcr;  // 90kHz time
    uint8_t                write_pcr;
    uint8_t                random_access_indicator;
    uint8_t                es_priority_indicator;

    uint8_t                header_stuffing_size;
    uint8_t                end_stuffing_size;

    uint8_t       buf[TS_MUXER_TX_PACKET_SIZE];

} ts_muxer_ts_packet_t;

// TS packet IEC 13818-1:2000 2.4.4.3
typedef struct {
    uint8_t                start;
    uint16_t              pid;
    size_t                size;
    size_t                remain;
    uint8_t                program_cnt;
    uint16_t             pmt_pid;
} ts_muxer_ts_pat_t;

typedef struct {
    uint8_t                start;
    uint16_t              pid;
    size_t                size;
    size_t                remain;

}ts_muxer_ts_pmt_t;

typedef struct {
    uint16_t  pid;
    uint8_t   header_ready;      // determine TS packet payload_unit_start_indicator
    uint8_t   is_IDR;
    uint8_t   start;

    uint16_t  dts;   // decoding time stamp in 90kHz unit
    uint16_t  pts;   // presentation time stamp in 90kHz unit

    uint8_t   header_data[256];     // for header, PPS SPS etc
    size_t    header_len;
    uint8_t*  payload;
    size_t    payload_len;

    size_t    filled;       // number of bytes already filled in TS packets

} ts_muxer_h264_pes_t;

typedef struct {
    uint16_t              pid;
    uint8_t                start;

    uint64_t             pts;   // presentation time stamp in 90kHz unit

    uint8_t                header_data[256];     // PES header + ADTS header
    size_t                header_len;
    uint8_t*  payload;
    size_t    payload_len;
    // PES length = PES header (including ADTS header) + ADTS frame + .....
    //

    uint8_t               *raw_pos;
    size_t                 raw_len;

    size_t                filled;       // number of bytes already filled in TS packets

} ts_muxer_aac_pes_t;

// 1 byte stream type used in PMT table
#define TS_MUXER_STREAM_TYPE_H264 0x1B
#define TS_MUXER_STREAM_TYPE_AAC  0x0F
#define TS_MUXER_STREAM_TYPE_MP3  0x03

typedef struct {
    int          stream_index;  // stream index of av_context_t.streams
    uint16_t             pid;   // PID of corresponding TS packet for this ES
    // 0 indicates this ES not exists
    uint8_t               stream_type;
    uint8_t               nalu_size_length;
    uint8_t               done;  // indicate whole stream is encoded in TS stream
    uint8_t               continuity_count; // for TS packet header
    ts_muxer_h264_pes_t     pes;    // current PES, used to compose TS packet

} ts_muxer_h264_stream_t;


typedef struct {
    int          stream_index;  // stream index of av_context_t.streams
    uint16_t             pid;   // PID of corresponding TS packet for this ES
    // 0 indicates this ES not exists
    uint8_t               stream_type;

    uint32_t            frame_count;              // count of AAC audio ADTS frame in one PES
    // each frame consists of one raw data block, which is 1024 samples (audio sample)
    uint8_t               done;

    uint8_t               object_type_indication;   // ObjectTypeIndication in DecoderConfigDescriptor
    uint8_t               aac_audio_object_type;    // audioObjectType from AudioSpecificConfig
    // used in ADTS fixed header
    uint8_t               aac_sampling_frequency_index;  // samplingFrequencyIndex from AudioSpecificConfig
    // used in ADTS fixed header
    uint8_t               aac_channel_config;       // channelConfiguration from AudioSpecificConfig

    uint8_t               continuity_count; // for TS packet header

    ts_muxer_aac_pes_t pes;    // current PES, used to compose TS packet

} ts_muxer_aac_stream_t;

typedef struct {
    uint16_t             pmt_pid;       // can be choose from 0x0020-0x1FFA
    ts_muxer_h264_stream_t  video_stream;
    ts_muxer_aac_stream_t   audio_stream;
} ngx_flv_ts_program_t;


struct _ts_muxer {
    av_context_t* av_context;
    void* avio_context;
    avio_write_func avio_write;
    ngx_flv_ts_program_t program;
    ts_muxer_ts_packet_t ts_packet;
};





typedef struct {
    size_t                size;
    ts_muxer_ts_pat_t      pat;
    ts_muxer_ts_pmt_t      pmt;
    ts_muxer_ts_packet_t   packet;
    ngx_flv_ts_program_t  program;
} ngx_flv_ts_t;


#define ngx_flv_get_16value(p)                                                \
    ( ((uint32_t) ((uint8_t *) (p))[0] << 8)                                   \
    + (           ((uint8_t *) (p))[1]) )

#define ts_muxer_get_24value(p)                                                \
    ( ((uint32_t) ((uint8_t *) (p))[0] << 16)                                  \
    + (           ((uint8_t *) (p))[1] << 8)                                   \
    + (           ((uint8_t *) (p))[2]) )

#define ts_muxer_get_32value(p)                                                \
    ( ((uint32_t) ((uint8_t *) (p))[0] << 24)                                  \
    + (           ((uint8_t *) (p))[1] << 16)                                  \
    + (           ((uint8_t *) (p))[2] << 8)                                   \
    + (           ((uint8_t *) (p))[3]) )

#define ts_muxer_set_32value(p, n)                                             \
    ((uint8_t *) (p))[0] = (uint8_t) ((n) >> 24);                               \
    ((uint8_t *) (p))[1] = (uint8_t) ((n) >> 16);                               \
    ((uint8_t *) (p))[2] = (uint8_t) ((n) >> 8);                                \
    ((uint8_t *) (p))[3] = (uint8_t)  (n)

#define ngx_flv_get_64value(p)                                                \
    ( ((uint64_t) ((uint8_t *) (p))[0] << 56)                                  \
    + ((uint64_t) ((uint8_t *) (p))[1] << 48)                                  \
    + ((uint64_t) ((uint8_t *) (p))[2] << 40)                                  \
    + ((uint64_t) ((uint8_t *) (p))[3] << 32)                                  \
    + ((uint64_t) ((uint8_t *) (p))[4] << 24)                                  \
    + (           ((uint8_t *) (p))[5] << 16)                                  \
    + (           ((uint8_t *) (p))[6] << 8)                                   \
    + (           ((uint8_t *) (p))[7]) )

#define ngx_flv_set_64value(p, n)                                             \
    ((uint8_t *) (p))[0] = (uint8_t) ((uint64_t) (n) >> 56);                    \
    ((uint8_t *) (p))[1] = (uint8_t) ((uint64_t) (n) >> 48);                    \
    ((uint8_t *) (p))[2] = (uint8_t) ((uint64_t) (n) >> 40);                    \
    ((uint8_t *) (p))[3] = (uint8_t) ((uint64_t) (n) >> 32);                    \
    ((uint8_t *) (p))[4] = (uint8_t) (           (n) >> 24);                    \
    ((uint8_t *) (p))[5] = (uint8_t) (           (n) >> 16);                    \
    ((uint8_t *) (p))[6] = (uint8_t) (           (n) >> 8);                     \
    ((uint8_t *) (p))[7] = (uint8_t)             (n)


#define ngx_http_flv_ts_h264_pes_remain(pes, remain) \
    if (pes->filled > pes->header_len + pes->file_len) { \
        return -1; \
    } \
    remain = pes->header_len + pes->file_len - pes->filled


uint32_t flv_crc32_table[256];

void flv_init_crc32_table()
{
    uint32_t i, j, k;
    for( i = 0; i < 256; i++ )
    {
        k = 0;
        for( j = (i << 24) | 0x800000; j != 0x80000000; j <<= 1 )
            k = (k << 1) ^ (((k ^ j) & 0x80000000) ? 0x04c11db7 : 0);

        flv_crc32_table[i] = k;
    }
}

uint32_t ts_muxer_calc_crc32(uint8_t *data, uint32_t nLen)
{
    uint32_t     i;
    uint32_t     i_crc = 0xffffffff;

    for( i = 0; i < nLen; i++ ) {
        i_crc = (i_crc << 8) ^
                flv_crc32_table[((i_crc >> 24) ^ data[i]) & 0xff];
    }
    return i_crc;
}


const u_int32_t ts_muxer_aac_sample_frequencies[] = {
        96000,
        88200,
        64000,
        48000,
        44100,
        32000,
        24000,
        22050,
        16000,
        12000,
        11025,
        8000,
        7350,
        0           // mark the end
};

/* TS packet IEC 13818-1:2000 2.4.3
 *
 */
int ts_muxer_prepare_ts_packet_info(ts_muxer_ts_packet_t *packet, uint8_t payload_type, void *payload,
                                    uint8_t continuity_count)
{
    ts_muxer_ts_pat_t *pat = NULL;
    ts_muxer_ts_pmt_t *pmt = NULL;
    ts_muxer_h264_pes_t *pes = NULL;
    ts_muxer_aac_pes_t  *aac_pes = NULL;
    size_t           payload_remain;
    uint8_t           is_adaptation_filed=0;

    if (!packet || !payload) {
        return -1;
    }

    memset(packet, 0, sizeof(ts_muxer_ts_packet_t));

    packet->continuity_count = continuity_count;
    packet->len = 4;  // 1 byte sync, 2 bytes indicator and PID, 1 byte control and continuity count

    if (TS_MUXER_PAYLOAD_PAT == payload_type) {
        pat = (ts_muxer_ts_pat_t*)payload;
        packet->pid = 0;
        packet->payload_unit_start_indicator = pat->start;
        if ((size_t)(188-packet->len) > pat->remain)
            packet->end_stuffing_size = 188-packet->len-pat->remain;
    } else if (TS_MUXER_PAYLOAD_PMT == payload_type) {
        pmt = (ts_muxer_ts_pmt_t*)payload;
        packet->pid = pmt->pid;
        packet->payload_unit_start_indicator = pmt->start;
        if ((size_t)(188-packet->len) > pmt->remain)
            packet->end_stuffing_size = 188-packet->len-pmt->remain;
    } else if (TS_MUXER_PAYLOAD_H264_PES == payload_type) {
        pes = (ts_muxer_h264_pes_t*)payload;
        packet->pid = pes->pid;
        packet->payload_unit_start_indicator = pes->start;
        if (pes->is_IDR) {
            packet->es_priority_indicator = 1;
        }
        if (pes->start && pes->is_IDR) {
            packet->random_access_indicator = 1;
        }
        // for access unit start
        if (pes->start) {
            // prepare PCR
            packet->pcr = pes->dts;
            packet->write_pcr = 1;
            is_adaptation_filed = 1;
            packet->len += 8;
        }
        payload_remain = pes->header_len + pes->payload_len - pes->filled;
        if (payload_remain < 0) {
            return -1;
        }
        if (payload_remain < (size_t)188 - packet->len) {
            if (188 - packet->len - payload_remain >= 3) {
                packet->header_stuffing_size = 188 - packet->len - payload_remain;
                if (!is_adaptation_filed) {
                    packet->header_stuffing_size -= 2; // need to add adaptation field header
                    is_adaptation_filed = 1;
                }
                packet->len = 188 - payload_remain;
            } else {
                // fill only 1 byte stuff here, and move the rest of the remain to next packet
                if (!is_adaptation_filed) {
                    packet->len += 3; // 2 byte adaption field header + 1 byte stuffing;
                    is_adaptation_filed = 1;
                    packet->header_stuffing_size = 1;
                } else {
                    packet->len += 3;
                    packet->header_stuffing_size = 3;
                }
            }
        }
    } else if (TS_MUXER_PAYLOAD_AAC_PES == payload_type) {
        aac_pes = (ts_muxer_aac_pes_t*)payload;
        packet->pid = aac_pes->pid;
        packet->payload_unit_start_indicator = aac_pes->start;
        payload_remain = aac_pes->header_len + aac_pes->payload_len - aac_pes->filled;
        if (payload_remain < 0) {
            return -1;
        }
        if (payload_remain < (size_t)(188-packet->len)) {
            packet->len += 2; // 2 byte adaption field header + 1 byte stuffing;
            is_adaptation_filed = 1;
            if (packet->len + 1 + payload_remain > 188) {
                packet->header_stuffing_size = 1;
                packet->len += 1;
            } else {
                packet->header_stuffing_size = 188 - packet->len - payload_remain;
                packet->len += packet->header_stuffing_size;
            }
        }
    } else if (TS_MUXER_PAYLOAD_EMPTY_PES == payload_type) {
        packet->pid = *(uint16_t*)payload;
        packet->payload_unit_start_indicator = 0;
        packet->len += 2;  // 2 byte adaption field header
        packet->header_stuffing_size = 188 - packet->len;
        packet->len = 188;
    } else {
        return -1;
    }

    return 0;
}



/* TS packet IEC 13818-1:2000 2.4.3
 *
 */
static uint8_t*
ts_muxer_enc_packet_header(ts_muxer_ts_packet_t *packet, uint8_t *buf)
{
    uint32_t    len, stuff_len;

    // buf should always be large enough for the packet content
    if (!packet || !buf) {
        return NULL;
    }

    *buf++ = 0x47;  // 1 byte sync
    // 2 bytes indicators and PID
    *buf++ = packet->pid >> 8 | (packet->payload_unit_start_indicator ? 0x40 : 0);
    *buf++ = packet->pid;
    // continuity count and adaptation field control
    if (packet->write_pcr || packet->header_stuffing_size) {
        *buf++ = packet->continuity_count | 0x30;
        len = 1+(packet->write_pcr?6:0)+packet->header_stuffing_size;    // adaptation_field_length = flag + PCR + stuffing
        *buf++ = len;
        *buf++ = 0 | (packet->random_access_indicator?0x60:0) | (packet->write_pcr?0x10:0); // 1 byte indicators
        if (packet->write_pcr) {
            // write 6bytes PCR
            *buf++ = packet->pcr >> 25;
            *buf++ = packet->pcr >> 17;
            *buf++ = packet->pcr >> 9;
            *buf++ = packet->pcr >> 1;
            *buf++ = (packet->pcr & 1) | 0x7E;
            *buf++ = 0;
        }
        // stuff
        stuff_len = packet->header_stuffing_size;
        while (stuff_len > 0) {
            *buf++ = 0xFF;
            stuff_len--;
        }
    } else {
        *buf++ = packet->continuity_count | 0x10;
    }

    return buf;
}


/* TS packet IEC 13818-1:2000 2.4.4.3
 *
 */
static int
ts_muxer_enc_psi(struct _ts_muxer *ts)
{
    ts_muxer_ts_packet_t   *packet;
    uint8_t                *buf;
    uint32_t              crc32;
    ts_muxer_ts_pat_t     pat;
    ts_muxer_ts_pmt_t     pmt;

    if (NULL == ts) {
        return -1;
    }

    if (NULL == ts->avio_context || NULL == ts->avio_write) {
        return 0;
    }

    packet = &ts->ts_packet;

    // PAT
    memset(packet->buf, 0, TS_MUXER_TX_PACKET_SIZE);
    buf = packet->buf;
    pat.start = 1;
    pat.pid = 0;
    pat.pmt_pid = ts->program.pmt_pid;
    pat.size = pat.remain = 17;   // because we have only one program, so we know how largs PAT should be

    if (0 != ts_muxer_prepare_ts_packet_info(packet, TS_MUXER_PAYLOAD_PAT, &pat, 0)) {
        return -1;
    }

    buf = ts_muxer_enc_packet_header(packet, buf);

    *buf++ = 0x00; // pointer
    *buf++ = 0x00; // table ID: 0x00 for program association section
    // 12 bit length 0x00D = 13
    *buf++ = 0xB0;
    *buf++ = 0x0D;
    // 16 bit user defined transport_stream_id
    *buf++ = 0x00;
    *buf++ = 0x00;
    // 2 bit reserved 1, 5 bit version 0, 1 bit current_next_indicator 1
    *buf++ = 0xC1;
    *buf++ = 0; // 1 byte section number
    *buf++ = 0; // 1 byte current section number
    // 2 byte program number, we have 1 only
    *buf++ = 0;
    *buf++ = 1;
    // 3 bit reserved 1, 13 bit PMT PID
    *buf++ = (pat.pmt_pid) >> 8 | 0xE0;
    *buf++ = pat.pmt_pid;
    // CRC32
    crc32 = ts_muxer_calc_crc32(buf - 12, 12);
    ts_muxer_set_32value(buf, crc32);
    buf += 4;
    // all stuffing afterwards
    while (buf < packet->buf+TS_MUXER_TX_PACKET_SIZE) {
        *buf++ = 0xFF;
    }

    if (ts->avio_write(ts->avio_context, buf, TS_MUXER_TX_PACKET_SIZE) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to write PAT packet to avio context");
    }

    // PMT
    memset(packet->buf, 0, TS_MUXER_TX_PACKET_SIZE);
    buf = packet->buf;
    pmt.start = 1;
    pmt.pid = ts->program.pmt_pid;
    pmt.size = pmt.remain = 27; // we know the exact size because we are sure we will have at most 2 streams

    if (0 != ts_muxer_prepare_ts_packet_info(packet, TS_MUXER_PAYLOAD_PMT, &pmt, 0)) {
        return -1;
    }

    buf = ts_muxer_enc_packet_header(packet, buf);

    // encoding PMT
    *buf++ = 0x00; // pointer
    *buf++ = 0x02; // table ID: 0x00 for TS_program_map_section
    // 12 bit length 0x00D = 13
    *buf++ = 0xB0;
    if (0 == ts->program.video_stream.stream_type && 0 == ts->program.audio_stream.stream_type) {
        return -1;
    } else if (0 == ts->program.video_stream.stream_type || 0 == ts->program.audio_stream.stream_type) {
        *buf++ = 0x12;
    } else {
        *buf++ = 0x17;
    }
    // 2 bytes program number
    *buf++ = 0x00;
    *buf++ = 0x01;
    // 2 bit reserved 1, 5 bit version 0, 1 bit current_next_indicator 1
    *buf++ = 0xC1;
    *buf++ = 0; // 1 byte section number
    *buf++ = 0; // 1 byte current section number
    // 3 bit reserved and 13 bit of PCR_PID (usually the video stream PID)
    *buf++ = 0xE0 | (ts->program.video_stream.pid >> 8);
    *buf++ = ts->program.video_stream.pid;
    // 4 bit reserved 1, 12 bit program_info_length
    // don't known what this descriptor is, put 0 here
    *buf++ = 0xF0;
    *buf++ = 0;
    // h264 stream, 1 byte stream type
    if (0 == ts->program.video_stream.stream_type) {
        *buf++ = ts->program.video_stream.stream_type;
        *buf++ = 0xE0 | (ts->program.video_stream.pid >> 8);
        *buf++ = ts->program.video_stream.pid;
        *buf++ = 0xF0;   // don't known what descriptor is, just ignore it.
        *buf++ = 0x00;
    }
    // audio stream
    if (0 == ts->program.audio_stream.stream_type) {
        *buf++ = ts->program.audio_stream.stream_type;
        *buf++ = 0xE0 | (ts->program.audio_stream.pid >> 8);
        *buf++ = ts->program.audio_stream.pid;
        *buf++ = 0xF0;   // don't known what descriptor is, just ignore it.
        *buf++ = 0x00;
    }
    // CRC32
    crc32 = ts_muxer_calc_crc32(buf - 22, 22);
    ts_muxer_set_32value(buf, crc32);
    buf += 4;
    // all stuffing afterwards
    while (buf <= packet->buf+TS_MUXER_TX_PACKET_SIZE) {
        *buf++ = 0xFF;
    }

    if (ts->avio_write(ts->avio_context, packet->buf, TS_MUXER_TX_PACKET_SIZE) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to write PMT packet to avio context");
    }

    return 0;
}

int ts_muxer_enc_av_packet(ts_muxer_t* ts_muxer, av_packet_t* av_packet) {

    av_context_t* av_context;
    av_stream_t* av_stream;

    if (ts_muxer == NULL || av_packet == NULL) {
        return -1;
    }

    av_context = ts_muxer->av_context;

    if (NULL == av_context) {
        return -1;
    }

    if (av_packet->stream_index >= av_context->stream_count) {
        av_log(NULL, AV_LOG_ERROR, "Invalid stream count %d in av packet", av_packet->stream_index);
        return -1;
    }

    av_stream = av_context->streams[av_packet->stream_index];
    if (av_stream == NULL) {
        av_log(NULL, AV_LOG_ERROR, "stream %d is NULL in av context", av_packet->stream_index);
        return -1;
    }

    if (av_stream->type == AV_STREAM_TYPE_VIDEO
        && av_stream->codec == AV_STREAM_CODEC_H264
        && ts_muxer->program.video_stream.stream_type == TS_MUXER_STREAM_TYPE_H264
        && ts_muxer->program.video_stream.stream_index == av_packet->stream_index) {

        return ts_muxer_enc_h264_packet(ts_muxer, &ts_muxer->program.video_stream, av_packet);
    } else if (av_stream->type == AV_STREAM_TYPE_AUDIO
               && av_stream->codec == AV_STREAM_CODEC_AAC
               && ts_muxer->program.audio_stream.stream_type == TS_MUXER_STREAM_TYPE_AAC
               && ts_muxer->program.audio_stream.stream_index == av_packet->stream_index) {
        return ts_muxer_enc_aac_packet(ts_muxer, &ts_muxer->program.video_stream, av_packet);
    } else {
        av_log(NULL, AV_LOG_ERROR, "Unprepared stream type %d stream codec %d", av_stream->type, av_stream->codec);
        return -1;
    }

}

int ts_muxer_enc_h264_packet(ts_muxer_t* ts, ts_muxer_h264_stream_t* h264_stream, av_packet_t* av_packet) {

    ts_muxer_h264_pes_t* pes;
    ts_muxer_ts_packet_t* ts_packet;
    uint8_t* pos;
    size_t len;

    if (NULL == ts || NULL == h264_stream || NULL == av_packet) {
        return -1;
    }

    if (NULL == ts->avio_context || NULL == ts->avio_write) {
        return 0;
    }


    pes = &h264_stream->pes;
    ts_packet = &ts->ts_packet;

    if (0 != ts_muxer_prepare_h264_pes(h264_stream, pes, av_packet)) {
        return -1;
    }

    while (pes->header_len + pes->payload_len > pes->filled && pes->header_len != 0) {

        // prepare av_packet
        if (0 != ts_muxer_prepare_ts_packet_info(ts_packet, TS_MUXER_PAYLOAD_H264_PES, pes, h264_stream->continuity_count)) {
            av_log(NULL, AV_LOG_ERROR, "Failed to prepare TS av_packet for H264 frame");
            return -1;
        }

        // encode TS av_packet
        pos = ts_muxer_enc_packet_header(ts_packet, ts_packet->buf);
        if (NULL == pos) {
            av_log(NULL, AV_LOG_ERROR, "Failed to encode TS av_packet for H.264 PES");
            return -1;
        }

        // fill PES into TS av_packet
        pes->start = 0;
        if (pes->filled < pes->header_len) {
            // fill PES header part
            len = MIN(pes->header_len - pes->filled, TS_MUXER_TX_PACKET_SIZE+ts_packet->buf-pos);
            memcpy(pos, pes->header_data+pes->filled, len);
            pos += len;
            pes->filled += len;
        }

        if (pes->filled >= pes->header_len) {
            // fill h264 NALU
            len = MIN(pes->payload_len+pes->header_len-pes->filled, TS_MUXER_TX_PACKET_SIZE+ts_packet->buf-pos);
            if (len) {
                memcpy(pos, pes->payload+(pes->filled-pes->header_len), len);
                pos += len;
                pes->filled += len;
            }
        }

        // there should not be end stuffing for PES TS av_packet
        h264_stream->continuity_count++;
        if (h264_stream->continuity_count > 0x0F) {
            h264_stream->continuity_count = 0;
        }

        if (ts->avio_write(ts->avio_context, ts_packet->buf, TS_MUXER_TX_PACKET_SIZE) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to write H264 packet to avio context");
        }
    }

    return 0;
}


int ts_muxer_enc_aac_packet(ts_muxer_t* ts, ts_muxer_aac_stream_t* aac_stream, av_packet_t* av_packet) {

    ts_muxer_aac_pes_t* pes;
    ts_muxer_ts_packet_t* ts_packet;
    uint8_t* pos;
    size_t len;

    if (NULL == ts || NULL == aac_stream || NULL == av_packet) {
        return -1;
    }

    if (NULL == ts->avio_context || NULL == ts->avio_write) {
        return 0;
    }

    pes = &aac_stream->pes;
    ts_packet = &ts->ts_packet;

    if (0 != ts_muxer_prepare_aac_pes(aac_stream, pes, av_packet)) {
        return -1;
    }

    while (pes->header_len + pes->payload_len > pes->filled && pes->header_len != 0) {

        // prepare packet
        if (0 != ts_muxer_prepare_ts_packet_info(ts_packet, TS_MUXER_PAYLOAD_AAC_PES, pes, aac_stream->continuity_count)) {
            av_log(NULL, AV_LOG_ERROR, "Failed to prepare TS packet for AAC frame");
            return -1;
        }

        // encode TS packet
        pos = ts_muxer_enc_packet_header(ts_packet, ts_packet->buf);
        if (NULL == pos) {
            av_log(NULL, AV_LOG_ERROR, "Failed to encode TS packet for AAC PES");
            return -1;
        }

        // fill PES into TS packet
        pes->start = 0;
        if (pes->filled < pes->header_len) {
            // fill PES header part
            len = MIN(pes->header_len - pes->filled, TS_MUXER_TX_PACKET_SIZE+ts_packet->buf-pos);
            memcpy(pos, pes->header_data+pes->filled, len);
            pos += len;
            pes->filled += len;
        }

        if (pes->filled >= pes->header_len) {
            len = MIN(pes->payload_len+pes->header_len-pes->filled, TS_MUXER_TX_PACKET_SIZE+ts_packet->buf-pos);
            if (len) {
                memcpy(pos, pes->payload+(pes->filled-pes->header_len), len);
                pos += len;
                pes->filled += len;
            }
        }

        // there should not be end stuffing for PES TS packet
        aac_stream->continuity_count++;
        if (aac_stream->continuity_count > 0x0F) {
            aac_stream->continuity_count = 0;
        }

        if (ts->avio_write(ts->avio_context, ts_packet->buf, TS_MUXER_TX_PACKET_SIZE) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to write AAC packet to avio context");
        }
    }

    return 0;

}


/* PES IEC 13818-1 2.4.3.7
 *
 */
static int
ts_muxer_prepare_h264_pes(ts_muxer_h264_stream_t *stream, ts_muxer_h264_pes_t *pes, av_packet_t *av_packet)
{
    uint8_t    *buf;

    if (NULL == pes || NULL == stream || NULL == av_packet) {
        return -1;
    }

    memset(pes, 0, sizeof(ts_muxer_h264_pes_t));
    pes->start = 1;
    pes->is_IDR = av_packet->flags & AV_PACKET_FLAGS_IS_IDR;
    pes->pid = stream->pid;
    pes->dts = av_packet->dts;
    pes->pts = av_packet->pts;
    pes->payload = av_packet->data;
    pes->payload_len = av_packet->size;
    pes->filled = 0;

    // PES header
    buf = pes->header_data;
    // 3 bytes packet_start_code_prefix
    *buf++ = 0x00;
    *buf++ = 0x00;
    *buf++ = 0x01;
    *buf++ = 0xE0; // 1 byte stream_id
    // 2 byte PES length, let's make it 0 for simplicity
    *buf++ = 0x00;
    *buf++ = 0x00;
    *buf++ = 0x80;
    *buf++ = 0xC0;
    *buf++ = 0x0A;  // 1 byte PES_header_data_length
    // 5 bytes PTS
    *buf++ = 0x31 | ((pes->pts >> 29) & 0x0E);
    *buf++ = pes->pts >> 22;
    *buf++ = (pes->pts >> 14) | 0x01;
    *buf++ = pes->pts >> 7;
    *buf++ = (pes->pts << 1) | 0x01;
    // 5 bytes DTS, currently we have same PTS and DTS, which means we don't support B-frame
    *buf++ = 0x11 | ((pes->dts >> 29) & 0x0E);
    *buf++ = pes->dts >> 22;
    *buf++ = (pes->dts >> 14) | 0x01;
    *buf++ = pes->dts >> 7;
    *buf++ = (pes->dts << 1) | 0x01;

    // PES body
    // AU delimiter
    ts_muxer_set_32value(buf, 1);
    buf += 4;
    *buf++ = 0x09;
    if (pes->is_IDR) {
        *buf++ = 0x10;
    } else {
        *buf++ = 0x30;
    }

    // let's add synchronization byte sequence before VCL NALU
    //ts_muxer_set_32value(buf, 1);
    //buf += 4;

    pes->header_len = buf - pes->header_data;

    return 0;
}


/* IEC 14496-3:3005 1.a.3.2
 * One PES for each chunk, which contains multiple samples
 * and one ADTS for each frame
 *
 * PES IEC 13818-1 2.4.3.7
 */
static int
ts_muxer_prepare_aac_pes(ts_muxer_aac_stream_t *stream, ts_muxer_aac_pes_t *pes, av_packet_t *av_packet)
{
    uint8_t                  *buf;
    size_t                   adts_frame_len;

    if (NULL == stream || NULL == pes || NULL == pes) {
        return -1;
    }

    if (pes->header_len + pes->payload_len != 0
        && pes->header_len + pes->payload_len > pes->filled) {
        // not all filled
        return 0;
    }

    // fill PES header
    memset(pes, 0, sizeof(ts_muxer_aac_pes_t));
    pes->start = 1;
    pes->pid = stream->pid;
    pes->pts = av_packet->pts;
    pes->payload = av_packet->data;
    pes->payload_len = av_packet->size;
    pes->filled = 0;

    // PES header
    buf = pes->header_data;
    // 3 bytes packet_start_code_prefix
    *buf++ = 0x00;
    *buf++ = 0x00;
    *buf++ = 0x01;
    *buf++ = 0xC0; // 1 byte stream_id for MPEG audio
    // 2 byte PES length, let's leave it 0 and fiil it at the end of this function
    *buf++ = 0x00;
    *buf++ = 0x00;
    *buf++ = 0x84;
    *buf++ = 0x80;
    *buf++ = 0x05;  // 1 byte PES_header_data_length
    // 5 bytes PTS
    *buf++ = 0x21 | ((pes->pts >> 29) & 0x0E);
    *buf++ = pes->pts >> 22;
    *buf++ = (pes->pts >> 14) | 0x01;
    *buf++ = pes->pts >> 7;
    *buf++ = (pes->pts << 1) | 0x01;

    pes->header_len = 14;

    // ADTS header
    //buf = pes->adts_header_fix;
    *buf++ = 0xFF;
    if (stream->object_type_indication == 0x40) {
        // MPEG-4 AAC
        *buf++ = 0xF1;
        if (stream->aac_audio_object_type <= 4 && stream->aac_audio_object_type > 0) {
            *buf = (stream->aac_audio_object_type-1) << 6;
        } else if (stream->aac_audio_object_type > 4) {
            *buf = 3 << 6;
        } else {
            // 0 ??
            return -1;
        }
    } else {
        if (stream->object_type_indication == 0x69
            || stream->object_type_indication == 0x6B) {
            // mp3 (MPEG-1 layer III or MPEG-2 layer III)
            av_log(NULL, AV_LOG_ERROR, "flv we don't support MP3 audio currently");
            return -1;
        } else {
            // MPEG-2 AAC
            *buf++ = 0xF9;
            if (stream->object_type_indication == 0x66) {
                *buf = 0 << 6;
            } else if (stream->object_type_indication == 0x67) {
                *buf = 1 << 6;
            } else if (stream->object_type_indication == 0x68) {
                *buf = 2 << 6;
            } else {
                // unknown
                return -1;
            }
        }
    }
    *buf |= (stream->aac_sampling_frequency_index & 0x0F) << 2;
    if (stream->aac_channel_config > 7) {
        stream->aac_channel_config = 0;
    }
    *buf |= (stream->aac_channel_config & 0x07) >> 2;
    buf ++;
    *buf = (stream->aac_channel_config & 0x07) << 6;

    adts_frame_len = 7+pes->payload_len;
    if (adts_frame_len >= 1 << 13) {
        av_log(NULL, AV_LOG_ERROR, "flv ADTS frame too large");
        return -1;
    }

    *buf |= (adts_frame_len >> 11) & 0x03;
    buf ++;
    *buf++ = adts_frame_len >> 3;
    *buf = adts_frame_len << 5;
    *buf |= 0x1F;
    buf ++;
    *buf = 0xFC;

    pes->header_len += 7;

    // finally PES length
    if (pes->header_len+pes->payload_len-6+1 >= 1<<16) {
        av_log(NULL, AV_LOG_ERROR, "FLV AAC audio PES too large");
        return -1;
    }
    pes->header_data[4] = (pes->header_len + pes->payload_len - 6)>>8;
    pes->header_data[5] = pes->header_len + pes->payload_len - 6;

    return 0;
}


ts_muxer_t* new_ts_muxer(av_context_t* av_context) {

    struct _ts_muxer* ts_muxer;

    if (av_context == NULL) {
        av_log(NULL, AV_LOG_ERROR, "invalid av_context");
        return NULL;
    }

    ts_muxer = malloc(sizeof(struct _ts_muxer));
    if (ts_muxer == NULL) {
        av_log(NULL, AV_LOG_ERROR, "Failed to malloc ts_muxer");
        return NULL;
    }
    memset(ts_muxer, 0, sizeof(struct _ts_muxer));

    ts_muxer->av_context = av_context;

    return ts_muxer;
}


void free_ts_muxer(ts_muxer_t* ts_muxer) {

    if (NULL == ts_muxer) {
        return;
    }

    // Try to flush all AV data to file

    // close io context

    // free
    free(ts_muxer);
}


int ts_muxer_set_avio_context(ts_muxer_t* ts_muxer, void* avio_context, avio_write_func avio_write) {

    if (NULL == ts_muxer) {
        av_log(NULL, AV_LOG_ERROR, "Failed to set avio context, ts_muxer is NOLL");
    }

    ts_muxer->avio_context = avio_context;
    ts_muxer->avio_write = avio_write;
}


int ts_muxer_write_header(ts_muxer_t* ts_muxer) {

    av_context_t* av_context;
    av_stream_t* av_stream;
    int i;

    if (NULL == ts_muxer) {
        return -1;
    }

    if (NULL == ts_muxer->av_context) {
        return -1;
    }
    av_context = ts_muxer->av_context;

    // find first video and audio stream
    for (i=0; i<av_context->stream_count; i++) {
        av_stream = av_context->streams[i];
        if (AV_STREAM_TYPE_VIDEO == av_stream->type && AV_STREAM_CODEC_H264 == av_stream->codec) {
            // found H264 stream
            if (0 == ts_muxer->program.video_stream.pid) {
                ts_muxer->program.pmt_pid = 0x0FF0;
                ts_muxer->program.video_stream.stream_index = i;
                ts_muxer->program.video_stream.pid = 0x1000;
                ts_muxer->program.video_stream.stream_type = TS_MUXER_STREAM_TYPE_H264;  // for H.264
            }
        } else if (AV_STREAM_TYPE_AUDIO == av_stream->type && AV_STREAM_CODEC_AAC == av_stream->codec) {
            if (0 == ts_muxer->program.audio_stream.pid) {
                ts_muxer->program.pmt_pid = 0x0FF0;
                ts_muxer->program.video_stream.stream_index = i;
                ts_muxer->program.audio_stream.pid = 0x1001;
                ts_muxer->program.audio_stream.stream_type = TS_MUXER_STREAM_TYPE_AAC;
                // https://wiki.multimedia.cx/index.php?title=MPEG-4_Audio#Audio_Object_Types
                ts_muxer->program.audio_stream.aac_audio_object_type = 1; // AAC main
                ts_muxer->program.audio_stream.aac_channel_config = av_stream->audio_channel_count;
                for (i = 0; ts_muxer_aac_sample_frequencies[i] != 0 && ts_muxer_aac_sample_frequencies[i] != av_stream->audio_sample_rate; i++) {
                }
                if (ts_muxer_aac_sample_frequencies[i] == 0) {
                    av_log(NULL, AV_LOG_ERROR, "Invalid audio sample frequency %d", av_stream->audio_sample_rate);
                    return -1;
                }
                ts_muxer->program.audio_stream.aac_sampling_frequency_index = i;
            }
        }
    }

    if (0 == ts_muxer->program.pmt_pid) {
        av_log(NULL, AV_LOG_ERROR, "failed find either video or audio stream");
        return -1;
    }

    // write PAT and PMT
    ts_muxer_enc_psi(ts_muxer);

    return 0;
}


int ts_muxer_write_packet(ts_muxer_t* ts_muxer, av_packet_t* av_packet) {

    av_context_t* av_context;
    ts_muxer_h264_stream_t* h264_stream;

    if (NULL == ts_muxer) {
        return -1;
    }

    if (NULL == av_packet) {
        // flush data to file

        return 0;
    }

    av_context = ts_muxer->av_context;
    ts_muxer->program.video_stream;

    // write packet to file

    if (0 != ts_muxer_enc_av_packet(&ts_muxer, av_packet)) {
        av_log(NULL, AV_LOG_ERROR, "Failed to prepare h264 PES");
        return -1;
    }

    return 0;
}


int ts_muxer_write_trailer(ts_muxer_t* ts_muxer) {

    if (NULL == ts_muxer) {
        return -1;
    }

    return 0;
}