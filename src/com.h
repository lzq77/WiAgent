/*
 * hostapd / Initialization and configuration
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */
#ifndef COM_H
#define COM_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <asm/byteorder.h>
#include <stdarg.h>

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

//#define STA_HASH(sta) (sta[5])
#define os_memcmp(s1, s2, n) memcmp((s1), (s2), (n))
/* Define platform specific integer types */
#define ETH_ALEN 6
#define NUM_TX_QUEUES 4
#define NUM_WEP_KEYS 4
#define STA_HASH_SIZE 256
#define STA_HASH(sta) (sta[5])
#define IFNAMSIZ 16


//移位操作
#ifndef BIT
#define BIT(x) (1 << (x))
#endif

//输出操作

#define MSG_ERROR stderr
#define MSG_INFO stderr
#define MSG_DEBUG stderr
#define MSG_MSGDUMP stderr
#define MSG_EXCESSIVE stderr

#define wpa_printf fprintf

//大小端转化问题
#define host_to_le16 cpu_to_le16
#define le_to_host16 le16_to_cpu

#ifdef _MSC_VER
typedef UINT64 u64;
typedef UINT32 u32;
typedef UINT16 u16;
typedef UINT8 u8;
typedef INT64 s64;
typedef INT32 s32;
typedef INT16 s16;
typedef INT8 s8;
#define WPA_TYPES_DEFINED
#endif /* _MSC_VER */

#ifdef __vxworks
typedef unsigned long long u64;
typedef UINT32 u32;
typedef UINT16 u16;
typedef UINT8 u8;
typedef long long s64;
typedef INT32 s32;
typedef INT16 s16;
typedef INT8 s8;
#define WPA_TYPES_DEFINED
#endif /* __vxworks */

#ifdef CONFIG_TI_COMPILER
#ifdef _LLONG_AVAILABLE
typedef unsigned long long u64;
#else
/*
 * TODO: 64-bit variable not available. Using long as a workaround to test the
 * build, but this will likely not work for all operations.
 */
typedef unsigned long u64;
#endif
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
#define WPA_TYPES_DEFINED
#endif /* CONFIG_TI_COMPILER */

#ifndef WPA_TYPES_DEFINED
#ifdef CONFIG_USE_INTTYPES_H
#include <inttypes.h>
#else
#include <stdint.h>
#endif
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;

typedef long os_time_t;
typedef int (*nl_recv_callback)(struct nl_msg *msg, void *arg);//netlink receive callback function
struct os_time {
	os_time_t sec;
	os_time_t usec;
};

struct os_reltime {
	os_time_t sec;
	os_time_t usec;
};
#define WPA_TYPES_DEFINED
#endif /* !WPA_TYPES_DEFINED */

//对齐使用
#ifdef __GNUC__
#define PRINTF_FORMAT(a,b) __attribute__ ((format (printf, (a), (b))))
#define STRUCT_PACKED __attribute__ ((packed))
#else
#define PRINTF_FORMAT(a,b)
#define STRUCT_PACKED
#endif

#ifdef __CHECKER__
#define __force __attribute__((force))
#define __bitwise __attribute__((bitwise))
#else
#define __force
#define __bitwise
#endif
typedef u16 __bitwise be16;
typedef u16 __bitwise le16;
typedef u32 __bitwise be32;
typedef u32 __bitwise le32;
typedef u64 __bitwise be64;
typedef u64 __bitwise le64;

//日志等级
enum hostapd_logger_level {
	HOSTAPD_LEVEL_DEBUG_VERBOSE = 0,
	HOSTAPD_LEVEL_DEBUG = 1,
	HOSTAPD_LEVEL_INFO = 2,
	HOSTAPD_LEVEL_NOTICE = 3,
	HOSTAPD_LEVEL_WARNING = 4
};

void * os_memset(void *s, int c, size_t n);

void * os_realloc(void *ptr, size_t size);

 void * os_malloc(size_t size);

void * os_zalloc(size_t size);

void * os_calloc(size_t nmemb, size_t size);

int os_memcmp_back(const void *s1, const void *s2, size_t n);

void * os_memcpy(void *dest, const void *src, size_t n);

size_t os_strlcpy(char *dest, const char *src, size_t siz);
 
int os_strncmp(const void *s11,const void *s22, size_t n);

int os_strcmp(const char *s1, const char *s2);

size_t os_strlen(const char *s);

char * os_strdup(const char *s);

char * os_strchr(const char *s, int c);

char * os_strrchr(const char *s, int c);

char * os_strstr(const char *haystack, const char *needle);

void * os_memmove(void *dest, const void *src, size_t n);

size_t merge_byte_arrays(u8 *res, size_t res_len,
         const u8 *src1, size_t src1_len,
         const u8 *src2, size_t src2_len);

void os_remove_in_array(void *ptr, size_t nmemb, size_t size,
				      size_t idx);
void os_free(void *ptr);

//输出的操作
void * os_realloc_array(void *ptr, size_t nmemb, size_t size);

int os_snprintf(char *str, size_t size, const char *format, ...);

#endif
