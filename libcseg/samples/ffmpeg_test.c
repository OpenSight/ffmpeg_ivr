
/**
 * @file
 * Demuxing from ffmpeg and muxing remuxing with libcseg.
 *
 * Show how to use the libcseg API to mux into hls.
 * @example demuxing_decoding.c
 */
#include <signal.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "libcseg.h"
#include "auxiliary/mpeg4audio.h"

static AVFormatContext *fmt_ctx = NULL;
static char * program_name = NULL;

static const char *src_filename = NULL;
static const char *dst_url = NULL;

static AVBitStreamFilterContext *filter_ctx = NULL;
static char * filter_name = NULL;

static CachedSegmentContext *cseg_ctx = NULL;
static av_stream_t *stream_infos = NULL;
static uint8_t stream_count = 0;                    
static uint64_t start_sequence = 0; 
static double segment_time = 10.0;
static int max_nb_segments = 3;
static uint32_t max_seg_size = 2097152;
static double pre_recoding_time = 5.0;  
static double start_ts = 0.0;
static int32_t io_timeout = 10000;    //10sec

static volatile int should_shutdown = 0;

static  int init_stream_info(void)
{
    int ret = 0;
    int i;
    
    if(fmt_ctx->nb_streams == 0){
        fprintf(stderr, "no streams in the src file\n");
        return -1;
    }
    stream_count = fmt_ctx->nb_streams;
    stream_infos = malloc(sizeof(av_stream_t) * stream_count);
    memset(stream_infos, 0, sizeof(stream_infos) * stream_count);
    
    for(i=0; i<fmt_ctx->nb_streams; i++) {
        AVStream *st= fmt_ctx->streams[i];
        AVCodecContext *codec= st->codec;
        
        switch(codec->codec_type){
        case AVMEDIA_TYPE_AUDIO:    
            stream_infos[i].type = AV_STREAM_TYPE_AUDIO;
            if(codec->codec_id == AV_CODEC_ID_AAC){
                MPEG4AudioConfig m4ac;                
                memset(&m4ac, 0, sizeof(MPEG4AudioConfig));
                
                if(codec->extradata_size != 0){
                    stream_infos[i].codec = AV_STREAM_CODEC_AAC;
                    ret = avpriv_mpeg4audio_get_config(&m4ac, codec->extradata, 
                                                       codec->extradata_size * 8, 1);
                    if(ret){
                        fprintf(stderr, "get aac config failed\n");
                        ret = -1;
                        goto failed;                    
                    }
                    if (m4ac.object_type > 4U) {
                        fprintf(stderr, "MPEG-4 AOT %d is not allowed in ADTS\n", m4ac.object_type);
                        ret = -1;
                        goto failed;  
                    }                   
                                        
                    stream_infos[i].audio_sample_rate = m4ac.sample_rate;
                    stream_infos[i].audio_channel_count = m4ac.channels;
                    stream_infos[i].audio_object_type = m4ac.object_type;
                    
                    /*for debug*/
                    fprintf(stderr, 
                            "AAC audio_sample_rate:%d, audio_channel_count:%d, audio_object_type: %d from extradata\n",
                            (int)stream_infos[i].audio_sample_rate, 
                            (int)stream_infos[i].audio_channel_count, 
                            (int)stream_infos[i].audio_object_type);
                    
                }else{
                    /*for debug*/
                    fprintf(stderr, "AAC has no extradata\n");
                    stream_infos[i].codec = AV_STREAM_CODEC_AAC_WITH_ADTS;
                }

                
            }else{
                fprintf(stderr, "only support AAC codec for audio\n");
                ret = -1;
                goto failed;
            }
            
            break;
        case AVMEDIA_TYPE_VIDEO:
            stream_infos[i].type = AV_STREAM_TYPE_VIDEO;
            if(codec->codec_id == AV_CODEC_ID_H264){                
                stream_infos[i].codec = AV_STREAM_CODEC_H264;
            }else{
                fprintf(stderr, "only support h264 for video\n");
                ret = -1;
                goto failed;
            }             
            break;
        default:
            fprintf(stderr,
                    "codec type (%d) unsupported, only support VIDEO or AUDIO \n", 
                    codec->codec_type);
            ret = -1;
            goto failed;
            break;
        }   
    }//for(int i=0; i<fmt_ctx_->nb_streams; i++) {  
  
    return 0;
        
failed:
    if(stream_infos != NULL){
        free(stream_infos);
        stream_infos = NULL;
    }
    return ret;
}


