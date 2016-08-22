/* The ziplist is a specially encoded dually linked list that is designed
 * to be very memory efficient.
 *
 * Ziplist是为了尽可能地节约内存而设计的特殊编码双端链表。
 *
 * It stores both strings and integer values,
 * where integers are encoded as actual integers instead of a series of
 * characters.
 *
 * Ziplist可以存储字符串值和整数值，其中，整数值被保存为实际的整数，而不是字符数组。
 *
 * It allows push and pop operations on either side of the list
 * in O(1) time. However, because every operation requires a reallocation of
 * the memory used by the ziplist, the actual complexity is related to the
 * amount of memory used by the ziplist.
 *
 * Ziplist允许在列表的两端进行O(1)复杂度的push和pop操作。
 * 但是，因为这些操作都需要对整个ziplist进行内存重分配，所以实际的复杂度和ziplist占用的内存大小有关。
 *
 * ----------------------------------------------------------------------------
 *
 * ZIPLIST OVERALL LAYOUT:
 * Ziplist的整体布局：
 *
 * The general layout of the ziplist is as follows:
 * 以下是ziplist的一般布局：
 *
 * <zlbytes><zltail><zllen><entry><entry><zlend>
 *
 * <zlbytes> is an unsigned integer to hold the number of bytes that the
 * ziplist occupies. This value needs to be stored to be able to resize the
 * entire structure without the need to traverse it first.
 * <zlbytes>是一个无符号整数，保存着ziplist使用的内存数量。通过这个值，程序可以直接对ziplist的内存大小进行调整，而无需为了计算ziplist的内存大小而遍历整个列表
 * 。(类型：uint32_t。长度：4字节。用途：记录整个压缩列表占用的内存字节数：在对压缩列表进行内存重分配，或者计算zlend的位置时使用。)
 *
 * <zltail> is the offset to the last entry in the list. This allows a pop
 * operation on the far side of the list without the need for full traversal.
 * <zltail>保存着到达列表中最后一个节点的偏移量。这个偏移量使得对表尾的pop操作可以在无须遍历整个列表的情况下进行。(类型：uint32_t。长度：4字节。记录压缩列表表
 * 尾节点举例压缩列表的起始位置有多少个字节：通过这个偏移量，程序无须遍历整个压缩列表就可以确定表尾节点的地址)
 *
 * <zllen> is the number of entries.When this value is larger than 2**16-2,
 * we need to traverse the entire list to know how many items it holds.
 * <zllen>保存着列表中的节点数量。当zllen保存的值大于2**16-2时，程序需要遍历整个列表才能知道列表实际包含多少个节点。(类型：uint16_t。长度：2字节。用途：记录
 * 了压缩列表包含的节点数量：当这个属性的值小于UINT16_MAX(65535)时，这个属性的值就是压缩列表包含节点的数量；当这个值等于UINT16_MAX时，节点的真实数量需要遍历
 * 整个压缩列表才能计算得出。)
 *
 * <zlend> is a single byte special value, equal to 255, which indicates the
 * end of the list.
 * <zlend>的长度为1字节，值为255，标识列表的末尾。(类型：uint8_t。长度：1字节。用途：特殊值0xFF(十进制255)，用于标记压缩列表的末端。)
 *
 * ZIPLIST ENTRIES:
 * ZIPLIST节点：
 *
 * Every entry in the ziplist is prefixed by a header that contains two pieces
 * of information. First, the length of the previous entry is stored to be
 * able to traverse the list from back to front. Second, the encoding with an
 * optional string length of the entry itself is stored.
 *
 * 每个ziplist节点的前面都带有一个header，这个header包含两部分信息：
 * 1).前置节点的长度，在程序从后向前遍历时使用。
 * 2).当前节点所保存的值的类型和长度。
 *
 * The length of the previous entry is encoded in the following way:
 * If this length is smaller than 254 bytes, it will only consume a single
 * byte that takes the length as value. When the length is greater than or
 * equal to 254, it will consume 5 bytes. The first byte is set to 254 to
 * indicate a larger value is following. The remaining 4 bytes take the
 * length of the previous entry as value.
 *
 * 编码前置节点的长度的方法如下：
 * 1).如果前置节点的长度小于254字节，那么程序将使用1个字节来保存这个长度值。
 * 2).如果前置节点的长度大于等于254字节，那么程序将使用5个字节来保存这个长度值：
 * 	a).第1个字节的值将被设为254，用于标识这是一个5字节的长度值。
 * 	b).之后的4个字节则用于保存前置节点的实际长度。
 *
 * The other header field of the entry itself depends on the contents of the
 * entry. When the entry is a string, the first 2 bits of this header will hold
 * the type of encoding used to store the length of the string, followed by the
 * actual length of the string. When the entry is an integer the first 2 bits
 * are both set to 1. The following 2 bits are used to specify what kind of
 * integer will be stored after this header. An overview of the different
 * types and encodings is as follows:
 *
 * header另一部分的内容和节点所保存的值有关。
 * 1).如果节点保存的是字符串值，那么这部分header的头2个位将保存编码字符串长度所使用的类型，而之后跟着的内容则是字符串的实际长度。
 *
 * |00pppppp| - 1 byte
 *      String value with length less than or equal to 63 bytes (6 bits).
 *      字符串的长度小于或等于63字节。
 * |01pppppp|qqqqqqqq| - 2 bytes
 *      String value with length less than or equal to 16383 bytes (14 bits).
 *      字符串的长度小于或等于16383字节。
 * |10______|qqqqqqqq|rrrrrrrr|ssssssss|tttttttt| - 5 bytes
 *      String value with length greater than or equal to 16384 bytes.
 *      字符串的长度大于或等于16384字节。
 *
 * 2).如果节点保存的是整数值，那么这部分header的头2位都将被设置为1，而之后跟着的2位则用于标识节点所保存的整数的类型。
 *
 * |11000000| - 1 byte
 *      Integer encoded as int16_t (2 bytes).
 *      节点的值为int16_t类型的整数，长度为2字节。
 * |11010000| - 1 byte
 *      Integer encoded as int32_t (4 bytes).
 *      节点的值为int32_t类型的整数，长度为4字节。
 * |11100000| - 1 byte
 *      Integer encoded as int64_t (8 bytes).
 *      节点的值为int64_t类型的整数，长度为8字节。
 * |11110000| - 1 byte
 *      Integer encoded as 24 bit signed (3 bytes).
 *      节点的值为24位(3字节)长的整数。
 * |11111110| - 1 byte
 *      Integer encoded as 8 bit signed (1 byte).
 *      节点的值为8位(1字节)长的整数。
 * |1111xxxx| - (with xxxx between 0000 and 1101) immediate 4 bit integer.
 *      Unsigned integer from 0 to 12. The encoded value is actually from
 *      1 to 13 because 0000 and 1111 can not be used, so 1 should be
 *      subtracted from the encoded 4 bit value to obtain the right value.
 *      节点的值为介于0至12之间的无符号整数。
 *      因为0000和1111都不能使用，所以位的实际值将是1至13。
 *      程序在取得这4个位的值之后，还需要减去1，才能计算出正确的值。
 *      比如说，如果位的值为0001 = 1，那么程序返回的值将是1 - 1 = 0。
 * |11111111| - End of ziplist.
 * 		ziplist的结尾标识
 *
 * All the integers are represented in little endian byte order.
 *
 * 所有整数都表示为小端字节序。
 *
 * ----------------------------------------------------------------------------
 *
 * Copyright (c) 2009-2012, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include "zmalloc.h"
#include "util.h"
#include "ziplist.h"
#include "endianconv.h"
#include "redisassert.h"

/*
 * ziplist末端标识符，以及5字节长长度标识符
 */
#define ZIP_END 255
#define ZIP_BIGLEN 254

/* Different encoding/length possibilities */
/*
 * 字符串编码和整数编码的掩码
 */
#define ZIP_STR_MASK 0xc0
#define ZIP_INT_MASK 0x30
/*
 * 字符串编码类型
 */
#define ZIP_STR_06B (0 << 6)
#define ZIP_STR_14B (1 << 6)
#define ZIP_STR_32B (2 << 6)
/*
 * 整数编码类型
 */
#define ZIP_INT_16B (0xc0 | 0<<4)
#define ZIP_INT_32B (0xc0 | 1<<4)
#define ZIP_INT_64B (0xc0 | 2<<4)
#define ZIP_INT_24B (0xc0 | 3<<4)
#define ZIP_INT_8B 0xfe
/* 4 bit integer immediate encoding
 *
 * 4位整数编码的掩码和类型
 */
#define ZIP_INT_IMM_MASK 0x0f
#define ZIP_INT_IMM_MIN 0xf1    /* 11110001 */
#define ZIP_INT_IMM_MAX 0xfd    /* 11111101 */
#define ZIP_INT_IMM_VAL(v) (v & ZIP_INT_IMM_MASK)

/*
 * 24位整数的最大值和最小值
 */
#define INT24_MAX 0x7fffff
#define INT24_MIN (-INT24_MAX - 1)

/* Macro to determine type
 *
 * 查看给定编码enc是否为字符串编码
 */
#define ZIP_IS_STR(enc) (((enc) & ZIP_STR_MASK) < ZIP_STR_MASK)

/* Utility macros */
/*
 * ziplist属性宏
 */
