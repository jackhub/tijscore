/**
 * Appcelerator Titanium License
 * This source code and all modifications done by Appcelerator
 * are licensed under the Apache Public License (version 2) and
 * are Copyright (c) 2009-2014 by Appcelerator, Inc.
 */

/*
 *  Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
#ifndef Bitmap_h
#define Bitmap_h

#include <array>
#include <wtf/Atomics.h>
#include <wtf/StdLibExtras.h>
#include <stdint.h>
#include <string.h>

namespace WTI {

enum BitmapAtomicMode {
    // This makes concurrentTestAndSet behave just like testAndSet.
    BitmapNotAtomic,

    // This makes concurrentTestAndSet use compareAndSwap, so that it's
    // atomic even when used concurrently.
    BitmapAtomic
};

template<size_t size, BitmapAtomicMode atomicMode = BitmapNotAtomic, typename WordType = uint32_t>
class Bitmap {
public:
    Bitmap();

    bool get(size_t) const;
    void set(size_t);
    bool testAndSet(size_t);
    bool testAndClear(size_t);
    bool concurrentTestAndSet(size_t);
    bool concurrentTestAndClear(size_t);
    size_t nextPossiblyUnset(size_t) const;
    void clear(size_t);
    void clearAll();
    int64_t findRunOfZeros(size_t) const;
    size_t count(size_t = 0) const;
    size_t isEmpty() const;
    size_t isFull() const;

private:
    static const unsigned wordSize = sizeof(WordType) * 8;
    static const unsigned words = (size + wordSize - 1) / wordSize;

    // the literal '1' is of type signed int.  We want to use an unsigned
    // version of the correct size when doing the calculations because if
    // WordType is larger than int, '1 << 31' will first be sign extended
    // and then casted to unsigned, meaning that set(31) when WordType is
    // a 64 bit unsigned int would give 0xffff8000
    static const WordType one = 1;

    std::array<WordType, words> bits;
};

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline Bitmap<size, atomicMode, WordType>::Bitmap()
{
    clearAll();
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<size, atomicMode, WordType>::get(size_t n) const
{
    return !!(bits[n / wordSize] & (one << (n % wordSize)));
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<size, atomicMode, WordType>::set(size_t n)
{
    bits[n / wordSize] |= (one << (n % wordSize));
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<size, atomicMode, WordType>::testAndSet(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool result = bits[index] & mask;
    bits[index] |= mask;
    return result;
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<size, atomicMode, WordType>::testAndClear(size_t n)
{
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    bool result = bits[index] & mask;
    bits[index] &= ~mask;
    return result;
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<size, atomicMode, WordType>::concurrentTestAndSet(size_t n)
{
    if (atomicMode == BitmapNotAtomic)
        return testAndSet(n);

    ASSERT(atomicMode == BitmapAtomic);
    
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    WordType* wordPtr = bits.data() + index;
    WordType oldValue;
    do {
        oldValue = *wordPtr;
        if (oldValue & mask)
            return true;
    } while (!weakCompareAndSwap(wordPtr, oldValue, oldValue | mask));
    return false;
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline bool Bitmap<size, atomicMode, WordType>::concurrentTestAndClear(size_t n)
{
    if (atomicMode == BitmapNotAtomic)
        return testAndClear(n);

    ASSERT(atomicMode == BitmapAtomic);
    
    WordType mask = one << (n % wordSize);
    size_t index = n / wordSize;
    WordType* wordPtr = bits.data() + index;
    WordType oldValue;
    do {
        oldValue = *wordPtr;
        if (!(oldValue & mask))
            return false;
    } while (!weakCompareAndSwap(wordPtr, oldValue, oldValue & ~mask));
    return true;
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<size, atomicMode, WordType>::clear(size_t n)
{
    bits[n / wordSize] &= ~(one << (n % wordSize));
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline void Bitmap<size, atomicMode, WordType>::clearAll()
{
    memset(bits.data(), 0, sizeof(bits));
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline size_t Bitmap<size, atomicMode, WordType>::nextPossiblyUnset(size_t start) const
{
    if (!~bits[start / wordSize])
        return ((start / wordSize) + 1) * wordSize;
    return start + 1;
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline int64_t Bitmap<size, atomicMode, WordType>::findRunOfZeros(size_t runLength) const
{
    if (!runLength) 
        runLength = 1; 
     
    for (size_t i = 0; i <= (size - runLength) ; i++) {
        bool found = true; 
        for (size_t j = i; j <= (i + runLength - 1) ; j++) { 
            if (get(j)) {
                found = false; 
                break;
            }
        }
        if (found)  
            return i; 
    }
    return -1;
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline size_t Bitmap<size, atomicMode, WordType>::count(size_t start) const
{
    size_t result = 0;
    for ( ; (start % wordSize); ++start) {
        if (get(start))
            ++result;
    }
    for (size_t i = start / wordSize; i < words; ++i)
        result += WTI::bitCount(bits[i]);
    return result;
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline size_t Bitmap<size, atomicMode, WordType>::isEmpty() const
{
    for (size_t i = 0; i < words; ++i)
        if (bits[i])
            return false;
    return true;
}

template<size_t size, BitmapAtomicMode atomicMode, typename WordType>
inline size_t Bitmap<size, atomicMode, WordType>::isFull() const
{
    for (size_t i = 0; i < words; ++i)
        if (~bits[i])
            return false;
    return true;
}

}
#endif
