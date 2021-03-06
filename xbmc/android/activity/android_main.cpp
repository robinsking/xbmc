/*
 *      Copyright (C) 2012-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <android_native_app_glue.h>
#include "EventLoop.h"
#include "XBMCApp.h"
#include "android/jni/SurfaceTexture.h"
#include "utils/StringUtils.h"
#include "CompileInfo.h"

#include "Application.h"

#include "android/activity/JNIMainActivity.h"
#include "messaging/ApplicationMessenger.h"

// copied from new android_native_app_glue.c
static void process_input(struct android_app* app, struct android_poll_source* source) {
    AInputEvent* event = NULL;
    int processed = 0;
    while (AInputQueue_getEvent(app->inputQueue, &event) >= 0) {
        if (AInputQueue_preDispatchEvent(app->inputQueue, event)) {
            continue;
        }
        int32_t handled = 0;
        if (app->onInputEvent != NULL) handled = app->onInputEvent(app, event);
        AInputQueue_finishEvent(app->inputQueue, event, handled);
        processed = 1;
    }
    if (processed == 0) {
        CXBMCApp::android_printf("process_input: Failure reading next input event: %s", strerror(errno));
    }
}

extern void android_main(struct android_app* state)
{
  {
    // make sure that the linker doesn't strip out our glue
    app_dummy();

    // revector inputPollSource.process so we can shut up
    // its useless verbose logging on new events (see ouya)
    // and fix the error in handling multiple input events.
    // see https://code.google.com/p/android/issues/detail?id=41755
    state->inputPollSource.process = process_input;

    CEventLoop eventLoop(state);
    CXBMCApp xbmcApp(state->activity);
    if (xbmcApp.isValid())
    {
      IInputHandler inputHandler;
      eventLoop.run(xbmcApp, inputHandler);
    }
    else
      CXBMCApp::android_printf("android_main: setup failed");

    CXBMCApp::android_printf("android_main: Exiting");
    // We need to call exit() so that all loaded libraries are properly unloaded
    // otherwise on the next start of the Activity android will simple re-use
    // those loaded libs in the state they were in when we quit XBMC last time
    // which will lead to crashes because of global/static classes that haven't
    // been properly uninitialized
  }
  exit(0);
}

#include "android/jni/ContextApp.h"
#include "android/jni/System.h"
#include "android/jni/ApplicationInfo.h"
#include "android/jni/File.h"
#include <android/log.h>

static pthread_t s_thread;
static CJNIContextApp s_context;
static JavaVM* s_vm;
static JNIEnv* s_env;
static ANativeActivity s_activity;
static CXBMCApp* s_xbmcApp;

static void* thread_run(void *arg);

jlong getTotalTime(JNIEnv *env, jobject context)
{
  if (s_xbmcApp == NULL)
  {
    CXBMCApp::android_printf(" => getTotalTime fail, s_xbmcApp is NULL");
    return 0;
  }

  jlong iTotalTime = (jlong)g_application.GetTotalTime();
  return iTotalTime > 0 ? iTotalTime : 0;
}

jlong getPlayTime(JNIEnv *env, jobject context)
{
  if (s_xbmcApp == NULL)
  {
    CXBMCApp::android_printf(" => getPlayTime fail, s_xbmcApp is NULL");
    return 0;
  }
  if (g_application.m_pPlayer->IsPlaying())
  {
    int64_t lPTS = (int64_t)(g_application.GetTime() * 1000);
    if (lPTS < 0) lPTS = 0;
    return lPTS;
  }
  return 0;
}

int getCurrentPlayState(JNIEnv *env, jobject context)
{
  if (s_xbmcApp == NULL)
  {
    CXBMCApp::android_printf(" => getCurrentCoverArt fail, s_xbmcApp is NULL");
    return 0;
  }
  return s_xbmcApp->getCurrentPlayState();
}
jbyteArray getCurrentCoverArt(JNIEnv *env, jobject context)
{
  char *pResult = NULL;
  if (s_xbmcApp == NULL)
  {
    CXBMCApp::android_printf(" => getCurrentCoverArt fail, s_xbmcApp is NULL");
    return 0;
  }
  int size = s_xbmcApp->getCurrentCoverArt(&pResult);
  if (size <= 0 || pResult == NULL)
  {
    CXBMCApp::android_printf(" => getCurrentCoverArt fail, fail to get image from xbmcApp");
    return 0;
  }
  jbyteArray bytes = env->NewByteArray(size);
  if (bytes == 0)
  {
    CXBMCApp::android_printf(" => getCurrentCoverArt fail, Error in creating the image");
    free(pResult);
    return 0;
  }
  env->SetByteArrayRegion(bytes, 0, size, (jbyte*) pResult);
  free(pResult);
  return bytes;
}
jstring getCurrentArtist(JNIEnv *env, jobject context)
{
  if (s_xbmcApp == NULL)
  {
    CXBMCApp::android_printf(" => getCurrentArtist fail, s_xbmcApp is NULL");
    return 0;
  }
  return env->NewStringUTF(s_xbmcApp->getCurrentArtist().c_str());
}
jstring getCurrentTitle(JNIEnv *env, jobject context)
{
  if (s_xbmcApp == NULL)
  {
    CXBMCApp::android_printf(" => getCurrentTitle fail, s_xbmcApp is NULL");
    return 0;
  }
  return env->NewStringUTF(s_xbmcApp->getCurrentTitle().c_str());
}

void stopNetworkService(JNIEnv *env, jobject context)
{
  CXBMCApp::android_printf(" => stopNetworkService");
  if (!s_xbmcApp)
  {
    CXBMCApp::android_printf(" => stopNetworkService fail, it is NOT started yet");
    return;
  }
  //CXBMCApp::stopNetworkService();
  KODI::MESSAGING::CApplicationMessenger::GetInstance().PostMsg(TMSG_QUIT);
  pthread_join(s_thread, NULL);
  CXBMCApp::android_printf(" => XBMC finished");
  delete(s_xbmcApp);
  s_xbmcApp = NULL;
}

void startXbmcInternal(int bSetupEnv) {
  if (s_xbmcApp)
  {
    CXBMCApp::android_printf(" => startXbmcInternal fail, it already started");
    return;
  }
  s_xbmcApp = new CXBMCApp(&s_activity);
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  pthread_create(&s_thread, &attr, thread_run, (bSetupEnv != 0 ? s_env : NULL));
  pthread_attr_destroy(&attr);
}

void startNetworkService(JNIEnv *env, jobject context)
{
  CXBMCApp::android_printf(" => startNetworkService");
  startXbmcInternal(1);
}

void startXbmc(JNIEnv *env, jobject context)
{
  s_env = env;
  s_context.setContext(context, s_vm, s_env);
  s_activity.env = s_env;
  s_activity.vm = s_vm;
  s_activity.clazz = env->NewGlobalRef(context);
  s_activity.sdkVersion= 21;
  startXbmcInternal(1);
}

void setupEnv()
{    
    setenv("XBMC_ANDROID_SYSTEM_LIBS", CJNISystem::getProperty("java.library.path").c_str(), 0);
    setenv("XBMC_ANDROID_LIBS", s_context.getApplicationInfo().nativeLibraryDir.c_str(), 0);
    setenv("XBMC_ANDROID_APK", s_context.getPackageResourcePath().c_str(), 0);

    std::string appName = CCompileInfo::GetAppName();
    StringUtils::ToLower(appName);
    std::string className = "org.xbmc." + appName;

    std::string xbmcHome = CJNISystem::getProperty("xbmc.home", "");
    if (xbmcHome.empty())
    {
      std::string cacheDir = s_context.getCacheDir().getAbsolutePath();
      setenv("KODI_BIN_HOME", (cacheDir + "/apk/assets").c_str(), 0);
      setenv("KODI_HOME", (cacheDir + "/apk/assets").c_str(), 0);
    }
    else
    {
      setenv("KODI_BIN_HOME", (xbmcHome + "/assets").c_str(), 0);
      setenv("KODI_HOME", (xbmcHome + "/assets").c_str(), 0);
    }

    std::string externalDir = CJNISystem::getProperty("xbmc.data", "");
    if (externalDir.empty())
    {
      CJNIFile androidPath = s_context.getExternalFilesDir("");
      if (!androidPath)
        androidPath = s_context.getDir(className.c_str(), 1);

      if (androidPath)
        externalDir = androidPath.getAbsolutePath();
    }

    if (!externalDir.empty())
      setenv("HOME", externalDir.c_str(), 0);
    else
      setenv("HOME", getenv("KODI_TEMP"), 0);

    std::string apkPath = getenv("XBMC_ANDROID_APK");
    apkPath += "/assets/python2.6";
    setenv("PYTHONHOME", apkPath.c_str(), 1);
    setenv("PYTHONPATH", "", 1);
    setenv("PYTHONOPTIMIZE","", 1);
    setenv("PYTHONNOUSERSITE", "1", 1);

}
static void* thread_run(void *arg)
{
  s_context.setEnv(s_vm, s_env);

  if (arg != NULL)
  {
    setupEnv();
  }

  CXBMCApp::android_printf(" => running XBMC_Run...");
  //try
  {
    int status = XBMC_Run(false);
    CXBMCApp::android_printf(" => XBMC_Run finished with %d", status);
  }
  // Let it crash since we can recover it...
  /*catch(...)
  {
    CXBMCApp::android_printf("ERROR: Exception caught on main loop. Exiting");
  }*/
  return NULL;
}


extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
  jint version = JNI_VERSION_1_6;
  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), version) != JNI_OK)
    return -1;

  s_vm = vm;
  std::string appName = CCompileInfo::GetAppName();
  StringUtils::ToLower(appName);
  std::string mainClass = "org/xbmc/" + appName + "/Main";
  std::string bcReceiver = "org/xbmc/" + appName + "/XBMCBroadcastReceiver";
  std::string frameListener = "org/xbmc/" + appName + "/XBMCOnFrameAvailableListener";
  std::string settingsObserver = "org/xbmc/" + appName + "/XBMCSettingsContentObserver";
  std::string audioFocusChangeListener = "org/xbmc/" + appName + "/XBMCOnAudioFocusChangeListener";
  std::string kodiApp = "org/xbmc/" + appName + "/KodiApp";

  jclass cMain = env->FindClass(mainClass.c_str());
  if(cMain)
  {
    JNINativeMethod mOnNewIntent = {
      "_onNewIntent",
      "(Landroid/content/Intent;)V",
      (void*)&CJNIMainActivity::_onNewIntent
    };
    env->RegisterNatives(cMain, &mOnNewIntent, 1);

    JNINativeMethod mCallNative = {
      "_callNative",
      "(JJ)V",
      (void*)&CJNIMainActivity::_callNative
    };
    env->RegisterNatives(cMain, &mCallNative, 1);
  }

  jclass cBroadcastReceiver = env->FindClass(bcReceiver.c_str());
  if(cBroadcastReceiver)
  {
    JNINativeMethod mOnReceive =  {
      "_onReceive",
      "(Landroid/content/Intent;)V",
      (void*)&CJNIBroadcastReceiver::_onReceive
    };
    env->RegisterNatives(cBroadcastReceiver, &mOnReceive, 1);
  }

  jclass cFrameAvailableListener = env->FindClass(frameListener.c_str());
  if(cFrameAvailableListener)
  {
    JNINativeMethod mOnFrameAvailable = {
      "_onFrameAvailable",
      "(Landroid/graphics/SurfaceTexture;)V",
      (void*)&CJNISurfaceTextureOnFrameAvailableListener::_onFrameAvailable
    };
    env->RegisterNatives(cFrameAvailableListener, &mOnFrameAvailable, 1);
  }

  jclass cSettingsObserver = env->FindClass(settingsObserver.c_str());
  if(cSettingsObserver)
  {
    JNINativeMethod mOnVolumeChanged = {
      "_onVolumeChanged",
      "(I)V",
      (void*)&CJNIMainActivity::_onVolumeChanged
    };
    env->RegisterNatives(cSettingsObserver, &mOnVolumeChanged, 1);
  }

  jclass cAudioFocusChangeListener = env->FindClass(audioFocusChangeListener.c_str());
  if(cAudioFocusChangeListener)
  {
    JNINativeMethod mOnAudioFocusChange = {
      "_onAudioFocusChange",
      "(I)V",
      (void*)&CJNIMainActivity::_onAudioFocusChange
    };
    env->RegisterNatives(cAudioFocusChangeListener, &mOnAudioFocusChange, 1);
  }

  jclass cKodiApp = env->FindClass(kodiApp.c_str());
  if(cKodiApp)
  {
    JNINativeMethod mStartXbmc = {
      "_startXbmc",
      "()V",
      (void*)&startXbmc
    };
    env->RegisterNatives(cKodiApp, &mStartXbmc, 1);

    JNINativeMethod mStartNetworkService = {
      "_startNetworkService",
      "()V",
      (void*)&startNetworkService
    };
    env->RegisterNatives(cKodiApp, &mStartNetworkService, 1);

    JNINativeMethod mStopNetworkService = {
      "_stopNetworkService",
      "()V",
      (void*)&stopNetworkService
    };
    env->RegisterNatives(cKodiApp, &mStopNetworkService, 1);

    JNINativeMethod mGetCurrentCoverArt = {
      "_getCurrentCoverArt",
      "()[B",
      (void*)&getCurrentCoverArt
    };
    env->RegisterNatives(cKodiApp, &mGetCurrentCoverArt, 1);

    JNINativeMethod mGetCurrentPlayState = {
      "_getCurrentPlayState",
      "()I",
      (void*)&getCurrentPlayState
    };
    env->RegisterNatives(cKodiApp, &mGetCurrentPlayState, 1);

    JNINativeMethod mGetCurrentArtist = {
      "_getCurrentArtist",
      "()Ljava/lang/String;",
      (void*)&getCurrentArtist
    };
    env->RegisterNatives(cKodiApp, &mGetCurrentArtist, 1);

    JNINativeMethod mGetCurrentTitle = {
      "_getCurrentTitle",
      "()Ljava/lang/String;",
      (void*)&getCurrentTitle
    };
    env->RegisterNatives(cKodiApp, &mGetCurrentTitle, 1);

    JNINativeMethod mGetTotalTime = {
      "_getTotalTime",
      "()J",
      (void*)&getTotalTime
    };
    env->RegisterNatives(cKodiApp, &mGetTotalTime, 1);

    JNINativeMethod mGetPlayTime = {
      "_getPlayTime",
      "()J",
      (void*)&getPlayTime
    };
    env->RegisterNatives(cKodiApp, &mGetPlayTime, 1);
  }

  return version;
}