// 定位到ziplist的bytes属性，该属性记录了整个ziplist所占用的内存字节数。用于取出bytes属性的现有值，或者为bytes属性赋予新值。
#define ZIPLIST_BYTES(zl)       (*((uint32_t*)(zl)))
// 定位到ziplist的offset属性，该属性记录了到达表尾节点的偏移量。用于取出offset属性的现有值，或者为offset属性赋予新值。
#define ZIPLIST_TAIL_OFFSET(zl) (*((uint32_t*)((zl)+sizeof(uint32_t))))
// 定位到ziplist的length属性，该属性记录了ziplist包含的节点数量。用于取出length属性的现有值，或者为length属性赋予新值。
#define ZIPLIST_LENGTH(zl)      (*((uint16_t*)((zl)+sizeof(uint32_t)*2)))
// 返回ziplist表头的大小
#define ZIPLIST_HEADER_SIZE     (sizeof(uint32_t)*2+sizeof(uint16_t))
// 返回指向ziplist第一个节点(的起始位置)的指针
#define ZIPLIST_ENTRY_HEAD(zl)  ((zl)+ZIPLIST_HEADER_SIZE)
// 返回指向ziplist最后一个节点(的起始位置)的指针
#define ZIPLIST_ENTRY_TAIL(zl)  ((zl)+intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl)))
// 返回指向ziplist末端ZIP_END(的起始位置)的指针
#define ZIPLIST_ENTRY_END(zl)   ((zl)+intrev32ifbe(ZIPLIST_BYTES(zl))-1)

/*
空白ziplist示例图
area        |<---- ziplist header ---->|<-- end -->|

size          4 bytes   4 bytes 2 bytes  1 byte
            +---------+--------+-------+-----------+
component   | zlbytes | zltail | zllen | zlend     |
            |         |        |       |           |
value       |  1011   |  1010  |   0   | 1111 1111 |
            +---------+--------+-------+-----------+
                                       ^
                                       |
                               ZIPLIST_ENTRY_HEAD
                                       &
address                        ZIPLIST_ENTRY_TAIL
                                       &
                               ZIPLIST_ENTRY_END

非空 ziplist 示例图
area        |<---- ziplist header ---->|<----------- entries ------------->|<-end->|

size          4 bytes  4 bytes  2 bytes    ?        ?        ?        ?     1 byte
            +---------+--------+-------+--------+--------+--------+--------+-------+
component   | zlbytes | zltail | zllen | entry1 | entry2 |  ...   | entryN | zlend |
            +---------+--------+-------+--------+--------+--------+--------+-------+
                                       ^                          ^        ^
address                                |                          |        |
                                ZIPLIST_ENTRY_HEAD                |   ZIPLIST_ENTRY_END
                                                                  |
                                                        ZIPLIST_ENTRY_TAIL
 */

/* We know a positive increment can only be 1 because entries can only be
 * pushed one at a time. */
/*
 * 增加ziplist的节点数
 *
 * T = O(1)
 */
#define ZIPLIST_INCR_LENGTH(zl,incr) { \
    if (ZIPLIST_LENGTH(zl) < UINT16_MAX) \
        ZIPLIST_LENGTH(zl) = intrev16ifbe(intrev16ifbe(ZIPLIST_LENGTH(zl))+incr); \
}

/*
 * 保存ziplist节点信息的结构(类型：列表节点。长度：不定。用途：压缩列表包含的各个节点，节点的长度由节点保存的内容决定，)
 */
typedef struct zlentry {
	// prevrawlensize: 编码prevrawlen所需的字节大小
	// prevrawlen: 前置节点的长度
    unsigned int prevrawlensize, prevrawlen;
    // lensize: 编码len所需的字节大小
    // len: 当前节点值的长度
    unsigned int lensize, len;
    // 当前节点header的大小。等于prevrawlensize + lensize。
    unsigned int headersize;
    // 当前节点值所使用的编码类型
    unsigned char encoding;
    // 指向当前节点的指针
    unsigned char *p;
} zlentry;

/* Extract the encoding from the byte pointed by 'ptr' and set it into
 * 'encoding'.
 *
 * 从ptr中取出节点值的编码类型，并将它保存到encoding变量中。
 *
 * T = O(1)
 */
#define ZIP_ENTRY_ENCODING(ptr, encoding) do {  \
    (encoding) = (ptr[0]); \
    if ((encoding) < ZIP_STR_MASK) (encoding) &= ZIP_STR_MASK; \
} while(0)

/* Return bytes needed to store integer encoded by 'encoding'
 *
 * 返回保存encoding编码的值所需的字节数量
 *
 * T = O(1)
 */
static unsigned int zipIntSize(unsigned char encoding) {
    switch(encoding) {
    case ZIP_INT_8B:  return 1;
    case ZIP_INT_16B: return 2;
    case ZIP_INT_24B: return 3;
    case ZIP_INT_32B: return 4;
    case ZIP_INT_64B: return 8;
    default: return 0; /* 4 bit immediate */
    }
    assert(NULL);
    return 0;
}

/* Encode the length 'rawlen' writing it in 'p'. If p is NULL it just returns
 * the amount of bytes required to encode such a length.
 *
 * 编码节点长度值1，并将它写入到p中，然后返回编码1所需的字节数量。
 *
 * 如果p为NULL，那么仅返回编码1所需的字节数量，不进行写入。
 *
 * T = O(1)
 */
static unsigned int zipEncodeLength(unsigned char *p, unsigned char encoding, unsigned int rawlen) {
    unsigned char len = 1, buf[5];

    // 编码字符串
    if (ZIP_IS_STR(encoding)) {
        /* Although encoding is given it may not be set for strings,
         * so we determine it here using the raw length. */
        if (rawlen <= 0x3f) {
            if (!p) return len;
            buf[0] = ZIP_STR_06B | rawlen;
        } else if (rawlen <= 0x3fff) {
            len += 1;
            if (!p) return len;
            buf[0] = ZIP_STR_14B | ((rawlen >> 8) & 0x3f);
            buf[1] = rawlen & 0xff;
        } else {
            len += 4;
            if (!p) return len;
            buf[0] = ZIP_STR_32B;
            buf[1] = (rawlen >> 24) & 0xff;
            buf[2] = (rawlen >> 16) & 0xff;
            buf[3] = (rawlen >> 8) & 0xff;
            buf[4] = rawlen & 0xff;
        }
    // 编码整数
    } else {
        /* Implies integer encoding, so length is always 1. */
        if (!p) return len;
        buf[0] = encoding;
    }

    /* Store this length at p */
    // 将编码后的长度写入p
    memcpy(p,buf,len);
    // 返回编码所需的字节数
    return len;
}

/* Decode the length encoded in 'ptr'. The 'encoding' variable will hold the
 * entries encoding, the 'lensize' variable will hold the number of bytes
 * required to encode the entries length, and the 'len' variable will hold the
 * entries length.
 *
 * 解码ptr指针，取出列表节点的相关信息，并将它们保存在以下变量中：
 * -encoding: 保存节点值的编码类型。
 * -lensize: 保存编码节点长度所需的字节数。
 * -len: 保存节点的长度。
 *
 * T = O(1)
 */
#define ZIP_DECODE_LENGTH(ptr, encoding, lensize, len) do {                    \
	/* 取出值的编码类型 */                                                       \
    ZIP_ENTRY_ENCODING((ptr), (encoding));                                     \
	/* 字符串编码 */                                                            \
    if ((encoding) < ZIP_STR_MASK) {                                           \
        if ((encoding) == ZIP_STR_06B) {                                       \
            (lensize) = 1;                                                     \
            (len) = (ptr)[0] & 0x3f;                                           \
        } else if ((encoding) == ZIP_STR_14B) {                                \
            (lensize) = 2;                                                     \
            (len) = (((ptr)[0] & 0x3f) << 8) | (ptr)[1];                       \
        } else if (encoding == ZIP_STR_32B) {                                  \
            (lensize) = 5;                                                     \
            (len) = ((ptr)[1] << 24) |                                         \
                    ((ptr)[2] << 16) |                                         \
                    ((ptr)[3] <<  8) |                                         \
                    ((ptr)[4]);                                                \
        } else {                                                               \
            assert(NULL);                                                      \
        }                                                                      \
	/* 整数编码 */                                                              \
    } else {                                                                   \
        (lensize) = 1;                                                         \
        (len) = zipIntSize(encoding);                                          \
    }                                                                          \
} while(0);

/* Encode the length of the previous entry and write it to "p". Return the
 * number of bytes needed to encode this length if "p" is NULL.
 *
 * 对前置节点的长度len进行编码，并将它写入到p中，然后返回编码len所需的字节数量。
 *
 * 如果p为NULL，那么不进行写入，仅返回编码len所需的字节数量。
 *
 * T = O(1)
 */
static unsigned int zipPrevEncodeLength(unsigned char *p, unsigned int len) {
	// 仅返回编码len所需的字节数量
    if (p == NULL) {
        return (len < ZIP_BIGLEN) ? 1 : sizeof(len)+1;
    // 写入并返回编码len所需的字节数量
    } else {
    	// 1字节
        if (len < ZIP_BIGLEN) {
            p[0] = len;
            return 1;
        // 5字节
        } else {
        	// 添加5字节长度标识
            p[0] = ZIP_BIGLEN;
            // 写入编码
            memcpy(p+1,&len,sizeof(len));
            // 如果有必要的话，进行大小端转换
            memrev32ifbe(p+1);
            // 返回编码长度
            return 1+sizeof(len);
        }
    }
}

