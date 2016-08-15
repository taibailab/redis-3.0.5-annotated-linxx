/* Hash Tables Implementation.
 *
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by
 * chaining. See the source code for more information... :)
 *
 * 这个文件实现了一个内存哈希表，它支持插入、删除、替换和获取随机元素等操作。
 *
 * 哈希表会自动在表的大小和二次方之间进行调整。
 *
 * 键的冲突通过链表来解决。
 *
 * Copyright (c) 2006-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

/*
 *字典的操作状态
 */
// 操作成功
#define DICT_OK 0
// 操作失败(或出错)
#define DICT_ERR 1

/* Unused arguments generate annoying warnings... */
// 如果字典的私有数据不使用时
// 用这个宏来避免编译器错误
#define DICT_NOTUSED(V) ((void) V)

// 哈希表节点
typedef struct dictEntry {
	// 键
    void *key;
    // 值(可以是一个指针，或者是一个uint64_t整数，又或者是一个int64_t整数，或者是一个double双精度浮点数)
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
        double d;
    } v;
    // 指向下个哈希表节点，形成链表(可以将多个哈希值相同的键值对连接在一起，以此来解决键冲突(collision)的问题)
    struct dictEntry *next;
} dictEntry;

/*
 * 字典类型特定函数
 */
typedef struct dictType {
	// 计算哈希值的函数
    unsigned int (*hashFunction)(const void *key);
    // 复制键的函数
    void *(*keyDup)(void *privdata, const void *key);
    // 复制值的函数
    void *(*valDup)(void *privdata, const void *obj);
    // 对比键的函数
    int (*keyCompare)(void *privdata, const void *key1, const void *key2);
    // 销毁键的函数
    void (*keyDestructor)(void *privdata, void *key);
    // 销毁值的函数
    void (*valDestructor)(void *privdata, void *obj);
} dictType;

/* This is our hash table structure. Every dictionary has two of this as we
 * implement incremental rehashing, for the old to the new table. */
/*
 * 哈希表
 * 每个字典都使用两个哈希表，从而实现渐进式rehash。
 */
typedef struct dictht {
	// 哈希表数组
    dictEntry **table;
    // 哈希表大小
    unsigned long size;
    // 哈希表大小掩码，用于计算索引值
    // 总是等于size-1(这个属性和哈希值一起决定一个键应该被放到table数组的哪个索引上面)
    unsigned long sizemask;
    // 该哈希表已有节点的数量
    unsigned long used;
} dictht;

// 字典
typedef struct dict {
	/*
	 * 类型特定函数(针对不同类型的键值对，为创建多态字典而设置的；type属性是一个指向dictType结构的指针，每个dictType结构保存了一簇用于操作特定类型键值对的函数，
	 * Redis会为用途不同的字典设置不同的类型特定函数。)
	 */
    dictType *type;
    // 私有数据(针对不同类型的键值对，为创建多态字典而设置的；privdata属性保存需要传给哪些类型特定函数的可选参数。)
    void *privdata;
    // 哈希表(包含两个项的数组，数组中的每个项都是一个dictht哈希表，一般情况下，字典只使用ht[0]哈希表，ht[1]哈希表只会在对ht[0]哈希表进行rehash时使用。)
    dictht ht[2];
    // rehash索引
    // 当rehash不在进行时，值为-1。(记录rehash目前的进度，如果目前没有在进行rehash，那么它的值为-1。)
    long rehashidx; /* rehashing not in progress if rehashidx == -1 */
    // 当前正在运行的安全迭代器的数量
    int iterators; /* number of iterators currently running */
} dict;

/* If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext()
 * should be called while iterating. */
/*
 * 字典迭代器
 *
 * 如果safe属性的值为1，那么在迭代进行的过程中，程序仍然可以进行dictAdd、dictFind和其他函数，对字典进行修改。
 *
 * 如果safe不为1，那么程序只会调用dictNext对字典进行迭代，而不对字典进行修改。
 */
typedef struct dictIterator {
	// 被迭代的字典
    dict *d;
    // index: 迭代器当前所指向的哈希表索引位置。
    long index;
    // table: 正在被迭代的哈希表号码，值可以是0或1。
    // safe: 标识这个迭代器是否安全
    int table, safe;
    // entry: 当前迭代到的节点的指针
    // nextEntry: 当前迭代节点的下一个节点。
    // 				因为在安全迭代器运作时，entry所指向的节点可能会被修改，所以需要一个额外的指针来保存下一节点的位置，从而防止指针丢失
    dictEntry *entry, *nextEntry;
    /* unsafe iterator fingerprint for misuse detection. */
    long long fingerprint;
} dictIterator;

typedef void (dictScanFunction)(void *privdata, const dictEntry *de);

/* This is the initial size of every hash table */
/*
 * 哈希表的初始大小
 */
#define DICT_HT_INITIAL_SIZE     4

/* ------------------------------- Macros ------------------------------------*/
// 释放给定字典节点的值
#define dictFreeVal(d, entry) \
    if ((d)->type->valDestructor) \
        (d)->type->valDestructor((d)->privdata, (entry)->v.val)

