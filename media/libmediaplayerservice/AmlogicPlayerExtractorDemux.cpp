/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "AmlogicPlayerExtractorDemux"

#define  TRACE()	ALOGV("[%s::%d]\n",__FUNCTION__,__LINE__)

#include <utils/Log.h>
#include <media/stagefright/MetaData.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/FileSource.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaErrors.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <utils/String8.h>

#include <cutils/properties.h>

#include"AmlogicPlayerExtractorDemux.h"

///#include "include/MPEG2TSExtractor.h"
namespace android
{
AVInputFormat AmlogicPlayerExtractorDemux::DrmDemux;
//static
int AmlogicPlayerExtractorDemux::BasicInit(void)
{
    static int inited = 0;
    if (inited) {
        return 0;
    }
    inited = 1;
    AVInputFormat *pinputdemux = &DrmDemux;
    pinputdemux->name           = "DRMdemux";
    pinputdemux->long_name      = NULL;
    pinputdemux->priv_data_size = 8;//for AmlogicPlayerExtractorDmux;
    pinputdemux->read_probe     = extractor_probe;
    pinputdemux->read_header    = extractor_read_header;
    pinputdemux->read_packet    = extractor_read_packet;
    pinputdemux->read_close     = extractor_close;
    pinputdemux->read_seek      = extractor_read_seek;


    av_register_input_format(pinputdemux);
	ALOGV("pinputdemux->read_probe=%p\n", pinputdemux->read_probe);
	extern bool SniffMPEG2TS(const sp<DataSource> &source, String8 *mimeType, float *confidence,sp<AMessage> *);
	extern bool SniffMPEG2PS(const sp<DataSource> &source, String8 *mimeType, float *confidence,sp<AMessage> *);
	extern bool SniffMatroska(const sp<DataSource> &source, String8 *mimeType, float *confidence,sp<AMessage> *);
	extern bool SniffDRM(const sp<DataSource> &source, String8 *mimeType, float *confidence,sp<AMessage> *);
#if 0
	AmlogicPlayerExtractorDataSource::RegisterExtractorSniffer(SniffMPEG2TS);
	AmlogicPlayerExtractorDataSource::RegisterExtractorSniffer(SniffMPEG2PS);
	AmlogicPlayerExtractorDataSource::RegisterExtractorSniffer(SniffMatroska);
#endif	
	AmlogicPlayerExtractorDataSource::RegisterExtractorSniffer(SniffDRM);
    return 0;
}


AmlogicPlayerExtractorDemux::AmlogicPlayerExtractorDemux(AVFormatContext *s)
	:mAudioTrack(NULL),
	 mVideoTrack(NULL),
	 mBuffer(NULL),
	 IsAudioAlreadyStarted(false),
	 IsVideoAlreadyStarted(false),
	 hasAudio(false),
	 hasVideo(false),
	 mLastAudioTimeUs(0),
	 mLastVideoTimeUs(0),
	 mVideoIndex(-1),
	 mAudioIndex(-1)
	 //mDecryptHandle(NULL),
     //mDrmManagerClient(NULL)
  {
    mReadDataSouce = AmlogicPlayerExtractorDataSource::CreateFromPbData(s->pb);
    unsigned long *pads;
    if (s && s->pb) {
        pads = s->pb->proppads;
        char *smimeType = (char *)pads[0];
        float confidence = (pads[1]) + ((float)(pads[2])) / 100000;
		 ALOGV( "s=%p pads[0]=%d smimeType=%s \n" , s, pads[0], smimeType);
        mMediaExtractor = MediaExtractor::Create(mReadDataSouce.get(), smimeType);
    }
}
AmlogicPlayerExtractorDemux::~AmlogicPlayerExtractorDemux()
{
	if(smimeType) {
		free(smimeType);
	}
}
//private static for demux struct.
//static
int AmlogicPlayerExtractorDemux:: extractor_probe(AVProbeData *p)
{
    sp<DataSource> mProbeDataSouce = AmlogicPlayerExtractorDataSource::CreateFromProbeData(p);
    AmlogicPlayerExtractorDataSource *mExtractorDataSource = (AmlogicPlayerExtractorDataSource *)mProbeDataSouce.get();
    if (mProbeDataSouce != NULL) {
        String8 mimeType;
        float confidence;
        sp<AMessage> meta;
        int gettype;	
		char  *mimestring;
        gettype = mExtractorDataSource->SimpleExtractorSniffer(&mimeType, &confidence, &meta);
		 ALOGV( "gettype=%d mimeType.string=%s\n" , gettype, mimeType.string());	
        if (gettype) {						
			mimestring = strdup(mimeType.string());
			ALOGV( "mimeType.string=%s\n" , mimeType.string());	    
			p->pads[0] = (unsigned long ) (mimestring );
            p->pads[1] = (unsigned long ) (confidence);
            p->pads[2] = (unsigned long ) (((int)(confidence * 100000)) % 100000);
			meta.clear();//del .
            mProbeDataSouce.clear();
            return 101;
        }
    }
    mProbeDataSouce.clear();
    return 0;
}
//static
int AmlogicPlayerExtractorDemux::extractor_read_header(AVFormatContext *s, AVFormatParameters *ap)
{
    AmlogicPlayerExtractorDemux *demux = new AmlogicPlayerExtractorDemux(s);
	if(!demux) {
		ALOGE( "Creat Extractor failed");
		return NULL;
	}
	s->priv_data = (void *)demux;
    return demux->ReadHeader(s, ap);
}
//static
int AmlogicPlayerExtractorDemux::extractor_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    AmlogicPlayerExtractorDemux *demux = (AmlogicPlayerExtractorDemux *)s->priv_data;
    return demux->ReadPacket(s, pkt);
}
//static
int AmlogicPlayerExtractorDemux::extractor_close(AVFormatContext *s)
{
    AmlogicPlayerExtractorDemux *demux = (AmlogicPlayerExtractorDemux *)s->priv_data;
    return demux->Close(s);
}
//static
int AmlogicPlayerExtractorDemux::extractor_read_seek(AVFormatContext *s, int stream_index, int64_t timestamp, int flags)
{
    AmlogicPlayerExtractorDemux *demux = (AmlogicPlayerExtractorDemux *)s->priv_data;
    return demux->ReadSeek(s, stream_index, timestamp, flags);
}