/* Encode the length of the previous entry and write it to "p". This only
 * uses the larger encoding (required in __ziplistCascadeUpdate).
 *
 * 将原本只需要1个字节来保存的前置节点长度len编码至一个5字节长的header中。
 *
 * T = O(1)
 */
static void zipPrevEncodeLengthForceLarge(unsigned char *p, unsigned int len) {
    if (p == NULL) return;
    // 设置5字节长度标识
    p[0] = ZIP_BIGLEN;
    // 写入len
    memcpy(p+1,&len,sizeof(len));
    memrev32ifbe(p+1);
}

/* Decode the number of bytes required to store the length of the previous
 * element, from the perspective of the entry pointed to by 'ptr'.
 *
 * 解码ptr指针，取出编码前置节点，长度所需的字节数，并将它保存到prevlensize变量中。
 *
 * T = O(1)
 */
#define ZIP_DECODE_PREVLENSIZE(ptr, prevlensize) do {                          \
    if ((ptr)[0] < ZIP_BIGLEN) {                                               \
        (prevlensize) = 1;                                                     \
    } else {                                                                   \
        (prevlensize) = 5;                                                     \
    }                                                                          \
} while(0);

/* Decode the length of the previous element, from the perspective of the entry
 * pointed to by 'ptr'.
 *
 * 解码ptr指针，取出编码前置节点长度所需的字节数，并将这个字节数保存到prevlensize中。
 *
 * 然后根据prevlensize，从ptr中取出前置节点的长度值，并将这个长度值保存到prevlen变量中。
 *
 * T = O(1)
 */
#define ZIP_DECODE_PREVLEN(ptr, prevlensize, prevlen) do {                     \
	/* 先计算被编码长度值的字节数 */                                             \
    ZIP_DECODE_PREVLENSIZE(ptr, prevlensize);                                  \
    /* 再根据编码字节数来取出长度值 */                                           \
    if ((prevlensize) == 1) {                                                  \
        (prevlen) = (ptr)[0];                                                  \
    } else if ((prevlensize) == 5) {                                           \
        assert(sizeof((prevlensize)) == 4);                                    \
        memcpy(&(prevlen), ((char*)(ptr)) + 1, 4);                             \
        memrev32ifbe(&prevlen);                                                \
    }                                                                          \
} while(0);

/* Return the difference in number of bytes needed to store the length of the
 * previous element 'len', in the entry pointed to by 'p'.
 *
 * 计算编码新的前置节点长度len所需的字节数，减去编码p原来的前置节点长度所需的字节数之差。
 *
 * T = O(1)
 */
static int zipPrevLenByteDiff(unsigned char *p, unsigned int len) {
    unsigned int prevlensize;
    // 取出编码原来的前置节点长度所需的字节数
    // T = O(1)
    ZIP_DECODE_PREVLENSIZE(p, prevlensize);
    // 计算编码len所需的字节数，然后进行减法运算
    // T = O(1)
    return zipPrevEncodeLength(NULL, len) - prevlensize;
}

/* Return the total number of bytes used by the entry pointed to by 'p'.
 *
 * 返回指针p所指向的节点占用的字节数总和。
 *
 * T = O(1)
 */
static unsigned int zipRawEntryLength(unsigned char *p) {
    unsigned int prevlensize, encoding, lensize, len;
    // 取出编码前置节点的长度所需的字节数
    // T = O(1)
    ZIP_DECODE_PREVLENSIZE(p, prevlensize);
    // 取出当前节点值的编码类型，编码节点值长度所需的字节数，以及节点值的长度
    // T = O(1)
    ZIP_DECODE_LENGTH(p + prevlensize, encoding, lensize, len);
    // 计算节点占用的字节数总和
    return prevlensize + lensize + len;
}

/* Check if string pointed to by 'entry' can be encoded as an integer.
 * Stores the integer value in 'v' and its encoding in 'encoding'.
 *
 * 检查entry中指向的字符串能否被编码为整数。
 * 如果可以的话，将编码后的整数保存在指针v的值中，并将编码的方式保存在指针encoding的值中。
 *
 * 注意，这里的entry和前面代表节点的entry不是一个意思。
 *
 * T = O(N)
 */
static int zipTryEncoding(unsigned char *entry, unsigned int entrylen, long long *v, unsigned char *encoding) {
    long long value;

    // 忽略太长或太短的字符串
    if (entrylen >= 32 || entrylen == 0) return 0;
    // 尝试转换
    // T = O(N)
    if (string2ll((char*)entry,entrylen,&value)) {
        /* Great, the string can be encoded. Check what's the smallest
         * of our encoding types that can hold this value. */
    	// 转换成功，以从小到大的顺序检查适合值value的编码方式
        if (value >= 0 && value <= 12) {
            *encoding = ZIP_INT_IMM_MIN+value;
        } else if (value >= INT8_MIN && value <= INT8_MAX) {
            *encoding = ZIP_INT_8B;
        } else if (value >= INT16_MIN && value <= INT16_MAX) {
            *encoding = ZIP_INT_16B;
        } else if (value >= INT24_MIN && value <= INT24_MAX) {
            *encoding = ZIP_INT_24B;
        } else if (value >= INT32_MIN && value <= INT32_MAX) {
            *encoding = ZIP_INT_32B;
        } else {
            *encoding = ZIP_INT_64B;
        }
        // 记录值到指针
        *v = value;
        // 返回转换成功标识
        return 1;
    }
    // 转换失败
    return 0;
}

/* Store integer 'value' at 'p', encoded as 'encoding'
 *
 * 以encoding指定的编码方式，将整数值value写入到p。
 *
 * T = O(1)
 */
static void zipSaveInteger(unsigned char *p, int64_t value, unsigned char encoding) {
    int16_t i16;
    int32_t i32;
    int64_t i64;
    if (encoding == ZIP_INT_8B) {
        ((int8_t*)p)[0] = (int8_t)value;
    } else if (encoding == ZIP_INT_16B) {
        i16 = value;
        memcpy(p,&i16,sizeof(i16));
        memrev16ifbe(p);
    } else if (encoding == ZIP_INT_24B) {
        i32 = value<<8;
        memrev32ifbe(&i32);
        memcpy(p,((uint8_t*)&i32)+1,sizeof(i32)-sizeof(uint8_t));
    } else if (encoding == ZIP_INT_32B) {
        i32 = value;
        memcpy(p,&i32,sizeof(i32));
        memrev32ifbe(p);
    } else if (encoding == ZIP_INT_64B) {
        i64 = value;
        memcpy(p,&i64,sizeof(i64));
        memrev64ifbe(p);
    } else if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX) {
        /* Nothing to do, the value is stored in the encoding itself. */
    } else {
        assert(NULL);
    }
}

/* Read integer encoded as 'encoding' from 'p'
 *
 * 以encoding指定的编码方式，读取并返回指针p中的整数值。
 *
 * T = O(1)
 */
static int64_t zipLoadInteger(unsigned char *p, unsigned char encoding) {
    int16_t i16;
    int32_t i32;
    int64_t i64, ret = 0;
    if (encoding == ZIP_INT_8B) {
        ret = ((int8_t*)p)[0];
    } else if (encoding == ZIP_INT_16B) {
        memcpy(&i16,p,sizeof(i16));
        memrev16ifbe(&i16);
        ret = i16;
    } else if (encoding == ZIP_INT_32B) {
        memcpy(&i32,p,sizeof(i32));
        memrev32ifbe(&i32);
        ret = i32;
    } else if (encoding == ZIP_INT_24B) {
        i32 = 0;
        memcpy(((uint8_t*)&i32)+1,p,sizeof(i32)-sizeof(uint8_t));
        memrev32ifbe(&i32);
        ret = i32>>8;
    } else if (encoding == ZIP_INT_64B) {
        memcpy(&i64,p,sizeof(i64));
        memrev64ifbe(&i64);
        ret = i64;
    } else if (encoding >= ZIP_INT_IMM_MIN && encoding <= ZIP_INT_IMM_MAX) {
        ret = (encoding & ZIP_INT_IMM_MASK)-1;
    } else {
        assert(NULL);
    }
    return ret;
}

/* Return a struct with all information about an entry.
 *
 * 将p所指向的列表节点的信息全部保存到zlentry中，并返回改zlentry。
 *
 * T = O(1)
 */
static zlentry zipEntry(unsigned char *p) {
    zlentry e;

    // e.prevrawlensize保存着编码前一个节点的长度所需的字节数
    // prevrawlen保存着前一个节点的长度
    // T = O(1)
    ZIP_DECODE_PREVLEN(p, e.prevrawlensize, e.prevrawlen);
    // p + e.prevrawlensize将指针移动到列表节点本身
    // e.encoding保存着节点值的编码类型
    // e.lensize保存着编码节点值长度所需的字节数
    // e.len保存着节点值的长度
    // T = O(1)
    ZIP_DECODE_LENGTH(p + e.prevrawlensize, e.encoding, e.lensize, e.len);
    // 计算头结点的字节数
    e.headersize = e.prevrawlensize + e.lensize;
    // 记录指针
    e.p = p;
    return e;
}

/* Create a new empty ziplist.
 *
 * 创建并返回一个新的ziplist
 *
 * T = O(1)
 */
unsigned char *ziplistNew(void) {
	// ZIPLIST_HEADER_SIZE是ziplist表头的大小
	// 1字节是表末端ZIP_END的大小
    unsigned int bytes = ZIPLIST_HEADER_SIZE+1;
    // 为表头和表末端分配空间
    unsigned char *zl = zmalloc(bytes);
    // 初始化表属性
    ZIPLIST_BYTES(zl) = intrev32ifbe(bytes);
    ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(ZIPLIST_HEADER_SIZE);
    ZIPLIST_LENGTH(zl) = 0;
    // 设置表末端
    zl[bytes-1] = ZIP_END;
    return zl;
}

