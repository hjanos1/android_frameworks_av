
#define LOG_NDEBUG 0
#define LOG_TAG "AmlogicPlayerRender"
#include "utils/Log.h"
#include <stdio.h>

#include "AmlogicPlayerRender.h"

#include <gui/Surface.h>
#include <gui/ISurfaceTexture.h>
#include <gui/SurfaceTextureClient.h>
#include <gui/ISurfaceComposer.h>

#include <android/native_window.h>
#include <cutils/properties.h>

#include "AmlogicPlayer.h"

#include <Amavutils.h>



//#define  TRACE()	ALOGV("[%s::%d]\n",__FUNCTION__,__LINE__)
#define  TRACE()	


namespace android {

AmlogicPlayerRender::AmlogicPlayerRender()
{
	AmlogicPlayerRender(NULL);
}

AmlogicPlayerRender::AmlogicPlayerRender(const sp<ANativeWindow> &nativeWindow)
	:Thread(false),mNativeWindow(nativeWindow)
{
	/*make sure first setting,*/
	mnewRect=Rect(0,0);
	moldRect=Rect(2,2);
	mcurRect=Rect(3,3);
	mVideoTransfer=0;
	mVideoTransferChanged=true;
	mWindowChanged=3;
	mVideoFrameReady=3;
	mUpdateInterval_ms=100;
	mRunning=false;
	mPaused=true;
	mOSDisScaled=false;
	return;
}

	
AmlogicPlayerRender::~AmlogicPlayerRender()
{
	Pause();
	Stop();
	updateOSDscaling(0);
	return;
}


bool AmlogicPlayerRender::PlatformWantOSDscale(void)
{
	char mode[32]="panel";
	#define OSDSCALESET					"rw.fb.need2xscale"
	#define PLAYERER_ENABLE_SCALER		"media.amplayer.osd2xenable"
	#define DISP_MODE_PATH  				"/sys/class/display/mode"
	
	if(	AmlogicPlayer::PropIsEnable(PLAYERER_ENABLE_SCALER) && /*Player has enabled scaler*/
		AmlogicPlayer::PropIsEnable(OSDSCALESET) && /*Player framebuffer have enable*/
		(!amsysfs_get_sysfs_str(DISP_MODE_PATH,mode,32)&& !strncmp(mode,"1080p",5)))/*hdmi  1080p*/
	{
		ALOGI("PlatformWantOSDscale true\n");

		return true;
	}
	
	return false;
}

int  AmlogicPlayerRender::updateOSDscaling(int enable)
{
	bool platneedscale;
	int needed=0;
	if(	mVideoTransfer==0 ||
		mVideoTransfer==HAL_TRANSFORM_ROT_180){
		needed=1;
		//only scale on equal or large than 720p 
	}
	platneedscale=PlatformWantOSDscale();/*platform need it*/
	if(enable && needed && !mOSDisScaled && platneedscale){
		mOSDisScaled=true;
		ALOGI("Enabled width scaling\n");
		amdisplay_utils_set_scale_mode(2,1);
	}else if(!enable || (mOSDisScaled && !needed)){
		ALOGI("Disable width scaling\n");
		amdisplay_utils_set_scale_mode(1,1);
	}else{
		/*no changes do nothing*/
	}
	return 0;
}


void AmlogicPlayerRender::onFirstRef()
{
	 TRACE();
	if(initCheck()==OK){
		
		int usage=0;
		int err;
		 TRACE();
		native_window_set_usage(mNativeWindow.get(),usage | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP | GRALLOC_USAGE_AML_VIDEO_OVERLAY);
		native_window_set_buffer_count(mNativeWindow.get(),3);
		native_window_set_buffers_format(mNativeWindow.get(),WINDOW_FORMAT_RGBA_8888);
		native_window_set_scaling_mode(mNativeWindow.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
	}
	return ;
}

status_t AmlogicPlayerRender::initCheck()
{
	if(mNativeWindow.get()!=NULL)
	    return OK;
	return UNKNOWN_ERROR;
}

status_t AmlogicPlayerRender::readyToRun()
{
	 TRACE();

	return OK;
}


status_t AmlogicPlayerRender::VideoFrameUpdate()
{
	ANativeWindowBuffer*buf;
	unsigned char *vaddr;
	int err;
	int transfer=0;
	TRACE();
	int rec_changed=(mcurRect!=mnewRect);
	mNativeWindow->query(mNativeWindow.get(), NATIVE_WINDOW_TRANSFORM_HINT,&transfer);
	if(transfer!=mVideoTransfer){
		mVideoTransferChanged=true;
		mVideoTransfer=transfer;
		mWindowChanged=3;
	}	
	if(1 || rec_changed){ //always in.. ransfer needed
		err=mNativeWindow->dequeueBuffer_DEPRECATED(mNativeWindow.get(), &buf);
		if (err != 0) {
		    ALOGE("dequeueBuffer failed: %s (%d)", strerror(-err), -err);
		    return -1;
		}
		TRACE();
		mNativeWindow->lockBuffer_DEPRECATED(mNativeWindow.get(), buf);
		//GraphicBufferMapper &mapper = GraphicBufferMapper::get();
		TRACE();
		//Rect bounds(mCropWidth, mCropHeight);
		sp<GraphicBuffer> graphicBuffer(new GraphicBuffer(buf, false));
		graphicBuffer->lock(1,(void **)&vaddr);
		///mapper.lock(buf->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, &vaddr)
		if(vaddr!=NULL && rec_changed ){//if recchanged,we do memset...
			TRACE();
			ALOGI("getWidth=%d,getHeight=%d\n",graphicBuffer->getWidth(),graphicBuffer->getHeight());
			memset(vaddr,0x0,graphicBuffer->getWidth()*graphicBuffer->getHeight()*4);
		}else{
			
		}
		TRACE();
		graphicBuffer->unlock();
		///mapper.unlock(buf->handle);
		mNativeWindow->queueBuffer_DEPRECATED(mNativeWindow.get(), buf);
		graphicBuffer.clear();
		TRACE();
	}
	return OK;
}	
status_t AmlogicPlayerRender::VideoPositionUpdate()
{
	int angle=0;
	int ret;
	int left,top,width,height;
	mcurRect=mnewRect;
	mVideoTransferChanged=false;
	left=mnewRect.left;
	top=mnewRect.top;
	width=mnewRect.width();
	height=mnewRect.height();
	switch(mVideoTransfer){
		case HAL_TRANSFORM_ROT_270:
			angle=270;
			left=mnewRect.top;
			top=mnewRect.left;
			width=mnewRect.height();
			height=mnewRect.width();
			break;
		case HAL_TRANSFORM_ROT_180:
			angle=180;
			break;
		case HAL_TRANSFORM_ROT_90:
			left=mnewRect.top;
			top=mnewRect.left;
			width=mnewRect.height();
			height=mnewRect.width();
			angle=90;
			break;
		case HAL_TRANSFORM_FLIP_V:
		case HAL_TRANSFORM_FLIP_H:
		default:
			break;
	}
	ALOGI("VideoPositionUpdate =[ltwh:%d,%d,%d,%d,A:%d]\n",
				mnewRect.left,mnewRect.top,mnewRect.width(),mnewRect.height(),angle);
	ret=amvideo_utils_set_virtual_position(left,top,width,height,angle);
	
	return ret;
}

status_t AmlogicPlayerRender::Update()
{
	 TRACE();
	if(mNativeWindow.get()!=NULL &&( mVideoFrameReady>0 || mWindowChanged>0)){
		mVideoFrameReady--;
		VideoFrameUpdate();
	}
	if(mWindowChanged){
		mWindowChanged--;
		updateOSDscaling(1);
		// set to full stretch mode
		amvideo_utils_set_screen_mode(1);
		//VideoPositionUpdate();
	}
	return OK;
}
status_t AmlogicPlayerRender::ScheduleOnce()
{
	TRACE();
	mCondition.signal();
	return OK;
}


status_t AmlogicPlayerRender::Start()
{
	TRACE();
	Mutex::Autolock l(mMutex);
	if(!mRunning){
		mRunning=true;
		run();
	}
	mPaused=false;
	ScheduleOnce();
	return OK;
}

status_t AmlogicPlayerRender::Stop()
{
	TRACE();
	{
		Mutex::Autolock l(mMutex);
		TRACE();
		mRunning=false;
	}
	requestExitAndWait();
	return OK;
}

status_t AmlogicPlayerRender::Pause()
{
	TRACE();
	Mutex::Autolock l(mMutex);
	ScheduleOnce();
	mPaused=true;
	return OK;
}



status_t AmlogicPlayerRender::onSizeChanged(Rect newR,Rect oldR)
{
	mnewRect=newR;
	moldRect=oldR;
	mWindowChanged=3;
	mVideoFrameReady=3;
	ScheduleOnce();
	TRACE();
	return OK;
}



bool AmlogicPlayerRender::threadLoop()
{
	TRACE();
	mMutex.lock();
	mCondition.waitRelative(mMutex,milliseconds_to_nanoseconds(mUpdateInterval_ms));
	mMutex.unlock();
	TRACE();
	if(mRunning && !mPaused)
		Update();
	return true;
}

}
