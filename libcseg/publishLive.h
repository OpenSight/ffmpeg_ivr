#include "rtmp.h"

#ifndef _PUBLISHLIVE_
#define _PUBLISHLIVE_

#define VDECODER_BUFFER_SIZE 2*1024*1024
#define ADECODER_BUFFER_SIZE 2048
typedef enum {
	AIPU_SPS = 7,
	AIPU_PPS = 8,
	AIPU_SEI = 6,
	AIPU_IDR = 5,
	AIPU_NON_IDR = 1,
	AIPU_UNKNOWN_NAL = -1
}AIPU_AVC_NAL;

typedef enum {
	AIPU_AAC = 0,
	AIPU_MP3 = 1,
	AIPU_G711A = 2,
	AIPU_G711U = 3,
	AIPU_H264 = 4,
	AIPU_UNKNOWN_CODEC
}AIPU_AVCODEC_TYPE;

typedef struct
{
	RTMP *pRtmp;
	RTMPPacket avPacket;
	int bSendAudioCfg;
	unsigned int aTimeS;
	unsigned int vTimeS;
	unsigned int sampleRate;
	int sendSps;
	char url[512+1];
}rtmpPublisher;

int publishLive_open(const char* url, rtmpPublisher *rPublish);
int publishLive_sendAV(rtmpPublisher *rPublish, const unsigned char* videoData, const int frameLen, int frmType, int frmRate);
void publishLive_close(rtmpPublisher *rPublish);


#endif










