/* Resize the ziplist.
 *
 * 调整ziplist的大小为len字节。
 *
 * 当ziplist原有的大小小于len时，扩展ziplist不会改变ziplist原有的元素。
 *
 * T = O(N)
 */
static unsigned char *ziplistResize(unsigned char *zl, unsigned int len) {
	// 用zrealloc，扩展时不改变现有元素
    zl = zrealloc(zl,len);
    // 更新bytes属性
    ZIPLIST_BYTES(zl) = intrev32ifbe(len);
    // 重新设置表末端
    zl[len-1] = ZIP_END;
    return zl;
}

/* When an entry is inserted, we need to set the prevlen field of the next
 * entry to equal the length of the inserted entry. It can occur that this
 * length cannot be encoded in 1 byte and the next entry needs to be grow
 * a bit larger to hold the 5-byte encoded prevlen. This can be done for free,
 * because this only happens when an entry is already being inserted (which
 * causes a realloc and memmove). However, encoding the prevlen may require
 * that this entry is grown as well. This effect may cascade throughout
 * the ziplist when there are consecutive entries with a size close to
 * ZIP_BIGLEN, so we need to check that the prevlen can be encoded in every
 * consecutive entry.
 *
 * 当将一个新节点添加到某个节点之前的时候，如果原节点的header空间不足以保存新节点的长度，那么就需要对原节点的header空间进行扩展(从1字节扩展到5字节)。但是，当
 * 对原节点进行扩展之后，原节点的下一个节点的prevlen可能出现空间不足，这种情况在多个连续节点的长度都接近ZIP_BIGLEN时可能发生。
 *
 * 这个函数就用于检查并修复后续节点的空间问题。
 *
 * Note that this effect can also happen in reverse, where the bytes required
 * to encode the prevlen field can shrink. This effect is deliberately ignored,
 * because it can cause a "flapping" effect where a chain prevlen fields is
 * first grown and then shrunk again after consecutive inserts. Rather, the
 * field is allowed to stay larger than necessary, because a large prevlen
 * field implies the ziplist is holding large entries anyway.
 *
 * 反过来说，因为节点的长度变小而引起的连续缩小也是可能出现的，不过，为了避免扩展-缩小-扩展-缩小这样的情况反复出现(flapping, 抖动)，我们不处理这种情况，而是任
 * 由prevlen比所需的长度更长。
 *
 * The pointer "p" points to the first entry that does NOT need to be
 * updated, i.e. consecutive fields MAY need an update.
 *
 * 注意，程序的检查是针对p的后续节点，而不是p所指向的节点。因为节点p在传入之前已经完成了所需的扩展扩展工作。
 *
 * T = O(N^2)
 */
static unsigned char *__ziplistCascadeUpdate(unsigned char *zl, unsigned char *p) {
    size_t curlen = intrev32ifbe(ZIPLIST_BYTES(zl)), rawlen, rawlensize;
    size_t offset, noffset, extra;
    unsigned char *np;
    zlentry cur, next;

    // T = O(N^2)
    while (p[0] != ZIP_END) {
    	// 将p所指向的节点的信息保存到cur结构中
        cur = zipEntry(p);
        // 当前节点的长度
        rawlen = cur.headersize + cur.len;
        // 计算编码当前节点的长度所需的字节数
        // T = O(1)
        rawlensize = zipPrevEncodeLength(NULL,rawlen);

        /* Abort if there is no next entry. */
        // 如果已经没有后续空间需要更新了，跳出
        if (p[rawlen] == ZIP_END) break;
        // 取出后续节点的信息，保存到next结构中
        // T = O(1)
        next = zipEntry(p+rawlen);

        /* Abort when "prevlen" has not changed. */
        // 后续节点编码当前节点的空间已经足够，无须再进行任何处理，跳出
        // 可以证明，只要遇到一个空间足够的节点，那么这个节点之后的所有节点的是空间都是足够的
        if (next.prevrawlen == rawlen) break;

        if (next.prevrawlensize < rawlensize) {
            /* The "prevlen" field of "next" needs more bytes to hold
             * the raw length of "cur". */
        	// 执行到这里，表示next空间的大小不足以编码cur的长度，所以程序需要对next节点的(header部分)空间进行扩展

        	// 记录p的偏移量
            offset = p-zl;
            // 计算需要增加的节点数量
            extra = rawlensize-next.prevrawlensize;
            // 扩展zl的大小
            // T = O(N)
            zl = ziplistResize(zl,curlen+extra);
            // 还原指针p
            p = zl+offset;

            /* Current pointer and offset for next element. */
            // 记录下一节点的偏移量
            np = p+rawlen;
            noffset = np-zl;

            /* Update tail offset when next element is not the tail element. */
            // 当next节点不是表尾节点时，更新列表到表尾节点的偏移量
			// 不用更新的情况(next为表尾节点)：
			// |     | next |      ==>    |     | new next          |
			//       ^                          ^
			//       |                          |
			//     tail                        tail
			//
			// 需要更新的情况(next不是表尾节点)：
			// | next |     |   ==>     | new next          |     |
			//        ^                        ^
			//        |                        |
			//    old tail                 old tail
			//
			// 更新之后：
			// | new next          |     |
			//                     ^
			//                     |
			//                  new tail
			// T = O(1)
            if ((zl+intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))) != np) {
                ZIPLIST_TAIL_OFFSET(zl) =
                    intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))+extra);
            }

            /* Move the tail to the back. */
            // 向后移动cur节点之后的数据，为cur的新header腾出空间
			// 示例：
			// | header | value |  ==>  | header |    | value |  ==>  | header      | value |
			//                                   |<-->|
			//                            为新header腾出的空间
			// T = O(N)
            memmove(np+rawlensize,
                np+next.prevrawlensize,
                curlen-noffset-next.prevrawlensize-1);
            // 将新的前一节点长度值编码进行新的next节点的header
            // T = O(1)
            zipPrevEncodeLength(np,rawlen);

            /* Advance the cursor */
            // 移动指针，继续处理下个节点
            p += rawlen;
            curlen += extra;
        } else {
            if (next.prevrawlensize > rawlensize) {
                /* This would result in shrinking, which we want to avoid.
                 * So, set "rawlen" in the available bytes. */
            	// 执行到这里，说明next节点编码前置节点的header空间有5字节，而编码rawlen只需要1字节，但是程序不会对next进行缩小，所以这里只将rawlen写入5字节的header中就算了。
            	// T = O(1)
                zipPrevEncodeLengthForceLarge(p+rawlen,rawlen);
            } else {
            	// 运行到这里，说明cur节点的长度正好可以编码到next节点的header中
            	// T = O(1)
                zipPrevEncodeLength(p+rawlen,rawlen);
            }

            /* Stop here, as the raw length of "next" has not changed. */
            break;
        }
    }
    return zl;
}

/* Delete "num" entries, starting at "p". Returns pointer to the ziplist.
 *
 * 从位置p开始，连续删除num个节点。
 *
 * 函数的返回值为处理删除操作之后操作的ziplist。
 *
 * T = O(N^2)
 */
static unsigned char *__ziplistDelete(unsigned char *zl, unsigned char *p, unsigned int num) {
    unsigned int i, totlen, deleted = 0;
    size_t offset;
    int nextdiff = 0;
    zlentry first, tail;

    // 计算被删除节点总共，占用的内存字节数
    // 以及被删除节点的总个数
    // T = O(N)
    first = zipEntry(p);
    for (i = 0; p[0] != ZIP_END && i < num; i++) {
        p += zipRawEntryLength(p);
        deleted++;
    }

    // totlen是所有被删除节点总共占用的内存字节数
    totlen = p-first.p;
    if (totlen > 0) {
        if (p[0] != ZIP_END) {
        	// 执行这里，表示被删除节点之后仍然有节点存在

            /* Storing `prevrawlen` in this entry may increase or decrease the
             * number of bytes required compare to the current `prevrawlen`.
             * There always is room to store this, because it was previously
             * stored by an entry that is now being deleted. */
        	// 因为位于被删除范围之后的第一个节点的header部分的大小可能容纳不了新的前置节点，所以需要计算新旧前置节点之间的字节数差
        	// T = O(1)
            nextdiff = zipPrevLenByteDiff(p,first.prevrawlen);
            // 如果有需要的话，将指针p后退nextdiff字节，为新header空出空间
            p -= nextdiff;
            // 将first的前置节点的长度编码至p中
            // T = O(1)
            zipPrevEncodeLength(p,first.prevrawlen);

            /* Update offset for tail */
            // 更新到达表尾的偏移量
            // T = O(1)
            ZIPLIST_TAIL_OFFSET(zl) =
                intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))-totlen);

            /* When the tail contains more than one entry, we need to take
             * "nextdiff" in account as well. Otherwise, a change in the
             * size of prevlen doesn't have an effect on the *tail* offset. */
            // 如果被删除节点之后，有多于一个节点。那么程序需要将nextdiff记录的字节数也计算到表尾偏移量中，这样才能让表尾偏移量正确对齐表尾节点
            // T = O(1)
            tail = zipEntry(p);
            if (p[tail.headersize+tail.len] != ZIP_END) {
                ZIPLIST_TAIL_OFFSET(zl) =
                   intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))+nextdiff);
            }

            /* Move tail to the front of the ziplist */
            // 从表尾向表头移动数据，覆盖被删除节点的数据
            // T = O(N)
            memmove(first.p,p,
                intrev32ifbe(ZIPLIST_BYTES(zl))-(p-zl)-1);
        } else {
        	// 执行这里，表示被删除节点之后已经没有其他节点了
            /* The entire tail was deleted. No need to move memory. */
        	// T = O(1)
            ZIPLIST_TAIL_OFFSET(zl) =
                intrev32ifbe((first.p-zl)-first.prevrawlen);
        }

        /* Resize and update length */
        // 缩小并更新ziplist的长度
        offset = first.p-zl;
        zl = ziplistResize(zl, intrev32ifbe(ZIPLIST_BYTES(zl))-totlen+nextdiff);
        ZIPLIST_INCR_LENGTH(zl,-deleted);
        p = zl+offset;

        /* When nextdiff != 0, the raw length of the next entry has changed, so
         * we need to cascade the update throughout the ziplist */
        // 如果p所指向的节点的大小已经变更，那么进行级联更新
        // 检查p之后的所有节点是否符合ziplist的编码要求
        // T = O(N^2)
        if (nextdiff != 0)
            zl = __ziplistCascadeUpdate(zl,p);
    }
    return zl;
}

