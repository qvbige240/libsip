/**
 * History:
 * ================================================================
 * 2019-11-25 qing.zou created
 *
 */
#ifndef SIP_TYPEDEF_H
#define SIP_TYPEDEF_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
#define SIP_BEGIN_DELS extern "C" {
#define SIP_END_DELS }
#else
#define SIP_BEGIN_DELS
#define SIP_END_DELS
#endif

#ifndef return_if_fail
#define return_if_fail(p) if(!(p)) \
	{printf("%s:%d Error: "#p" failed.\n", __FILE__, __LINE__); \
	(void)fprintf(stderr, "%s:%d Error: "#p" failed.\n", \
	__FILE__, __LINE__); return;}
#define return_val_if_fail(p, ret) if(!(p)) \
	{printf("%s:%d Error: "#p" failed.\n", __FILE__, __LINE__); \
	(void)fprintf(stderr, "%s:%d Error: "#p" failed.\n", \
	__FILE__, __LINE__); return (ret);}
#endif

#ifndef DECL_PRIV
  #define DECL_PRIV(thiz, priv) PrivInfo* priv = thiz != NULL ? (PrivInfo*)thiz->priv : NULL
#endif

#ifndef INLINE
#define INLINE __inline__
#else
//#define INLINE
#endif

#define	SIP_MALLOC		malloc
#define	SIP_CALLOC		calloc
#define	SIP_FREE		free
#define	SIP_MEMSET		memset
#define	SIP_MEMCPY		memcpy
#define	SIP_REALLOC		realloc

#define SIPAPI			extern	

//#define SIP_CONFIG_ANDROID 0

// #if defined(SIP_CONFIG_ANDROID) && SIP_CONFIG_ANDROID==1

//   #include <android/log.h>
//   #define LOG_TAG "sip"
//   #define SIP_LOGD(x)	XXX_LOGD x
//   #define XXX_LOGD(format, args...) 	__android_log_print(ANDROID_LOG_DEBUG,LOG_TAG, format, ##args)
//   #define SIP_LOGI(format, args...) 	__android_log_print(ANDROID_LOG_INFO, LOG_TAG, format, ##args)
//   #define SIP_LOGW(format, args...) 	__android_log_print(ANDROID_LOG_WARN, LOG_TAG, format, ##args)
//   #define SIP_LOGE(format, args...) 	__android_log_print(ANDROID_LOG_ERROR,LOG_TAG, format, ##args)
//   #define SIP_LOGF(format, args...) 	__android_log_print(ANDROID_LOG_FATAL,LOG_TAG, format, ##args)

// #else
// #ifdef USE_ZLOG
// #include "zlog.h"
// /* use zlog */
// #ifndef TIMA_LOGI
// /* tima log macros */
// #define TIMA_LOGF(format, args...) \
// 	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
// 	ZLOG_LEVEL_FATAL, format, ##args)
// #define TIMA_LOGE(format, args...) \
// 	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
// 	ZLOG_LEVEL_ERROR, format, ##args)
// #define TIMA_LOGW(format, args...) \
// 	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
// 	ZLOG_LEVEL_WARN, format, ##args)
// #define TIMA_LOGN(format, args...) \
// 	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
// 	ZLOG_LEVEL_NOTICE, format, ##args)
// #define TIMA_LOGI(format, args...) \
// 	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
// 	ZLOG_LEVEL_INFO, format, ##args)
// #define TIMA_LOGD(format, args...) \
// 	dzlog(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
// 	ZLOG_LEVEL_DEBUG, format, ##args)
// #endif

// #define SIP_LOGD(x)	XXX_LOGD x
// #define XXX_LOGD(format, args...) 	TIMA_LOGD(format, ##args)
// #define SIP_LOGI(format, args...) TIMA_LOGI(format, ##args)
// #define SIP_LOGW(format, args...) TIMA_LOGW(format, ##args)
// #define SIP_LOGE(format, args...) TIMA_LOGE(format, ##args)
// #define SIP_LOGF(format, args...) TIMA_LOGF(format, ##args)
// #else
// 	#if 0
// 	#define SIP_LOGD(x)	SIP_LOGD x
// 	#define SIP_LOGI		SIP_LOGI
// 	#define SIP_LOGW		SIP_LOGW
// 	#define SIP_LOGE		SIP_LOGE
// 	#define SIP_LOGF		SIP_LOGF
// 	#else
// 	#define SIP_LOGD(x)	printf
// 	#define SIP_LOGI		printf
// 	#define SIP_LOGW		printf
// 	#define SIP_LOGE		printf
// 	#define SIP_LOGF		printf
// 	#endif

// #endif

// #endif //SIP_CONFIG_ANDROID

// #define SIP_EVUTIL_ASSERT(cond)						\
// 	do {								\
// 		if (!(cond)) {				\
// 			SIP_LOGE(							\
// 				"%s:%d: Assertion %s failed in %s",		\
// 				__FILE__,__LINE__,#cond,__func__);		\
// 			/* In case a user-supplied handler tries to */	\
// 			/* return control to us, log and abort here. */	\
// 			(void)fprintf(stderr,				\
// 					"%s:%d: Assertion %s failed in %s",		\
// 					__FILE__,__LINE__,#cond,__func__);		\
// 			abort();					\
// 		}							\
// 	} while (0)

// #ifdef _DEBUG
// #define SIP_MEMCPY1(to, from, len)			\
// 	do {						\
// 		cnt_memcpy++;					\
// 		memcpy(to, from, len);				\
// 	} while (0)

// #define SIP_MEMMOVE1(to, from, len) 			\
// 	do {						\
// 		cnt_memmove++;					\
// 		memmove(to, from, len);				\
// 	} while (0)
// #else
//   #define SIP_MEMCPY1		memcpy
//   #define SIP_MEMMOVE1		memmove
// #endif // _DEBUG

#endif //SIP_TYPEDEF_H
