// Copyright (c) 2004-present, Facebook, Inc.

// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#include <cxxreact/JSCExecutor.h>

#include <string>

#include <cxxreact/Platform.h>
#include <fb/fbjni.h>
#include <folly/Conv.h>
#include <folly/dynamic.h>
#include <folly/Memory.h>
#include <jschelpers/JSCHelpers.h>

#include "JSCPerfLogging.h"
#include "JSLogging.h"
#include "JReactMarker.h"

using namespace facebook::jni;

namespace facebook {
namespace react {

namespace {

ExceptionHandling::ExtractedEror extractJniError(const std::exception& ex, const char *context) {
  auto jniEx = dynamic_cast<const jni::JniException *>(&ex);
  if (!jniEx) {
    return {};
  }

  auto stackTrace = jniEx->getThrowable()->getStackTrace();
  std::ostringstream stackStr;
  for (int i = 0, count = stackTrace->size(); i < count; ++i) {
    auto frame = stackTrace->getElement(i);

    auto methodName = folly::to<std::string>(frame->getClassName(), ".",
      frame->getMethodName());

    // Cut off stack traces at the Android looper, to keep them simple
    if (methodName == "android.os.Looper.loop") {
      break;
    }

    stackStr << std::move(methodName) << '@' << frame->getFileName();
    if (frame->getLineNumber() > 0) {
      stackStr << ':' << frame->getLineNumber();
    }
    stackStr << std::endl;
  }

  auto msg = folly::to<std::string>("Java exception in '", context, "'\n\n", jniEx->what());
  return {.message = msg, .stack = stackStr.str()};
}

JSValueRef nativePerformanceNow(
    JSContextRef ctx,
    JSObjectRef function,
    JSObjectRef thisObject,
    size_t argumentCount,
    const JSValueRef arguments[], JSValueRef *exception) {
  static const int64_t NANOSECONDS_IN_SECOND = 1000000000LL;
  static const int64_t NANOSECONDS_IN_MILLISECOND = 1000000LL;

  // Since SystemClock.uptimeMillis() is commonly used for performance measurement in Java
  // and uptimeMillis() internally uses clock_gettime(CLOCK_MONOTONIC),
  // we use the same API here.
  // We need that to make sure we use the same time system on both JS and Java sides.
  // Links to the source code:
  // https://android.googlesource.com/platform/frameworks/native/+/jb-mr1-release/libs/utils/SystemClock.cpp
  // https://android.googlesource.com/platform/system/core/+/master/libutils/Timers.cpp
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  int64_t nano = now.tv_sec * NANOSECONDS_IN_SECOND + now.tv_nsec;
  return Value::makeNumber(ctx, (nano / (double)NANOSECONDS_IN_MILLISECOND));
}

JSValueRef nativeLoggingHook(
  JSContextRef ctx,
  JSObjectRef function,
  JSObjectRef thisObject,
  size_t argumentCount,
  const JSValueRef arguments[],
  JSValueRef* exception) {
  android_LogPriority logLevel = ANDROID_LOG_DEBUG;
  if (argumentCount > 1) {
    int level = (int)Value(ctx, arguments[1]).asNumber();
    // The lowest log level we get from JS is 0. We shift and cap it to be
    // in the range the Android logging method expects.
    logLevel = std::min(
      static_cast<android_LogPriority>(level + ANDROID_LOG_DEBUG),
      ANDROID_LOG_FATAL);
  }
  if (argumentCount > 0) {
    String message = Value(ctx, arguments[0]).toString();
    reactAndroidLoggingHook(message.str(), logLevel);
  }
  return Value::makeUndefined(ctx);
}

}

namespace detail {

void injectJSExecutorAndroidPlatform() {
  // Inject some behavior into react/
  JReactMarker::setLogPerfMarkerIfNeeded();
  ExceptionHandling::platformErrorExtractor = extractJniError;
  JSCNativeHooks::loggingHook = nativeLoggingHook;
  JSCNativeHooks::nowHook = nativePerformanceNow;
  JSCNativeHooks::installPerfHooks = addNativePerfLoggingHooks;
}

}

std::unique_ptr<JSExecutorFactory> makeAndroidJSCExecutorFactory(
    const folly::dynamic& jscConfig) {
  detail::injectJSCExecutorAndroidPlatform();
  return folly::make_unique<JSCExecutorFactory>(std::move(jscConfig));
}

}
}