/* Insert item at "p". */
/*
 * 根据指针p所指定的位置，将长度为slen的字符串s插入到zl中。
 *
 * 函数的返回值为完成插入操作后的ziplist
 *
 * T = O(N^2)
 */
static unsigned char *__ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen) {
	// 记录当前ziplist的长度
    size_t curlen = intrev32ifbe(ZIPLIST_BYTES(zl)), reqlen;
    unsigned int prevlensize, prevlen = 0;
    size_t offset;
    int nextdiff = 0;
    unsigned char encoding = 0;
    long long value = 123456789; /* initialized to avoid warning. Using a value
                                    that is easy to see if for some reason
                                    we use it uninitialized. */
    zlentry tail;

    /* Find out prevlen for the entry that is inserted. */
    if (p[0] != ZIP_END) {
    	// 如果p[0]不指向列表末端，说明列表非空，并且p正指向列表的其中一个节点
    	// 那么取出p所指向节点的信息，并将它保存到entry结构中
    	// 然后用prevlen变量记录前置节点的长度
    	// (当插入新节点之后p所指向的节点就成了新节点的前置节点)
    	// T = O(1)
        ZIP_DECODE_PREVLEN(p, prevlensize, prevlen);
    } else {
    	// 如果p指向表尾末端，那么程序需要检查列表是否为：
    	// 1)如果ptail也指向ZIP_END，那么列表为空；
    	// 2)如果列表不为空，那么ptail将指向列表的最后一个节点。
        unsigned char *ptail = ZIPLIST_ENTRY_TAIL(zl);
        if (ptail[0] != ZIP_END) {
        	// 表尾节点为新节点的前置节点

        	// 取出表尾节点的长度
        	// T = O(1)
            prevlen = zipRawEntryLength(ptail);
        }
    }

    /* See if the entry can be encoded */
    // 尝试看能否将输入字符串转换为整数，如果成功的话：
    // 1)value将保存转换后的整数值
    // 2)encoding则保存适用于value的编码方式
    // 无论使用什么编码，reqlen都保存节点值的长度
    // T = O(N)
    if (zipTryEncoding(s,slen,&value,&encoding)) {
        /* 'encoding' is set to the appropriate integer encoding */
        reqlen = zipIntSize(encoding);
    } else {
        /* 'encoding' is untouched, however zipEncodeLength will use the
         * string length to figure out how to encode it. */
        reqlen = slen;
    }
    /* We need space for both the length of the previous entry and
     * the length of the payload. */
    // 计算编码前置节点的长度所需的大小
    // T = O(1)
    reqlen += zipPrevEncodeLength(NULL,prevlen);
    // 计算编码当前节点值所需的大小
    // T = O(1)
    reqlen += zipEncodeLength(NULL,encoding,slen);

    /* When the insert position is not equal to the tail, we need to
     * make sure that the next entry can hold this entry's length in
     * its prevlen field. */
    // 只要新节点不是被添加到列表末端，那么程序就需要检查看p所指向的节点(的header)能否编码新节点的长度。
    // nextdiff保存了新旧编码之间的字节大小差，如果这个值大于0，那么说明需要对p所指向的节点(的header)进行扩展
    // T = O(1)
    nextdiff = (p[0] != ZIP_END) ? zipPrevLenByteDiff(p,reqlen) : 0;

    /* Store offset because a realloc may change the address of zl. */
    // 因为重分配空间可能会改变zl的地址
    // 所以在分配之前，需要记录zl到p的偏移量，然后在分配之后依靠偏移量还原p
    offset = p-zl;
    // curlen是ziplist原来的长度
    // reqlen是整个新节点的长度
    // nextdiff是新节点的后继节点扩展header的长度(要么0字节，要么4个字节)
    // T = O(N)
    zl = ziplistResize(zl,curlen+reqlen+nextdiff);
    p = zl+offset;

    /* Apply memory move when necessary and update tail offset. */
    if (p[0] != ZIP_END) {
    	// 新元素之后还有节点，因为新元素的加入，需要对这些原有节点进行调整

        /* Subtract one because of the ZIP_END bytes */
    	// 移动现有元素，为新元素的插入空间腾出位置
    	// T = O(N)
        memmove(p+reqlen,p-nextdiff,curlen-offset-1+nextdiff);

        /* Encode this entry's raw length in the next entry. */
        // 将新节点的长度编码至后置节点
        // p+reqlen定位到后置节点
        // reqlen是新节点的长度
        // T = O(1)
        zipPrevEncodeLength(p+reqlen,reqlen);

        /* Update offset for tail */
        // 更新到达表尾的偏移量，将新节点的长度也算上
        ZIPLIST_TAIL_OFFSET(zl) =
            intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))+reqlen);

        /* When the tail contains more than one entry, we need to take
         * "nextdiff" in account as well. Otherwise, a change in the
         * size of prevlen doesn't have an effect on the *tail* offset. */
        // 如果新节点的后面有多于一个节点，那么程序需要将nextdiff记录的字节数也计算到表尾偏移量中，这样才能让表尾偏移量正确对齐表尾节点
        // T = O(1)
        tail = zipEntry(p+reqlen);
        if (p[reqlen+tail.headersize+tail.len] != ZIP_END) {
            ZIPLIST_TAIL_OFFSET(zl) =
                intrev32ifbe(intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl))+nextdiff);
        }
    } else {
        /* This element will be the new tail. */
        ZIPLIST_TAIL_OFFSET(zl) = intrev32ifbe(p-zl);
    }

    /* When nextdiff != 0, the raw length of the next entry has changed, so
     * we need to cascade the update throughout the ziplist */
    // 当nextdiff != 0时，新节点的后继节点的(header部分)长度已经被改变，所以需要级联地更新后续的节点
    if (nextdiff != 0) {
        offset = p-zl;
        // T = O(N^2)
        zl = __ziplistCascadeUpdate(zl,p+reqlen);
        p = zl+offset;
    }

    /* Write the entry */
    // 一切搞定，将前置节点的长度写入新节点的header
    p += zipPrevEncodeLength(p,prevlen);
    // 将节点值的长度写入新节点的header
    p += zipEncodeLength(p,encoding,slen);
    // 写入节点值
    if (ZIP_IS_STR(encoding)) {
    	// T = O(N)
        memcpy(p,s,slen);
    } else {
    	// T = O(1)
        zipSaveInteger(p,value,encoding);
    }
    // 更新列表的节点数量计数器
    // T = O(1)
    ZIPLIST_INCR_LENGTH(zl,1);
    return zl;
}

/*
 * 将长度为slen的字符串s推送到zl中
 *
 * where参数的值决定了推入的方向：
 * -值为ZIPLIST_HEAD时，将新值插推入到表头。
 * -否则，将新值推入到表末端。
 *
 * 函数的返回值为添加新值后的aiplist。
 *
 * T = O(N^2)
 */
unsigned char *ziplistPush(unsigned char *zl, unsigned char *s, unsigned int slen, int where) {
	// 根据where参数的值，决定将值推入到表头还是表尾
    unsigned char *p;
    p = (where == ZIPLIST_HEAD) ? ZIPLIST_ENTRY_HEAD(zl) : ZIPLIST_ENTRY_END(zl);
    // 返回添加新值后的ziplist
    // T = O(N^2)
    return __ziplistInsert(zl,p,s,slen);
}

/* Returns an offset to use for iterating with ziplistNext. When the given
 * index is negative, the list is traversed back to front. When the list
 * doesn't contain an element at the provided index, NULL is returned. */
/*
 * 根据给定索引，遍历列表，并返回索引指定节点的指针。
 *
 * 如果索引为正，那么从表头向表尾遍历。
 * 如果索引为负，那么从表尾向表头遍历。
 * 整数索引从0开始，复数索引从-1开始。
 *
 * 如果索引超过列表的节点数量，或者列表为空，那么返回NULL。
 *
 * T = O(N)
 */
