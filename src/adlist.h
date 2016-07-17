/* adlist.h - A generic doubly linked list implementation
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

#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structures used currently. */
/**
 * 双端链表节点
 */
typedef struct listNode {
	// 前置节点
    struct listNode *prev;
	// 后置节点
    struct listNode *next;
	// 节点的值
    void *value;
} listNode;

/**
 * 双端链表迭代器
 */
typedef struct listIter {
	// 当前迭代到的节点
    listNode *next;
    // 迭代的方向
    int direction;
} listIter;

/**
 * 双端链表结构
 */
typedef struct list {
	// 表头节点
    listNode *head;
	// 表尾节点
    listNode *tail;
	// 节点值复制函数
    void *(*dup)(void *ptr);
	// 节点值释放函数
    void (*free)(void *ptr);
	// 节点值对比函数
    int (*match)(void *ptr, void *key);
	// 链表所包含的节点数量
    unsigned long len;
} list;

/* Functions implemented as macros */
/**
 * 作用：返回链表的长度(包含了多少个节点)
 * 时间复杂度：链表长度可以通过链表的len属性直接获得，O(1)
 * 返回给定链表所包含的节点数量
 * T = O(1)
 */
#define listLength(l) ((l)->len)
/**
 * 作用：返回链表的表头节点
 * 时间复杂度：表头节点可以通过链表的head属性直接获得，O(1)
 * 返回给定链表的表头节点
 * T = O(1)
 */
#define listFirst(l) ((l)->head)
/**
 * 作用：返回链表的表尾节点
 * 时间复杂度：表尾节点可以通过链表的tail属性直接获得，O(1)
 * 返回给定链表的表尾节点
 * T = O(1)
 */
#define listLast(l) ((l)->tail)
/**
 * 作用：返回给定节点的前置节点
 * 时间复杂度：前置节点可以用过节点的prev属性直接获得，O(1)
 * 返回给定节点的前置节点
 * T = O(1)
 */
#define listPrevNode(n) ((n)->prev)
/**
 * 作用：返回给定节点的后置节点
 * 时间复杂度：后置节点可以通过节点的next属性直接获得，O(1)
 * 返回给定节点的后置节点
 * T = O(1)
 */
#define listNextNode(n) ((n)->next)
/**
 * 作用：返回给定节点目前正在保存的值
 * 时间复杂度：节点值可以通过节点的value属性直接获得，O(1)
 * 返回给定节点的值
 * T = O(1)
 */
#define listNodeValue(n) ((n)->value)

/**
 * 作用：将给定的函数设置为链表的节点值复制函数
 * 时间复杂度：复制函数可以通过链表dup属性直接获得，O(1)
 * 将链表l的值复制函数设置为m
 * T = O(1)
 */
#define listSetDupMethod(l,m) ((l)->dup = (m))
/**
 * 作用：将给定的函数设置为链表的节点值释放函数
 * 时间复杂度：释放函数可以通过链表的free属性直接获得，O(1)
 * 将链表l的值释放函数设置为m
 * T = O(1)
 */
#define listSetFreeMethod(l,m) ((l)->free = (m))
/**
 * 作用：将给定的函数设置为链表的节点值对比函数
 * 时间复杂度：对比函数可以通过链表的match属性直接获得，O(1)
 * 将链表的对比函数设置为m
 * T = O(1)
 */
#define listSetMatchMethod(l,m) ((l)->match = (m))

/**
 * 作用：返回链表当前正在使用的节点值复制函数
 * 时间复杂度：O(1)
 * 返回给定链表的值复制函数
 * T = O(1)
 */
#define listGetDupMethod(l) ((l)->dup)
/**
 * 作用：返回链表当前正在使用的节点值释放函数
 * 时间复杂度：O(1)
 * 返回给定链表的值释放函数
 * T = O(1)
 */
#define listGetFree(l) ((l)->free)
/**
 * 作用：返回链表当前正在使用的节点值对比函数
 * 时间复杂度：O(1)
 * 返回给定链表的值对比函数
 */
#define listGetMatchMethod(l) ((l)->match)

/* Prototypes */
/**
 * 作用：创建一个不包含任何节点的新链表
 * 时间复杂度：O(1)
 */
list *listCreate(void);
/**
 * 作用：释放给定链表，以及链表中的所有节点
 * 时间复杂度：O(N)，N为链表长度
 */
void listRelease(list *list);
/**
 * 作用：将一个包含给定值的新结点添加到给定链表的表头
 * 时间复杂度：O(1)
 */
list *listAddNodeHead(list *list, void *value);
/**
 * 作用：将一个包含给定值的新结点添加到给定链表的表尾
 * 时间复杂度：O(1)
 */
list *listAddNodeTail(list *list, void *value);
/**
 * 作用：将一个包含给定值的新结点添加到给定节点的之前或者之后
 * 时间复杂度：O(1)
 */
list *listInsertNode(list *list, listNode *old_node, void *value, int after);
/**
 * 作用：从链表中删除给定节点
 * 时间复杂度：O(N)，N为链表长度
 */
void listDelNode(list *list, listNode *node);
listIter *listGetIterator(list *list, int direction);
listNode *listNext(listIter *iter);
void listReleaseIterator(listIter *iter);
/**
 * 作用：复制一个给定链表的副本
 * 时间复杂度：O(N)，N为链表长度
 */
list *listDup(list *orig);
/**
 * 作用：查找并返回链表中包含给定值的节点
 * 时间复杂度：O(N)，N为链表长度
 */
listNode *listSearchKey(list *list, void *key);
/**
 * 作用：返回链表在给定索引上的节点
 * 时间复杂度：O(N)，N为链表长度
 */
listNode *listIndex(list *list, long index);
void listRewind(list *list, listIter *li);
void listRewindTail(list *list, listIter *li);
/**
 * 作用：将链表的表尾节点弹出，然后将被弹出的节点插入到链表的表头，成为新的表头节点
 * 时间复杂度：O(1)
 */
void listRotate(list *list);

/* Directions for iterators
 * 迭代器进行迭代的方向
 * */
// 从表头向表尾进行迭代
#define AL_START_HEAD 0
// 从表尾向表头进行迭代
#define AL_START_TAIL 1

#endif /* __ADLIST_H__ */
