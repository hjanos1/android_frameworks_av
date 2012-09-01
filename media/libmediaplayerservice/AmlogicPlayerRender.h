
#ifndef ANDROID_AMLOGIC_PLAYER_RENDER_H
#define ANDROID_AMLOGIC_PLAYER_RENDER_H

#include <stdint.h>
#include <sys/types.h>
#include <limits.h>

#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <utils/threads.h>
#include <android/native_window.h>
#include <ui/Rect.h>



namespace android {

class AmlogicPlayerRender: public Thread {
	
	public:
		AmlogicPlayerRender(void);
		AmlogicPlayerRender(const sp<ANativeWindow> &nativeWindow);
		~AmlogicPlayerRender();
		virtual status_t	readyToRun();
		virtual	void 		onFirstRef();
		virtual status_t	Start();
		virtual status_t	Stop();
		virtual status_t	Pause();
		
		virtual status_t 	onSizeChanged(Rect newR,Rect oldR);
			  	int 		updateOSDscaling(int enable);
		
		static  bool 		PlatformWantOSDscale(void);
		
		
	private:

		bool		threadLoop();
		status_t	Update();
		status_t	VideoFrameUpdate();
		status_t	VideoPositionUpdate();
		status_t	ScheduleOnce();
		status_t	initCheck();
		

		sp<ANativeWindow> 	mNativeWindow;

		int 		mUpdateInterval_ms;
		
		Rect 		mnewRect;
		Rect 		moldRect;
		Rect 		mcurRect;
		int 		mVideoTransfer;
		bool 		mVideoTransferChanged;
		int			mWindowChanged;
		int			mVideoFrameReady;

		Mutex   	mMutex;
		Condition	mCondition;

		bool 		mRunning;
		bool 		mPaused;

		bool 		mOSDisScaled;
};

}
#endif
