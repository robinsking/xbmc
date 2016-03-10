#pragma once
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

#include "JNIBase.h"
class CJNIApplicationInfo;
class CJNIFile;

class CJNIContextApp
{
public:
  static CJNIApplicationInfo getApplicationInfo();
  static std::string getPackageResourcePath();
  static CJNIFile getCacheDir();
  static CJNIFile getDir(const std::string &path, int mode);
  static CJNIFile getExternalFilesDir(const std::string &path);
  CJNIContextApp();
  void setContext(const jobject& object, JavaVM* vm, JNIEnv* env);
  void setEnv(JavaVM* vm, JNIEnv* env);
  ~CJNIContextApp();

protected:
  CJNIContextApp(const jobject& object, JavaVM* vm, JNIEnv* env);

  static jni::jhobject m_context;

protected:

  void operator=(CJNIContextApp const&){};
};
