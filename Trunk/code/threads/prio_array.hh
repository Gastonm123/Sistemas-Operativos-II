#ifndef NACHOS_PRIO_ARRAY__HH
#define NACHOS_PRIO_ARRAY__HH

#include "lib/bitmap.hh"
#include "lib/list.hh"
#include "lib/utility.hh"

#include <stdio.h>

/// Maximum array priority
static const unsigned MAX_PRIO = 140;

template<class Item>
class PrioArray {
public:
    /// Initialize `PrioArray`.
    PrioArray();

    /// De-initialize `PrioArray`.
    ~PrioArray();

    /// Pop the highest priority item on the array.
    Item Pop();

    /// Append a item to the list with `priority`.
    ///
    /// * `item` is the item to be put on the array.
    /// * `priority` is the item priority.
    void Append(Item item, unsigned priority);

    /// Remove item from `PrioArray`.
    ///
    /// * `item` is the item to be removed from the array.
    /// * `priority` is the item priority.
    void Remove(Item item, unsigned priority);

    /// Print the contents of `PrioArray`.
    ///
    /// * `ItemPrint` is a function for printing items.
    void Print(void (*ItemPrint)(Item)) const;

    /// Return `true` if the array is empty.
    bool IsEmpty() const;

private:
    /// List of queues for every priority.
    List<Item> *queue;

    /// Bitmap of priorities in the array.
    Bitmap       *bitmap;
};

/// Initialize `PrioArray`.
template<class Item>
PrioArray<Item>::PrioArray()
{
    queue  = new List<Item>[MAX_PRIO];
    bitmap = new Bitmap(MAX_PRIO);
}

/// De-initialize `PrioArray`.
template<class Item>
PrioArray<Item>::~PrioArray()
{
    delete[] queue;
    delete   bitmap;
}

/// Pop the highest priority item on the array.
template<class Item>
Item
PrioArray<Item>::Pop()
{
    unsigned idx;
    if ((idx = bitmap->FindFirstBit())) {
        idx -= 1;
        Item highest = queue[idx].Pop();
        if (queue[idx].IsEmpty())
            bitmap->Clear(idx);
        return highest;
    }
    return nullptr;
}

/// Append a item to the list with `priority`.
///
/// * `item` is the item to be put on the array.
/// * `priority` is the item priority.
template<class Item>
void
PrioArray<Item>::Append(Item item, unsigned priority)
{
    ASSERT(priority < MAX_PRIO);
    queue[priority].Append(item);
    bitmap->Mark(priority);
}

/// Remove item with `priority`.
///
/// * `item` is the item to be removed from the array.
/// * `priority` is the item priority.
template<class Item>
void
PrioArray<Item>::Remove(Item item, unsigned priority)
{
    ASSERT(priority < MAX_PRIO);
    queue[priority].Remove(item);
    if (queue[priority].IsEmpty())
        bitmap->Clear(priority);
}

/// Return true if `PrioArray` is empty
template<class Item>
bool
PrioArray<Item>::IsEmpty() const
{
    return (bitmap->FindFirstBit() == 0);
}

/// Print the contents of `PrioArray`.
template<class Item>
void
PrioArray<Item>::Print(void (*ItemPrint)(Item)) const
{
    for (unsigned prio = 0; prio < MAX_PRIO; prio++) {
        if (!(queue[prio].IsEmpty())) {
            printf("\n[%d] ", prio);
            queue[prio].Apply(ItemPrint);
        }
    }
}

#endif