unsigned char *ziplistIndex(unsigned char *zl, int index) {
    unsigned char *p;
    unsigned int prevlensize, prevlen = 0;
    // 处理负数索引
    if (index < 0) {
    	// 将索引转换为正数
        index = (-index)-1;
        // 定位到表尾节点
        p = ZIPLIST_ENTRY_TAIL(zl);
        // 如果列表不为空，那么...
        if (p[0] != ZIP_END) {
            ZIP_DECODE_PREVLEN(p, prevlensize, prevlen);
            // 从表尾向表头遍历
            // T = O(N)
            while (prevlen > 0 && index--) {
            	// 前移指针
                p -= prevlen;
                // T = O(1)
                ZIP_DECODE_PREVLEN(p, prevlensize, prevlen);
            }
        }
    // 处理正数索引
    } else {
    	// 定位到表头节点
        p = ZIPLIST_ENTRY_HEAD(zl);
        // T = O(N)
        while (p[0] != ZIP_END && index--) {
        	// 后移指针
        	// T = O(1)
            p += zipRawEntryLength(p);
        }
    }
    // 返回结果
    return (p[0] == ZIP_END || index > 0) ? NULL : p;
}

/* Return pointer to next entry in ziplist.
 *
 * zl is the pointer to the ziplist
 * p is the pointer to the current element
 *
 * The element after 'p' is returned, otherwise NULL if we are at the end. */
/*
 * 返回p所指向节点的后置节点。
 *
 * 如果p为表末端，或者p已经是表尾节点，那么返回NULL。
 *
 * T = O(1)
 */
unsigned char *ziplistNext(unsigned char *zl, unsigned char *p) {
    ((void) zl);

    /* "p" could be equal to ZIP_END, caused by ziplistDelete,
     * and we should return NULL. Otherwise, we should return NULL
     * when the *next* element is ZIP_END (there is no next entry). */
    // p已经指向列表末端
    if (p[0] == ZIP_END) {
        return NULL;
    }

    // 指向后一节点
    p += zipRawEntryLength(p);
    if (p[0] == ZIP_END) {
    	// p已经是表尾节点，没有后置节点
        return NULL;
    }

    return p;
}

/* Return pointer to previous entry in ziplist. */
/*
 * 返回p所指向节点的前置节点。
 *
 * 如果p所指向为空列表，或者p已经指向表头字节，那么返回NULL。
 *
 * T = O(1)
 */
unsigned char *ziplistPrev(unsigned char *zl, unsigned char *p) {
    unsigned int prevlensize, prevlen = 0;

    /* Iterating backwards from ZIP_END should return the tail. When "p" is
     * equal to the first element of the list, we're already at the head,
     * and should return NULL. */
    // 如果p指向列表末端(列表为空，或者刚开始从表尾向表头迭代)，那么尝试取出列表尾端节点
    if (p[0] == ZIP_END) {
        p = ZIPLIST_ENTRY_TAIL(zl);
        // 尾端节点也指向列表末端，那么列表为空
        return (p[0] == ZIP_END) ? NULL : p;
    // 如果p指向列表头，那么说明迭代已经完成
    } else if (p == ZIPLIST_ENTRY_HEAD(zl)) {
        return NULL;
    // 既不是表头也不是表尾，从表尾向表头移动指针
    } else {
    	// 计算前一个节点的节点数
        ZIP_DECODE_PREVLEN(p, prevlensize, prevlen);
        assert(prevlen > 0);
        // 移动指针，指向前一个节点
        return p-prevlen;
    }
}

/* Get entry pointed to by 'p' and store in either '*sstr' or 'sval' depending
 * on the encoding of the entry. '*sstr' is always set to NULL to be able
 * to find out whether the string pointer or the integer value was set.
 * Return 0 if 'p' points to the end of the ziplist, 1 otherwise. */
/*
 * 取出p所指向节点的值：
 * -如果节点保存的是字符串，那么将字符串值指针保存到*sstr中，字符串长度保存到*slen
 * -如果节点保存的是整数，那么将整数保存到*sval
 *
 * 程序可以通过检查*sstr是否为NULL来检查值是字符串还是整数。
 *
 * 提取值成功返回1，如果p为空，或者p指向的是列表末端，那么返回0，提取值失败。
 *
 * T = O(1)
 */
unsigned int ziplistGet(unsigned char *p, unsigned char **sstr, unsigned int *slen, long long *sval) {
    zlentry entry;
    if (p == NULL || p[0] == ZIP_END) return 0;
    if (sstr) *sstr = NULL;

    // 取出p所指向的节点的各项信息，并保存到结构entry中
    // T = O(1)
    entry = zipEntry(p);
    // 节点的值为字符串，将字符串长度保存到*slen，字符串保存到*sstr
    // T = O(1)
    if (ZIP_IS_STR(entry.encoding)) {
        if (sstr) {
            *slen = entry.len;
            *sstr = p+entry.headersize;
        }
    // 节点的值为整数，解码值，并将值保存到*sval
    // T = O(1)
    } else {
        if (sval) {
            *sval = zipLoadInteger(p+entry.headersize,entry.encoding);
        }
    }
    return 1;
}

/* Insert an entry at "p".
 *
 * 将包含给定值s的新节点插入到给定的位置p中。
 *
 * 如果p指向一个节点，那么新节点将放在原有节点的前面。
 *
 * T = O(N^2)
 */
unsigned char *ziplistInsert(unsigned char *zl, unsigned char *p, unsigned char *s, unsigned int slen) {
    return __ziplistInsert(zl,p,s,slen);
}

/* Delete a single entry from the ziplist, pointed to by *p.
 * Also update *p in place, to be able to iterate over the
 * ziplist, while deleting entries.
 *
 * 从zl中删除*p所指向的节点，并且原地更新*p所指向的位置，使得可以在迭代列表的过程中对节点进行删除。
 *
 * T = O(N^2)
 */
unsigned char *ziplistDelete(unsigned char *zl, unsigned char **p) {
	// 因为__ziplistDelete时会对zl进行内存重分配
	// 因内存重分配可能会改变zl的内存地址
	// 所以这里需要记录到达*p的偏移量
	// 这样在删除节点之后就可以通过偏移量来将*p还原到正确的位置
    size_t offset = *p-zl;
    zl = __ziplistDelete(zl,*p,1);

    /* Store pointer to current element in p, because ziplistDelete will
     * do a realloc which might result in a different "zl"-pointer.
     * When the delete direction is back to front, we might delete the last
     * entry and end up with "p" pointing to ZIP_END, so check this. */
    *p = zl+offset;
    return zl;
}

/* Delete a range of entries from the ziplist.
 *
 * 从index索引指定的节点开始，连续地从z1中删除num个节点。
 *
 * T = O(N^2)
 */
unsigned char *ziplistDeleteRange(unsigned char *zl, unsigned int index, unsigned int num) {
	// 根据索引定位到节点
	// T = O(N)
    unsigned char *p = ziplistIndex(zl,index);
    // 连续删除num个节点
    // T = O(N^2)
    return (p == NULL) ? zl : __ziplistDelete(zl,p,num);
}

/* Compare entry pointer to by 'p' with 'sstr' of length 'slen'. */
/* Return 1 if equal. */
/*
 * 将p所指向的节点的值和sstr进行对比。
 *
 * 如果节点值和sstr的值相等，返回1，不相等则返回0。
 *
 * T = O(N)
 */
unsigned int ziplistCompare(unsigned char *p, unsigned char *sstr, unsigned int slen) {
    zlentry entry;
    unsigned char sencoding;
    long long zval, sval;
    if (p[0] == ZIP_END) return 0;

    // 取出节点
    entry = zipEntry(p);
    if (ZIP_IS_STR(entry.encoding)) {
    	// 节点值为字符串，进行字符串对比

        /* Raw compare */
        if (entry.len == slen) {
        	// T = O(N)
            return memcmp(p+entry.headersize,sstr,slen) == 0;
        } else {
            return 0;
        }
    } else {
    	// 节点值为整数，进行整数对比

        /* Try to compare encoded values. Don't compare encoding because
         * different implementations may encoded integers differently. */
        if (zipTryEncoding(sstr,slen,&sval,&sencoding)) {
          zval = zipLoadInteger(p+entry.headersize,entry.encoding);
          return zval == sval;
        }
    }
    return 0;
}

/* Find pointer to the entry equal to the specified entry.
 *
 * 寻找节点值和vstr相等的列表节点，并返回该节点的指针。
 *
 * Skip 'skip' entries
 * between every comparison.
 *
 * 每次比对之前都跳过skip个节点。
 *
 * Returns NULL when the field could not be found.
 *
 * 如果找不到相应的节点，则返回NULL。
 *
 * T = O(N^2)
 */
