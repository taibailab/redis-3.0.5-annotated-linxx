/*
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

#ifndef __INTSET_H
#define __INTSET_H
#include <stdint.h>

/*
 * 整数集合(intset)用于保存整数值的集合抽象数据结构，保存类型为int16_t、int32_t或者int64_t的整数值，并且保存集合中不会出现重复元素。
 */
typedef struct intset {
	// 编码方式(如果值为INTSET_ENC_INT16，则contents是一个int16_t类型的数组，数组里的每个项都是一个int16_t类型的整数值(最小值为-32768，最大值为32767)。...32(
	// 最小值为-2147483648，最大值为2147483647)。...64(最小值为-9223372036854775808，最大值为9223372036854775807)。)
    uint32_t encoding;
    // 集合包含的元素数量(即contents数组的长度)
    uint32_t length;
    // 保存元素的数组(是整数集合的底层实现：整数集合的每个元素contents数组的一个数据项(item)，各个项在数组中按值的大小从小到大有序地排列，并且数组中不包含任何重
    // 复项。虽然属性声明为int8_t，但实际上并不保存任何int8_t类型的值，该数组真正类型取决于encoding属性的值。)
    int8_t contents[];
} intset;

/*
 * 作用：创建一个新的压缩列表
 * Time complexity: O(1)
 */
intset *intsetNew(void);
/*
 * 作用：将给定元素添加到整数集合里面
 * Time complexity: O(N)
 */
intset *intsetAdd(intset *is, int64_t value, uint8_t *success);
/*
 * 作用：从整数集合中移除给定元素
 * Time complexity: O(N)
 */
intset *intsetRemove(intset *is, int64_t value, int *success);
/*
 * 作用：检查给定值是否存在于集合
 * Time complexity: 因为底层数组有序，查找可以通过二分查找法来进行，所以复杂度为O(logN)
 */
uint8_t intsetFind(intset *is, int64_t value);
/*
 * 作用：从整数集合中随机返回一个元素
 * Time complexity: O(1)
 */
int64_t intsetRandom(intset *is);
/*
 * 作用：取出底层数组在给定索引上的元素
 * Time complexity: O(1)
 */
uint8_t intsetGet(intset *is, uint32_t pos, int64_t *value);
/*
 * 作用：返回整数集合包含的元素个数
 * Time complexity: O(1)
 */
uint32_t intsetLen(intset *is);
/*
 * 作用：返回整数集合占用的内存字节数
 * Time complexity: O(1)
 */
size_t intsetBlobLen(intset *is);

#endif // __INTSET_H
