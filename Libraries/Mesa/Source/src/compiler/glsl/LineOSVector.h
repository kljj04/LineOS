// Libraries/Mesa/Source/src/compiler/glsl/LineOSVector.h
// LineOS Project
// Copyright (C) 2026 LineOS Developer kljj04

#pragma once

#include <stddef.h>
#include <stdlib.h>

struct MESA_VECTOR_PLACEMENT
{
    void *Address;
};

inline void *operator new(size_t, MESA_VECTOR_PLACEMENT Placement) noexcept
{
    return Placement.Address;
}

template<typename Item>
class MesaVector
{
    Item *Data = nullptr;
    size_t Count = 0;
    size_t Capacity = 0;

public:
    MesaVector() = default;
    MesaVector(const MesaVector &) = delete;
    MesaVector &operator=(const MesaVector &) = delete;

    MesaVector(MesaVector &&Other) noexcept : Data(Other.Data), Count(Other.Count), Capacity(Other.Capacity)
    {
        Other.Data = nullptr;
        Other.Count = Other.Capacity = 0;
    }

    ~MesaVector()
    {
        while (Count != 0)
        {
            pop_back();
        }

        free(Data);
    }

    void push_back(Item &&Value)
    {
        if (Count == Capacity)
        {
            if (Capacity > (size_t) -1 / sizeof(Item) / 2)
            {
                abort();
            }

            size_t Next = Capacity == 0 ? 4 : Capacity * 2;
            Item *Storage = static_cast<Item *>(malloc(Next * sizeof(Item)));

            if (Storage == nullptr)
            {
                abort();
            }

            for (size_t Index = 0; Index < Count; Index++)
            {
                new (MESA_VECTOR_PLACEMENT{Storage + Index}) Item(static_cast<Item &&>(Data[Index]));
                Data[Index].~Item();
            }

            free(Data);
            Data = Storage;
            Capacity = Next;
        }

        new (MESA_VECTOR_PLACEMENT{Data + Count}) Item(static_cast<Item &&>(Value));
        Count++;
    }

    void pop_back() { Data[--Count].~Item(); }
    Item &back() { return Data[Count - 1]; }
    size_t size() const { return Count; }
    bool empty() const { return Count == 0; }
    const Item *begin() const { return Data; }
    const Item *end() const { return Count == 0 ? Data : Data + Count; }
    Item *end() { return Count == 0 ? Data : Data + Count; }
};
