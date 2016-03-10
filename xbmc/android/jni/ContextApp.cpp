/*
 *      Copyright (C) 2013 Team XBMC
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

#include "ContextApp.h"
#include "PackageManager.h"
#include <android/log.h>
#include "Intent.h"
#include "IntentFilter.h"
#include "ClassLoader.h"
#include "jutils/jutils-details.hpp"
#include "BroadcastReceiver.h"
#include "JNIThreading.h"
#include "ApplicationInfo.h"
#include "File.h"
#include "ContentResolver.h"
#include "BaseColumns.h"
#include "MediaStore.h"
#include "PowerManager.h"
#include "Cursor.h"
#include "ConnectivityManager.h"
#include "AudioFormat.h"
#include "AudioManager.h"
#include "AudioTrack.h"
#include "Surface.h"
#include "MediaCodec.h"
#include "MediaCodecInfo.h"
#include "MediaFormat.h"
#include "Window.h"
#include "View.h"
#include "Build.h"
#include "DisplayMetrics.h"
#include "Intent.h"
#include "KeyEvent.h"

#include <android/native_activity.h>

using namespace jni;

jhobject CJNIContextApp::m_context(0);

CJNIContextApp::CJNIContextApp()
{
}

CJNIContextApp::CJNIContextApp(const jobject& object, JavaVM* vm, JNIEnv* env)
{
  xbmc_jni_on_load(vm, env);
  m_context.reset(object);
  m_context.setGlobal();
}

void CJNIContextApp::setContext(const jobject& object, JavaVM* vm, JNIEnv* env)
{
  xbmc_jni_on_load(vm, env);
  m_context.reset(object);
  m_context.setGlobal();
}

void CJNIContextApp::setEnv(JavaVM* vm, JNIEnv* env)
{
  xbmc_jni_on_load(vm, env);
} 

CJNIContextApp::~CJNIContextApp()
{
}

CJNIApplicationInfo CJNIContextApp::getApplicationInfo()
{
  return call_method<jhobject>(m_context,
    "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
}

std::string CJNIContextApp::getPackageResourcePath()
{
  return jcast<std::string>(call_method<jhstring>(m_context,
    "getPackageResourcePath", "()Ljava/lang/String;"));
}

CJNIFile CJNIContextApp::getCacheDir()
{
  return call_method<jhobject>(m_context,
    "getCacheDir", "()Ljava/io/File;");
}

CJNIFile CJNIContextApp::getDir(const std::string &path, int mode)
{
  return call_method<jhobject>(m_context,
    "getDir", "(Ljava/lang/String;I)Ljava/io/File;",
    jcast<jhstring>(path), mode);
}

CJNIFile CJNIContextApp::getExternalFilesDir(const std::string &path)
{
  return call_method<jhobject>(m_context,
    "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;",
    jcast<jhstring>(path));
}