unsigned char *ziplistFind(unsigned char *p, unsigned char *vstr, unsigned int vlen, unsigned int skip) {
    int skipcnt = 0;
    unsigned char vencoding = 0;
    long long vll = 0;

    // 只要未到达列表末端，就一直迭代
    while (p[0] != ZIP_END) {
        unsigned int prevlensize, encoding, lensize, len;
        unsigned char *q;

        ZIP_DECODE_PREVLENSIZE(p, prevlensize);
        ZIP_DECODE_LENGTH(p + prevlensize, encoding, lensize, len);
        q = p + prevlensize + lensize;

        if (skipcnt == 0) {
            /* Compare current entry with specified entry */
        	// 对比字符串值
        	// T = O(N)
            if (ZIP_IS_STR(encoding)) {
                if (len == vlen && memcmp(q, vstr, vlen) == 0) {
                    return p;
                }
            } else {
                /* Find out if the searched field can be encoded. Note that
                 * we do it only the first time, once done vencoding is set
                 * to non-zero and vll is set to the integer value. */
            	// 因为传入值有可能被编码了，所以当第一次进行值对比时，程序会对传入值进行解码
            	// 这个解码操作只会进行一次
                if (vencoding == 0) {
                    if (!zipTryEncoding(vstr, vlen, &vll, &vencoding)) {
                        /* If the entry can't be encoded we set it to
                         * UCHAR_MAX so that we don't retry again the next
                         * time. */
                        vencoding = UCHAR_MAX;
                    }
                    /* Must be non-zero by now */
                    assert(vencoding);
                }

                /* Compare current entry with specified entry, do it only
                 * if vencoding != UCHAR_MAX because if there is no encoding
                 * possible for the field it can't be a valid integer. */
                // 对比整数值
                if (vencoding != UCHAR_MAX) {
                    long long ll = zipLoadInteger(q, encoding);
                    if (ll == vll) {
                        return p;
                    }
                }
            }

            /* Reset skip count */
            skipcnt = skip;
        } else {
            /* Skip entry */
            skipcnt--;
        }

        /* Move to next entry */
        // 后移指针，指向后置节点
        p = q + len;
    }

    return NULL;
}

/* Return length of ziplist.
 *
 * 返回ziplist中的子节点数
 *
 * T = O(N)
 */
unsigned int ziplistLen(unsigned char *zl) {
    unsigned int len = 0;
    // 节点数小于UINT16_MAX
    // T = O(1)
    if (intrev16ifbe(ZIPLIST_LENGTH(zl)) < UINT16_MAX) {
        len = intrev16ifbe(ZIPLIST_LENGTH(zl));
    // 节点数大于UINT16_MAX时，需要遍历整个列表才能计算出节点数
    // T = O(N)
    } else {
        unsigned char *p = zl+ZIPLIST_HEADER_SIZE;
        while (*p != ZIP_END) {
            p += zipRawEntryLength(p);
            len++;
        }

        /* Re-store length if small enough */
        if (len < UINT16_MAX) ZIPLIST_LENGTH(zl) = intrev16ifbe(len);
    }
    return len;
}

/* Return ziplist blob size in bytes.
 *
 * 返回整个ziplist占用的内存字节数
 *
 * T = O(1)
 */
size_t ziplistBlobLen(unsigned char *zl) {
    return intrev32ifbe(ZIPLIST_BYTES(zl));
}

void ziplistRepr(unsigned char *zl) {
    unsigned char *p;
    int index = 0;
    zlentry entry;

    printf(
        "{total bytes %d} "
        "{length %u}\n"
        "{tail offset %u}\n",
        intrev32ifbe(ZIPLIST_BYTES(zl)),
        intrev16ifbe(ZIPLIST_LENGTH(zl)),
        intrev32ifbe(ZIPLIST_TAIL_OFFSET(zl)));
    p = ZIPLIST_ENTRY_HEAD(zl);
    while(*p != ZIP_END) {
        entry = zipEntry(p);
        printf(
            "{"
                "addr 0x%08lx, "
                "index %2d, "
                "offset %5ld, "
                "rl: %5u, "
                "hs %2u, "
                "pl: %5u, "
                "pls: %2u, "
                "payload %5u"
            "} ",
            (long unsigned)p,
            index,
            (unsigned long) (p-zl),
            entry.headersize+entry.len,
            entry.headersize,
            entry.prevrawlen,
            entry.prevrawlensize,
            entry.len);
        p += entry.headersize;
        if (ZIP_IS_STR(entry.encoding)) {
            if (entry.len > 40) {
                if (fwrite(p,40,1,stdout) == 0) perror("fwrite");
                printf("...");
            } else {
                if (entry.len &&
                    fwrite(p,entry.len,1,stdout) == 0) perror("fwrite");
            }
        } else {
            printf("%lld", (long long) zipLoadInteger(p,entry.encoding));
        }
        printf("\n");
        p += entry.len;
        index++;
    }
    printf("{end}\n\n");
}

#ifdef ZIPLIST_TEST_MAIN
#include <sys/time.h>
#include "adlist.h"
#include "sds.h"

#define debug(f, ...) { if (DEBUG) printf(f, __VA_ARGS__); }

unsigned char *createList() {
    unsigned char *zl = ziplistNew();
    zl = ziplistPush(zl, (unsigned char*)"foo", 3, ZIPLIST_TAIL);
    zl = ziplistPush(zl, (unsigned char*)"quux", 4, ZIPLIST_TAIL);
    zl = ziplistPush(zl, (unsigned char*)"hello", 5, ZIPLIST_HEAD);
    zl = ziplistPush(zl, (unsigned char*)"1024", 4, ZIPLIST_TAIL);
    return zl;
}

unsigned char *createIntList() {
    unsigned char *zl = ziplistNew();
    char buf[32];

    sprintf(buf, "100");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);
    sprintf(buf, "128000");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);
    sprintf(buf, "-100");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_HEAD);
    sprintf(buf, "4294967296");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_HEAD);
    sprintf(buf, "non integer");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);
    sprintf(buf, "much much longer non integer");
    zl = ziplistPush(zl, (unsigned char*)buf, strlen(buf), ZIPLIST_TAIL);
    return zl;
}

long long usec(void) {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000000)+tv.tv_usec;
}

void stress(int pos, int num, int maxsize, int dnum) {
    int i,j,k;
    unsigned char *zl;
    char posstr[2][5] = { "HEAD", "TAIL" };
    long long start;
    for (i = 0; i < maxsize; i+=dnum) {
        zl = ziplistNew();
        for (j = 0; j < i; j++) {
            zl = ziplistPush(zl,(unsigned char*)"quux",4,ZIPLIST_TAIL);
        }

        /* Do num times a push+pop from pos */
        start = usec();
        for (k = 0; k < num; k++) {
            zl = ziplistPush(zl,(unsigned char*)"quux",4,pos);
            zl = ziplistDeleteRange(zl,0,1);
        }
        printf("List size: %8d, bytes: %8d, %dx push+pop (%s): %6lld usec\n",
            i,intrev32ifbe(ZIPLIST_BYTES(zl)),num,posstr[pos],usec()-start);
        zfree(zl);
    }
}

void pop(unsigned char *zl, int where) {
    unsigned char *p, *vstr;
    unsigned int vlen;
    long long vlong;

    p = ziplistIndex(zl,where == ZIPLIST_HEAD ? 0 : -1);
    if (ziplistGet(p,&vstr,&vlen,&vlong)) {
        if (where == ZIPLIST_HEAD)
            printf("Pop head: ");
        else
            printf("Pop tail: ");

        if (vstr)
            if (vlen && fwrite(vstr,vlen,1,stdout) == 0) perror("fwrite");
        else
            printf("%lld", vlong);

        printf("\n");
        ziplistDeleteRange(zl,-1,1);
    } else {
        printf("ERROR: Could not pop\n");
        exit(1);
    }
}

int randstring(char *target, unsigned int min, unsigned int max) {
    int p = 0;
    int len = min+rand()%(max-min+1);
    int minval, maxval;
    switch(rand() % 3) {
    case 0:
        minval = 0;
        maxval = 255;
    break;
    case 1:
        minval = 48;
        maxval = 122;
    break;
    case 2:
        minval = 48;
        maxval = 52;
    break;
    default:
        assert(NULL);
    }

    while(p < len)
        target[p++] = minval+rand()%(maxval-minval+1);
    return len;
}

void verify(unsigned char *zl, zlentry *e) {
    int i;
    int len = ziplistLen(zl);
    zlentry _e;

    for (i = 0; i < len; i++) {
        memset(&e[i], 0, sizeof(zlentry));
        e[i] = zipEntry(ziplistIndex(zl, i));

        memset(&_e, 0, sizeof(zlentry));
        _e = zipEntry(ziplistIndex(zl, -len+i));

        assert(memcmp(&e[i], &_e, sizeof(zlentry)) == 0);
    }
}