// 设置给定字典节点的值
#define dictSetVal(d, entry, _val_) do { \
    if ((d)->type->valDup) \
        entry->v.val = (d)->type->valDup((d)->privdata, _val_); \
    else \
        entry->v.val = (_val_); \
} while(0)

// 将一个有序号整数设为节点的值
#define dictSetSignedIntegerVal(entry, _val_) \
    do { entry->v.s64 = _val_; } while(0)

// 将一个无符号整数设为节点的值
#define dictSetUnsignedIntegerVal(entry, _val_) \
    do { entry->v.u64 = _val_; } while(0)

#define dictSetDoubleVal(entry, _val_) \
    do { entry->v.d = _val_; } while(0)

// 释放给定字典节点的键
#define dictFreeKey(d, entry) \
    if ((d)->type->keyDestructor) \
        (d)->type->keyDestructor((d)->privdata, (entry)->key)

// 设置给定字典节点的键
#define dictSetKey(d, entry, _key_) do { \
    if ((d)->type->keyDup) \
        entry->key = (d)->type->keyDup((d)->privdata, _key_); \
    else \
        entry->key = (_key_); \
} while(0)

// 对比两个键
#define dictCompareKeys(d, key1, key2) \
    (((d)->type->keyCompare) ? \
        (d)->type->keyCompare((d)->privdata, key1, key2) : \
        (key1) == (key2))

// 计算给定键的哈希值
#define dictHashKey(d, key) (d)->type->hashFunction(key)
// 返回获取给定节点的键
#define dictGetKey(he) ((he)->key)
// 返回获取给定节点的值
#define dictGetVal(he) ((he)->v.val)
// 返回获取给定节点的有符号整数值
#define dictGetSignedIntegerVal(he) ((he)->v.s64)
// 返回给定节点的无符号整数值
#define dictGetUnsignedIntegerVal(he) ((he)->v.u64)
#define dictGetDoubleVal(he) ((he)->v.d)
// 返回给定字典的大小
#define dictSlots(d) ((d)->ht[0].size+(d)->ht[1].size)
// 返回字典的已有节点数量
#define dictSize(d) ((d)->ht[0].used+(d)->ht[1].used)
// 查看字典是否正在rehash
#define dictIsRehashing(d) ((d)->rehashidx != -1)

/* API */
/*
 * 作用：创建一个新的字典
 * 时间复杂度：O(1)
 */
dict *dictCreate(dictType *type, void *privDataPtr);
int dictExpand(dict *d, unsigned long size);
/*
 * 作用：将给定的键值对添加到字典里面
 * 时间复杂度：O(1)
 */
int dictAdd(dict *d, void *key, void *val);
dictEntry *dictAddRaw(dict *d, void *key);
/*
 * 作用：将给定的键值对添加到字典里面，如果键已经存在于字典，那么用新值取代原有的值
 * 时间复杂度：O(1)
 */
int dictReplace(dict *d, void *key, void *val);
dictEntry *dictReplaceRaw(dict *d, void *key);
/*
 * 作用：从字典中删除给定键所对应的键值对
 * 时间复杂度：O(1)
 */
int dictDelete(dict *d, const void *key);
int dictDeleteNoFree(dict *d, const void *key);
/*
 * 作用：释放给定字典，以及字典中包含的所有键值对
 * 时间复杂度：O(N)，N为字典包含的键值对数量
 */
void dictRelease(dict *d);
dictEntry * dictFind(dict *d, const void *key);
/*
 * 作用：返回给定键的值
 * 时间复杂度：O(1)
 */
void *dictFetchValue(dict *d, const void *key);
int dictResize(dict *d);
dictIterator *dictGetIterator(dict *d);
dictIterator *dictGetSafeIterator(dict *d);
dictEntry *dictNext(dictIterator *iter);
void dictReleaseIterator(dictIterator *iter);
/*
 * 作用：从字典中随机返回一个键值对
 * 时间复杂度：O(1)
 */
dictEntry *dictGetRandomKey(dict *d);
unsigned int dictGetSomeKeys(dict *d, dictEntry **des, unsigned int count);
void dictPrintStats(dict *d);
unsigned int dictGenHashFunction(const void *key, int len);
unsigned int dictGenCaseHashFunction(const unsigned char *buf, int len);
void dictEmpty(dict *d, void(callback)(void*));
void dictEnableResize(void);
void dictDisableResize(void);
int dictRehash(dict *d, int n);
int dictRehashMilliseconds(dict *d, int ms);
void dictSetHashFunctionSeed(unsigned int initval);
unsigned int dictGetHashFunctionSeed(void);
unsigned long dictScan(dict *d, unsigned long v, dictScanFunction *fn, void *privdata);

/* Hash table types */
extern dictType dictTypeHeapStringCopyKey;
extern dictType dictTypeHeapStrings;
extern dictType dictTypeHeapStringCopyKeyValue;

#endif /* __DICT_H */