static  int init_stream_info_fake(void)
{
    int ret = 0;
    int i;
    
    if(fmt_ctx->nb_streams == 0){
        fprintf(stderr, "no streams in the src file\n");
        return -1;
    }
    stream_count = 1;
    stream_infos = malloc(sizeof(av_stream_t) * stream_count);
    memset(stream_infos, 0, sizeof(stream_infos) * stream_count);

    stream_infos[0].type = AV_STREAM_TYPE_AUDIO;             
    stream_infos[0].codec = AV_STREAM_CODEC_AAC;
    stream_infos[0].audio_sample_rate = 16000;
    stream_infos[0].audio_channel_count = 2;
    stream_infos[0].audio_object_type = 2;

    return 0;
        

}


static int write_packet(AVPacket *pkt)
{
    av_packet_t av_pkt;
    AVStream *st= fmt_ctx->streams[pkt->stream_index];
    AVCodecContext *codec= st->codec;
    int ret = 0;
    uint8_t *poutbuf = NULL;
    int poutbuf_size = 0;
/*    
    fprintf(stderr, "pkt stream index:%d, pts:%lld, dts:%lld, time_base:%d/%d\n",
            pkt->stream_index, pkt->pts, pkt->dts, st->time_base.num, st->time_base.den);
             * 
             * 
*/
/*
    if(codec->codec_type != AVMEDIA_TYPE_AUDIO){
        return 0;
    }
*/
    memset(&av_pkt, 0, sizeof(av_packet_t));
    av_pkt.av_stream_index = pkt->stream_index;
    if(pkt->flags & AV_PKT_FLAG_KEY){
        av_pkt.flags |= AV_PACKET_FLAGS_KEY;
    }
    av_pkt.data = pkt->data;
    av_pkt.size = pkt->size;
    if(pkt->pts != AV_NOPTS_VALUE){
        av_pkt.pts = av_rescale_q_rnd(pkt->pts, st->time_base, 
                                      av_make_q(1, TS_TIME_BASE), 
                                      AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    }else{
        av_pkt.pts = NOPTS_VALUE;
    }
    
    if(pkt->dts != AV_NOPTS_VALUE && pkt->dts != pkt->pts){
        av_pkt.dts = av_rescale_q_rnd(pkt->dts, st->time_base, 
                                      av_make_q(1, TS_TIME_BASE), 
                                      AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);        
    }else{
        av_pkt.dts = NOPTS_VALUE;
    }
/*    
    fprintf(stderr, "av_pkt stream index:%d, pts:%lld, dts:%lld\n",
            av_pkt.av_stream_index, av_pkt.pts, av_pkt.dts);    
*/   
    if(codec->codec_type == AVMEDIA_TYPE_VIDEO && filter_ctx != NULL){
        int i;
        
        av_packet_split_side_data(pkt);

        ret = av_bitstream_filter_filter(filter_ctx,
                                         codec, NULL,
                                         &poutbuf, &poutbuf_size,
                                         pkt->data, pkt->size, pkt->flags & AV_PKT_FLAG_KEY);
        if(ret < 0){
            fprintf(stderr, "filter failed: %d\n", ret);
            return ret;
        }else if(ret > 0){
            av_pkt.data = poutbuf;
            av_pkt.size = poutbuf_size;
        }
       
    }

    
    ret = cseg_write_packet(cseg_ctx, &av_pkt);
  
    if(av_pkt.data != pkt->data){
        av_free(av_pkt.data);
    }

    return ret;
}


void signalHandlerShutdown(int sig) {
  fprintf(stderr, "Got shutdown signal %d\n", sig);
  should_shutdown = 1; 
}

void print_help(void)
{
        fprintf(stderr, "usage: %s [--ffmpeg_options string][--ffmpeg_vf filter_name] "
                "input_file output_url\n"
                "API example program to show how to use the libcseg API, which make\n"
                "use of ffmpeg to input the frame\n", program_name);
        exit(1);    
}

int main (int argc, char **argv)
{
    int ret = 0;
    char * option_str = NULL;
    AVDictionary *format_opts = NULL;
    AVPacket pkt;

    program_name = argv[0];
    
    
    if (argc < 3) {
        print_help();
    }    
    
    while(strncmp(argv[1], "--", 2) == 0){
        if(strcmp(argv[1], "--ffmpeg_options") == 0){
            if(argc < 3 || argv[2] == NULL){
                print_help();
            }
        
            ret = av_dict_parse_string(&format_opts, 
                                       argv[2],
                                       "=", ",", 0);
            if(ret){
                //log
                fprintf(stderr, "Failed to parse the ffmpeg_options_str:%s to av_dict(ret:%d)\n", 
                         argv[1], ret);    
                exit(1);   
            }
            argv++;  
            argc--;
            argv++;
            argc--;
        }else if(strcmp(argv[1], "--ffmpeg_vf") == 0){
            if(argc < 3 || argv[2] == NULL){
                print_help();
            } 
            filter_name = argv[2];
            argv++;
            argc--;
            argv++;
            argc--;
        }else{
            print_help();
        }        
    }
    
    if(argc != 3){
        print_help();
    }
    
    
    src_filename = argv[1];
    dst_url = argv[2];


    // Allow ourselves to be shut down gracefully by a SIGHUP or a SIGUSR1:
    signal(SIGHUP, signalHandlerShutdown);
    signal(SIGUSR1, signalHandlerShutdown);
    signal(SIGINT, signalHandlerShutdown);
    
    
    /* register all formats and codecs */
    av_register_all();
    avformat_network_init();
    
    av_log_set_level(AV_LOG_DEBUG);

    libcseg_init();

    if(filter_name != NULL){
        filter_ctx = av_bitstream_filter_init(filter_name);
        if(filter_ctx == NULL){
            fprintf(stderr, "Could not found video filter:%s\n", filter_name);
            exit(1);            
        }
    }

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, &format_opts) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }

    if(format_opts != NULL){
        int no_parsed_option_num = av_dict_count(format_opts);
        if(no_parsed_option_num){
            fprintf(stderr,
            "%d options cannot be parsed by ffmpeg avformat context\n", 
            no_parsed_option_num);            
        }
        //printf("after open, option dict count:%d\n", av_dict_count(format_opts));
        av_dict_free(&format_opts);
        format_opts = NULL;
    }       
    

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }

    /* dump input information to stderr */
    fprintf(stderr, "source format info:\n");
    av_dump_format(fmt_ctx, 0, src_filename, 0);

    /* init the stream info config */

    if(init_stream_info()){
        fprintf(stderr, "Init stream info failed\n");
        exit(1);        
    }

    if(ret = init_cseg_muxer(dst_url,
                             stream_infos, stream_count,
                             start_sequence,
                             segment_time,
                             max_nb_segments,
                             max_seg_size,
                             pre_recoding_time,
                             start_ts,
                             io_timeout,
                             NULL,
                             &cseg_ctx) != 0){
        
        fprintf(stderr, "Init cseg muxer failed: %d\n", ret);
        exit(1);                                   
    }


    fprintf(stderr, "Start writing packets to %s\n", dst_url);


    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    ret = 0;
    while(!should_shutdown){
        ret = av_read_frame(fmt_ctx, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            av_usleep(10000);
            continue;
        }        
        if(ret < 0){
            fprintf(stderr, "Read frame failed(%d): %s\n", ret, av_err2str(ret));
            break;
        }
     
        if((ret = write_packet(&pkt))< 0){
            fprintf(stderr, "failed to write frame to cseg context (%d): %s\n", ret, strerror(-ret));
            break;      
        }       
       
        av_free_packet(&pkt);
    }
    
    av_free_packet(&pkt);
    
    if(ret == 0){
        printf("Demuxing succeeded.\n");
    }


end:
    if(stream_infos != NULL){
        free(stream_infos);
        stream_infos = NULL;
    }
    
    if(cseg_ctx != NULL){
        release_cseg_muxer(cseg_ctx);
        cseg_ctx = NULL;
    }
    if(fmt_ctx != NULL){
        avformat_close_input(&fmt_ctx);
        fmt_ctx = NULL;
    }

    if(format_opts != NULL){
        av_dict_free(&format_opts);
        format_opts = NULL;
    }   

    if(filter_ctx != NULL){
        av_bitstream_filter_close(filter_ctx);
        filter_ctx = NULL;
    }

    return ret < 0;
}