int main(int argc, char **argv) {
    unsigned char *zl, *p;
    unsigned char *entry;
    unsigned int elen;
    long long value;

    /* If an argument is given, use it as the random seed. */
    if (argc == 2)
        srand(atoi(argv[1]));

    zl = createIntList();
    ziplistRepr(zl);

    zl = createList();
    ziplistRepr(zl);

    pop(zl,ZIPLIST_TAIL);
    ziplistRepr(zl);

    pop(zl,ZIPLIST_HEAD);
    ziplistRepr(zl);

    pop(zl,ZIPLIST_TAIL);
    ziplistRepr(zl);

    pop(zl,ZIPLIST_TAIL);
    ziplistRepr(zl);

    printf("Get element at index 3:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 3);
        if (!ziplistGet(p, &entry, &elen, &value)) {
            printf("ERROR: Could not access index 3\n");
            return 1;
        }
        if (entry) {
            if (elen && fwrite(entry,elen,1,stdout) == 0) perror("fwrite");
            printf("\n");
        } else {
            printf("%lld\n", value);
        }
        printf("\n");
    }

    printf("Get element at index 4 (out of range):\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 4);
        if (p == NULL) {
            printf("No entry\n");
        } else {
            printf("ERROR: Out of range index should return NULL, returned offset: %ld\n", p-zl);
            return 1;
        }
        printf("\n");
    }

    printf("Get element at index -1 (last element):\n");
    {
        zl = createList();
        p = ziplistIndex(zl, -1);
        if (!ziplistGet(p, &entry, &elen, &value)) {
            printf("ERROR: Could not access index -1\n");
            return 1;
        }
        if (entry) {
            if (elen && fwrite(entry,elen,1,stdout) == 0) perror("fwrite");
            printf("\n");
        } else {
            printf("%lld\n", value);
        }
        printf("\n");
    }

    printf("Get element at index -4 (first element):\n");
    {
        zl = createList();
        p = ziplistIndex(zl, -4);
        if (!ziplistGet(p, &entry, &elen, &value)) {
            printf("ERROR: Could not access index -4\n");
            return 1;
        }
        if (entry) {
            if (elen && fwrite(entry,elen,1,stdout) == 0) perror("fwrite");
            printf("\n");
        } else {
            printf("%lld\n", value);
        }
        printf("\n");
    }

    printf("Get element at index -5 (reverse out of range):\n");
    {
        zl = createList();
        p = ziplistIndex(zl, -5);
        if (p == NULL) {
            printf("No entry\n");
        } else {
            printf("ERROR: Out of range index should return NULL, returned offset: %ld\n", p-zl);
            return 1;
        }
        printf("\n");
    }

    printf("Iterate list from 0 to end:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 0);
        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry,elen,1,stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            p = ziplistNext(zl,p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Iterate list from 1 to end:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 1);
        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry,elen,1,stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            p = ziplistNext(zl,p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Iterate list from 2 to end:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 2);
        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry,elen,1,stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            p = ziplistNext(zl,p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Iterate starting out of range:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, 4);
        if (!ziplistGet(p, &entry, &elen, &value)) {
            printf("No entry\n");
        } else {
            printf("ERROR\n");
        }
        printf("\n");
    }

    printf("Iterate from back to front:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, -1);
        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry,elen,1,stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            p = ziplistPrev(zl,p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Iterate from back to front, deleting all items:\n");
    {
        zl = createList();
        p = ziplistIndex(zl, -1);
        while (ziplistGet(p, &entry, &elen, &value)) {
            printf("Entry: ");
            if (entry) {
                if (elen && fwrite(entry,elen,1,stdout) == 0) perror("fwrite");
            } else {
                printf("%lld", value);
            }
            zl = ziplistDelete(zl,&p);
            p = ziplistPrev(zl,p);
            printf("\n");
        }
        printf("\n");
    }

    printf("Delete inclusive range 0,0:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 0, 1);
        ziplistRepr(zl);
    }

    printf("Delete inclusive range 0,1:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 0, 2);
        ziplistRepr(zl);
    }

    printf("Delete inclusive range 1,2:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 1, 2);
        ziplistRepr(zl);
    }

    printf("Delete with start index out of range:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 5, 1);
        ziplistRepr(zl);
    }

    printf("Delete with num overflow:\n");
    {
        zl = createList();
        zl = ziplistDeleteRange(zl, 1, 5);
        ziplistRepr(zl);
    }

    printf("Delete foo while iterating:\n");
    {
        zl = createList();
        p = ziplistIndex(zl,0);
        while (ziplistGet(p,&entry,&elen,&value)) {
            if (entry && strncmp("foo",(char*)entry,elen) == 0) {
                printf("Delete foo\n");
                zl = ziplistDelete(zl,&p);
            } else {
                printf("Entry: ");
                if (entry) {
                    if (elen && fwrite(entry,elen,1,stdout) == 0)
                        perror("fwrite");
                } else {
                    printf("%lld",value);
                }
                p = ziplistNext(zl,p);
                printf("\n");
            }
        }
        printf("\n");
        ziplistRepr(zl);
    }

    printf("Regression test for >255 byte strings:\n");
    {
        char v1[257],v2[257];
        memset(v1,'x',256);
        memset(v2,'y',256);
        zl = ziplistNew();
        zl = ziplistPush(zl,(unsigned char*)v1,strlen(v1),ZIPLIST_TAIL);
        zl = ziplistPush(zl,(unsigned char*)v2,strlen(v2),ZIPLIST_TAIL);

        /* Pop values again and compare their value. */
        p = ziplistIndex(zl,0);
        assert(ziplistGet(p,&entry,&elen,&value));
        assert(strncmp(v1,(char*)entry,elen) == 0);
        p = ziplistIndex(zl,1);
        assert(ziplistGet(p,&entry,&elen,&value));
        assert(strncmp(v2,(char*)entry,elen) == 0);
        printf("SUCCESS\n\n");
    }

    printf("Regression test deleting next to last entries:\n");
    {
        char v[3][257];
        zlentry e[3];
        int i;

        for (i = 0; i < (sizeof(v)/sizeof(v[0])); i++) {
            memset(v[i], 'a' + i, sizeof(v[0]));
        }

        v[0][256] = '\0';
        v[1][  1] = '\0';
        v[2][256] = '\0';

        zl = ziplistNew();
        for (i = 0; i < (sizeof(v)/sizeof(v[0])); i++) {
            zl = ziplistPush(zl, (unsigned char *) v[i], strlen(v[i]), ZIPLIST_TAIL);
        }

        verify(zl, e);

        assert(e[0].prevrawlensize == 1);
        assert(e[1].prevrawlensize == 5);
        assert(e[2].prevrawlensize == 1);

        /* Deleting entry 1 will increase `prevrawlensize` for entry 2 */
        unsigned char *p = e[1].p;
        zl = ziplistDelete(zl, &p);

        verify(zl, e);

        assert(e[0].prevrawlensize == 1);
        assert(e[1].prevrawlensize == 5);

        printf("SUCCESS\n\n");
    }

    printf("Create long list and check indices:\n");
    {
        zl = ziplistNew();
        char buf[32];
        int i,len;
        for (i = 0; i < 1000; i++) {
            len = sprintf(buf,"%d",i);
            zl = ziplistPush(zl,(unsigned char*)buf,len,ZIPLIST_TAIL);
        }
        for (i = 0; i < 1000; i++) {
            p = ziplistIndex(zl,i);
            assert(ziplistGet(p,NULL,NULL,&value));
            assert(i == value);

            p = ziplistIndex(zl,-i-1);
            assert(ziplistGet(p,NULL,NULL,&value));
            assert(999-i == value);
        }
        printf("SUCCESS\n\n");
    }

    printf("Compare strings with ziplist entries:\n");
    {
        zl = createList();
        p = ziplistIndex(zl,0);
        if (!ziplistCompare(p,(unsigned char*)"hello",5)) {
            printf("ERROR: not \"hello\"\n");
            return 1;
        }
        if (ziplistCompare(p,(unsigned char*)"hella",5)) {
            printf("ERROR: \"hella\"\n");
            return 1;
        }

        p = ziplistIndex(zl,3);
        if (!ziplistCompare(p,(unsigned char*)"1024",4)) {
            printf("ERROR: not \"1024\"\n");
            return 1;
        }
        if (ziplistCompare(p,(unsigned char*)"1025",4)) {
            printf("ERROR: \"1025\"\n");
            return 1;
        }
        printf("SUCCESS\n\n");
    }

    printf("Stress with random payloads of different encoding:\n");
    {
        int i,j,len,where;
        unsigned char *p;
        char buf[1024];
        int buflen;
        list *ref;
        listNode *refnode;

        /* Hold temp vars from ziplist */
        unsigned char *sstr;
        unsigned int slen;
        long long sval;

        for (i = 0; i < 20000; i++) {
            zl = ziplistNew();
            ref = listCreate();
            listSetFreeMethod(ref,sdsfree);
            len = rand() % 256;

            /* Create lists */
            for (j = 0; j < len; j++) {
                where = (rand() & 1) ? ZIPLIST_HEAD : ZIPLIST_TAIL;
                if (rand() % 2) {
                    buflen = randstring(buf,1,sizeof(buf)-1);
                } else {
                    switch(rand() % 3) {
                    case 0:
                        buflen = sprintf(buf,"%lld",(0LL + rand()) >> 20);
                        break;
                    case 1:
                        buflen = sprintf(buf,"%lld",(0LL + rand()));
                        break;
                    case 2:
                        buflen = sprintf(buf,"%lld",(0LL + rand()) << 20);
                        break;
                    default:
                        assert(NULL);
                    }
                }

                /* Add to ziplist */
                zl = ziplistPush(zl, (unsigned char*)buf, buflen, where);

                /* Add to reference list */
                if (where == ZIPLIST_HEAD) {
                    listAddNodeHead(ref,sdsnewlen(buf, buflen));
                } else if (where == ZIPLIST_TAIL) {
                    listAddNodeTail(ref,sdsnewlen(buf, buflen));
                } else {
                    assert(NULL);
                }
            }

            assert(listLength(ref) == ziplistLen(zl));
            for (j = 0; j < len; j++) {
                /* Naive way to get elements, but similar to the stresser
                 * executed from the Tcl test suite. */
                p = ziplistIndex(zl,j);
                refnode = listIndex(ref,j);

                assert(ziplistGet(p,&sstr,&slen,&sval));
                if (sstr == NULL) {
                    buflen = sprintf(buf,"%lld",sval);
                } else {
                    buflen = slen;
                    memcpy(buf,sstr,buflen);
                    buf[buflen] = '\0';
                }
                assert(memcmp(buf,listNodeValue(refnode),buflen) == 0);
            }
            zfree(zl);
            listRelease(ref);
        }
        printf("SUCCESS\n\n");
    }

    printf("Stress with variable ziplist size:\n");
    {
        stress(ZIPLIST_HEAD,100000,16384,256);
        stress(ZIPLIST_TAIL,100000,16384,256);
    }

    return 0;
}

#endif
