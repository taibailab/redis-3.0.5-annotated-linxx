/* SDSLib, A C dynamic strings library
 *
 * Copyright (c) 2006-2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SDS_H
#define __SDS_H

// 最大预分配长度
#define SDS_MAX_PREALLOC (1024*1024)

#include <sys/types.h>
#include <stdarg.h>

// 类型别名，用于指向sdshdr的buf属性
typedef char *sds;

/**
 * 每个sds.h/sdshdr结构表示一个SDS(Simple Dynamic String简单动态字符串)值
 * 保存字符串对象的结构
 */ 
struct sdshdr {
	/*
	 * 记录buf数组中已使用字节的数量
	 * 等于SDS所保存字符串的长度
	 * buf中已占用空间的长度
	 */
    unsigned int len;

    // 记录buf数组中未使用字节的数量
	// buf中剩余可用空间的长度
    unsigned int free;

    // 字节数组，用于保存字符串
	// 数据空间
    char buf[];
};

/**
 * 作用：返回SDS的已使用空间字节数
 * 时间复杂度：这个值可以通过读取SDS的len属性来直接获得，复杂度为O(1)
 * 返回sds实际保存的字符串的长度
 * T = O(1)
 */
static inline size_t sdslen(const sds s) {
    struct sdshdr *sh = (void*)(s-(sizeof(struct sdshdr)));
    return sh->len;
}

/**
 * 作用：返回SDS的未使用空间字节数
 * 时间复杂度：这个值可以通过读取SDS的free属性来直接获得，复杂度为O(1)
 * 返回sds可用空间的长度
 * T = O(1)
 */
static inline size_t sdsavail(const sds s) {
    struct sdshdr *sh = (void*)(s-(sizeof(struct sdshdr)));
    return sh->free;
}

sds sdsnewlen(const void *init, size_t initlen);
/**
 * 作用：创建一个包含给定C字符串的SDS
 * 时间复杂度：O(N)，N为给定C字符串的长度
 */
sds sdsnew(const char *init);
/**
 * 作用：创建一个不包含任何内容的空SDS
 * 时间复杂度：O(1)
 */
sds sdsempty(void);
size_t sdslen(const sds s);
/**
 * 作用：创建一个给定SDS的副本(copy)
 * 时间复杂度：O(N)，N为给定SDS的长度
 */
sds sdsdup(const sds s);
/**
 * 作用：释放给定的SDS
 * 时间复杂度：O(N)，N为被释放SDS的长度
 */
void sdsfree(sds s);
size_t sdsavail(const sds s);
/**
 * 作用：用空字符将SDS扩展至给定长度
 * 时间复杂度：O(N)，N为扩展新增的字节数
 */
sds sdsgrowzero(sds s, size_t len);
sds sdscatlen(sds s, const void *t, size_t len);
/**
 * 作用：将给定C字符串拼接到SDS字符串的末尾
 * 时间复杂度：O(N)，N为被拼接C字符串的长度
 */
sds sdscat(sds s, const char *t);
/**
 * 作用：将给定SDS字符串拼接到另一个SDS字符串的末尾
 * 时间复杂度：O(N)，N为被拼接SDS字符串的长度
 */
sds sdscatsds(sds s, const sds t);
sds sdscpylen(sds s, const char *t, size_t len);
/**
 * 作用：将给定的C字符串复制到SDS里面，覆盖SDS原有的字符串
 * 时间复杂度：O(N)，N为被复制C字符串的长度
 */
sds sdscpy(sds s, const char *t);

sds sdscatvprintf(sds s, const char *fmt, va_list ap);
#ifdef __GNUC__
sds sdscatprintf(sds s, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
#else
sds sdscatprintf(sds s, const char *fmt, ...);
#endif

sds sdscatfmt(sds s, char const *fmt, ...);
/**
 * 作用：接受一个SDS和一个C字符串作为参数，从SDS中移除所有在C字符串中出现过的字符
 * 时间复杂度：O(N^2)，N为给定C字符串的长度
 */
sds sdstrim(sds s, const char *cset);
/**
 * 作用：保留SDS给定区间内的数据，不在区间内的数据会被覆盖或清除
 * 时间复杂度：O(N)，N为被保留数据的字节数
 */
void sdsrange(sds s, int start, int end);
void sdsupdatelen(sds s);
/**
 * 作用：清空SDS保存的字符串内容
 * 时间复杂度：因为惰性空间释放策略，复杂度为O(1)
 */
void sdsclear(sds s);
/**
 * 作用：对比两个SDS字符串是否相同
 * 时间复杂度：O(N)，N为两个SDS中较短的那个SDS的长度
 */
int sdscmp(const sds s1, const sds s2);
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count);
void sdsfreesplitres(sds *tokens, int count);
void sdstolower(sds s);
void sdstoupper(sds s);
sds sdsfromlonglong(long long value);
sds sdscatrepr(sds s, const char *p, size_t len);
sds *sdssplitargs(const char *line, int *argc);
sds sdsmapchars(sds s, const char *from, const char *to, size_t setlen);
sds sdsjoin(char **argv, int argc, char *sep);

/* Low level functions exposed to the user API */
sds sdsMakeRoomFor(sds s, size_t addlen);
void sdsIncrLen(sds s, int incr);
sds sdsRemoveFreeSpace(sds s);
size_t sdsAllocSize(sds s);

#endif
