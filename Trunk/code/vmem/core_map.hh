#ifndef NACHOS_VMEM_CORE_MAP__HH
#define NACHOS_VMEM_CORE_MAP__HH

#include "lib/list.hh"

class Thread;

/// Main memory information about paging.
///
/// Each main memory frame pertain to an unique process and have an unique page
/// number within that process.
class CoreMapEntry {
public:
    /// Process to which this frame pertains.
    unsigned tid; 

    /// Page number within `tid` address space.
    unsigned vpn;
};

/// Virtual memory functionality.
///
/// A clock algorithm is used to select the next frame to be evicted from main
/// memory and moved into swap area.
namespace CoreMap {
    /// Selects and moves a frame from main memory to swap.
    ///
    /// Returns the frame number.
    unsigned MoveFrameToSwap();

    /// Finds an empty frame. If there is none, moves a frame to swap.
    ///
    /// Returns an empty-frame number.
    unsigned Find();
};

#endif