int AmlogicPlayerExtractorDemux::SetStreamInfo(AVStream *st, sp<MetaData> meta,  String8 mime)
{
		bool success ;
	
		int64_t duration = -1ll ;	
		success = meta->findInt64(kKeyDuration, &duration);
		if(success){
			st->duration = duration;
		}
		ALOGV("%s:%d : duration=%lld (us)\n", __FUNCTION__, __LINE__, duration);

		int32_t bps = 0;
		success = meta->findInt32(kKeyBitRate, &bps);
		if(success){
			st->codec->bit_rate = bps;					
		}	
		ALOGV("%s:%d :  bit_rate=%d\n", __FUNCTION__, __LINE__, bps);
		st->time_base.den = 90000;
		st->time_base.num = 1;
		ALOGV("%s:%d : mime=%s\n", __FUNCTION__, __LINE__, mime.string());
	   if( !strncasecmp(mime.string(), "video/", 6)){		 	
			int32_t displayWidth = 0, displayHeight = 0, Width = 0, Height = 0;
			st->codec->codec_type = CODEC_TYPE_VIDEO;
            success = meta->findInt32(kKeyDisplayWidth, &displayWidth);
            if (success) {
				success = meta->findInt32(kKeyDisplayHeight, &displayHeight);              
            }           
			if (success) {
                st->codec->width = displayWidth;
                st->codec->height = displayHeight;
            }  else {
            	success = meta->findInt32(kKeyWidth, &Width);
	            if (success) {
					success = meta->findInt32(kKeyHeight, &Height);              
	            }           
				if (success) {
	                st->codec->width = Width;
	                st->codec->height = Height;
	            } 
            } 
			ALOGV("%s:%d : video width=%d height=%d\n", __FUNCTION__, __LINE__, st->codec->width,  st->codec->height);			
			int32_t frameRate = 0;
			success = meta->findInt32(kKeyFrameRate, &frameRate);
			if(success){
				st->r_frame_rate.den = 1;
				st->r_frame_rate.num = frameRate;
			}		
			ALOGV("%s:%d : video fps=%d \n", __FUNCTION__, __LINE__, frameRate);			
			
			if(strstr(mime.string(), "avc"))
				st->codec->codec_id = CODEC_ID_H264;
				
	    }
		if( !strncasecmp(mime.string(), "audio/", 6)){
			int32_t sampleRate = 0, channelNum = 0, audioProfile = 0;
			st->codec->codec_type = CODEC_TYPE_AUDIO;
			ALOGV("%s:%d :codec_type:%d\n", __FUNCTION__, __LINE__, st->id, st->codec->codec_type);

			success = meta->findInt32(kKeySampleRate, &sampleRate);
			if (success)
				st->codec->sample_rate = sampleRate;
			success = meta->findInt32(kKeyChannelCount, &channelNum);
			if(success)
				st->codec->channels = channelNum;
			success = meta->findInt32(kKeyAudioProfile, &audioProfile);
			if(success)
				st->codec->audio_profile = audioProfile;
			ALOGV("%s:%d :audio sr=%d chnum=%d audio_profile=%d\n", __FUNCTION__, __LINE__, st->codec->sample_rate, st->codec->channels,st->codec->audio_profile);

			if(strstr(mime.string(), "mpeg-L2"))
				st->codec->codec_id = CODEC_ID_MP2;				
			else if(strstr(mime.string(), "mp4a-latm"))
				st->codec->codec_id = CODEC_ID_AAC;					
		}
		return 0;
}
int AmlogicPlayerExtractorDemux::ReadHeader(AVFormatContext *s, AVFormatParameters *ap)
{
	size_t i;
	AVStream *st;
	#define AV_NOPTS_VALUE 0x8000000000000000

	if(mMediaExtractor == NULL){
		ALOGE( "NO mMediaExtractor!\n");	
		return -1;
	}
	for ( i = 0; i < mMediaExtractor->countTracks(); ++i) {
		 st = av_new_stream(s, i);		    
		 if(!st) {
		 	ALOGE( "creat new stream failed!");
			return -1;
		 }		
		sp<MetaData> meta = mMediaExtractor->getTrackMetaData(i);	
		const char *_mime;
        CHECK(meta->findCString(kKeyMIMEType, &_mime));
        String8 mime = String8(_mime);
		st = s->streams[i];
	    SetStreamInfo(st, meta, mime);
		if (st->duration > 0 && (s->duration == AV_NOPTS_VALUE ||st->duration > s->duration )){
			ALOGV("s->duration=%lld st duration=%lld\n", s->duration, st->duration);
			s->duration = st->duration;
		}			
		if( !strncasecmp(mime.string(), "video/", 6) && mMediaExtractor->getTrack(i) !=NULL){
			mVideoTrack = mMediaExtractor->getTrack(i);		
			mVideoIndex = i;
			hasVideo = true;	
			ALOGV("[%s:%d]HAS VIDEO index=%d\n", __FUNCTION__, __LINE__, i);
		}
		if( !strncasecmp(mime.string(), "audio/", 6) && mMediaExtractor->getTrack(i) !=NULL){
			mAudioTrack = mMediaExtractor->getTrack(i);
			mAudioIndex = i;
			hasAudio = true;		
			ALOGV("[%s:%d]HAS AUDIO index=%d\n", __FUNCTION__, __LINE__, i);
		}
		if (s->duration != AV_NOPTS_VALUE) {
			s->duration = s->duration ;
			ALOGV("[%s:%d]duration=%lld \n", __FUNCTION__, __LINE__, s->duration );
		}
	}	
    return 0;
}
int AmlogicPlayerExtractorDemux::ReadPacket(AVFormatContext *s, AVPacket *pkt)
{
	if (pkt == NULL)
		return -1;

	 int64_t TimeUs = 0ll;	
	 status_t err = OK; 	
	 int ret = 0;
	
	  CHECK(mBuffer == NULL);
		
	 MediaSource::ReadOptions options; 	 
	//ALOGV("[%s]mLastVideoTimeUs=%lld mLastAudioTimeUs=%lld\n", __FUNCTION__, mLastVideoTimeUs, mLastAudioTimeUs);
    if (mLastVideoTimeUs <=mLastAudioTimeUs ) {					
		if (hasVideo==true && mVideoTrack != NULL){
			 if(IsVideoAlreadyStarted == false) {	 					 	
				status_t err = mVideoTrack->start();
				if (err != OK) {
				        mVideoTrack.clear();
						ALOGE("[%s]video track start failed err=%d\n", __FUNCTION__,err);
				        return err;
				}				
				IsVideoAlreadyStarted = true;
			 }						
			err = mVideoTrack->read(&mBuffer, &options);		
			if(err==OK ) {				
				if (mBuffer->meta_data()->findInt64(kKeyTime, &TimeUs)
	                      && TimeUs >= 0) {
	                  ALOGV("Key Video targetTimeUs = %lld us", TimeUs);
	                  mLastVideoTimeUs = TimeUs;
				}		             
			}	
			uint32_t size = mBuffer->range_length();	
			//ALOGV("[%s:%d]buffer data size=%d\n", __FUNCTION__, __LINE__, size);		
			if(size > 0){													
				if(ret = av_new_packet(pkt, size) < 0)
					return ret;		
				memcpy((char *)pkt->data,  (const char *)(mBuffer->data( ) + mBuffer->range_offset( )), size);
				pkt->size = size;
				pkt->pts = TimeUs * 9 / 100;					
				pkt->stream_index = mVideoIndex;						
			}	
			else {
				ALOGE("[%s]video track read failed err=%d\n", __FUNCTION__,err);
			}
		}
	} else {
    	if (hasAudio == true && mAudioTrack != NULL) {
				 if(IsAudioAlreadyStarted == false) {	 
					status_t err = mAudioTrack->start();
					if (err != OK) {
					        mAudioTrack.clear();
							ALOGE("[%s]audio track start failed err=%d\n", __FUNCTION__,err);
					        return err;
					} 
					IsAudioAlreadyStarted = true;
				}
				err = mAudioTrack->read(&mBuffer, &options);
				if(err == OK ) {						
					if (mBuffer->meta_data()->findInt64(kKeyTime, &TimeUs)
	                        && TimeUs >= 0) {
	                    ALOGV("Key Audio targetTimeUs = %lld us", TimeUs);
	                    mLastAudioTimeUs = TimeUs;			               
					}
					uint32_t size = mBuffer->range_length();	
					//ALOGV("[%s:%d]buffer data size=%d\n", __FUNCTION__, __LINE__, size);		
					if(size > 0){													
						if(err = av_new_packet(pkt, size) < 0)
							return err;		
						memcpy((char *)pkt->data,  (const char *)(mBuffer->data( ) + mBuffer->range_offset( )), size);
						pkt->size = size;
						pkt->pts = TimeUs * 9 /100;
						//pkt->dts = DecodetimeUs;
						pkt->stream_index = mAudioIndex;
					}				
				}			else {
					ALOGE("[%s]audio track read failed err=%d\n", __FUNCTION__,err);
				}
    		}
    }	
	if (mBuffer !=NULL) {
		mBuffer->release();
		mBuffer = NULL;
	}	
    return (pkt->size > 0 ? pkt->size : err);
}
int AmlogicPlayerExtractorDemux::Close(AVFormatContext *s)
{
	ALOGV("Close");
	if(mVideoTrack!=NULL && IsVideoAlreadyStarted == true){
		mVideoTrack->stop();
		mVideoTrack.clear();
		ALOGV("Clear VideoTrack");
	}
	if(mAudioTrack!=NULL && IsAudioAlreadyStarted == true){
		mAudioTrack->stop();
		mAudioTrack.clear();
		ALOGV("Clear AudioTrack");
	}
	if (mMediaExtractor!=NULL)
		mMediaExtractor.clear();
	if (mBuffer != NULL) {
		mBuffer->release();
		mBuffer = NULL;
	}
	if(mReadDataSouce.get()!=NULL){
		AmlogicPlayerExtractorDataSource *mExtractorDataSource = (AmlogicPlayerExtractorDataSource *)mReadDataSouce.get();
		mExtractorDataSource->release();
	}
    return 0;
}
int AmlogicPlayerExtractorDemux::ReadSeek(AVFormatContext *s, int stream_index, int64_t timestamp, int flags)
{
	  MediaSource::ReadOptions options;
		ALOGV("[%s:%d]stream_index=%d mSeeking=%d\n", __FUNCTION__, __LINE__, stream_index, mSeeking);
        if (mSeeking != NO_SEEK) {
            ALOGV("seeking to %lld us (%.2f secs)", timestamp, timestamp / 1E6);

            options.setSeekTo(
                    timestamp,
                    mSeeking == SEEK_VIDEO_ONLY
                        ? MediaSource::ReadOptions::SEEK_NEXT_SYNC
                        : MediaSource::ReadOptions::SEEK_CLOSEST_SYNC);
        }
    return 0;
}

};


