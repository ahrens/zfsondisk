# Block Pointers and Indirect Blocks

Data is transferred between disk and main memory in units called blocks.
A block pointer (`blkptr_t`) is a 128 byte ZFS structure used 
to physically locate, verify, and describe blocks of data on disk.

The 128 byte blkptr_t structure layout is shown in the @fig:blkptr.

```{.figure}
{
    "path"    : "Figures/zfs_blkptr",
    "caption" : "Block pointer structure showing byte by byte usage.",
    "label"   : "blkptr",
    "options" : "scale=1,center",
    "place"   : "ht"
}
```

Normally,
block pointers point (via their DVAs) to a block which holds data.
If the data that we need to store is very small,
this is an inefficient use of space,
Additionally, reading these small blocks tends to generate
more random reads.
Embedded-data Block Pointers was introduced.
It allows small pieces of data
(the "payload", upto 112 bytes) embedded in the block pointer,
the block pointer doesn't point to anything then.
The layout of an embedded block pointer is as @fig:embedded.

```{.figure}
{
    "path"    : "Figures/zfs_embedded_blkptr",
    "caption" : "Embedded Block Pointer Layout",
    "label"   : "embedded",
    "options" : "scale=1,center",
    "place"   : "ht"
}
```

## Data Virtual Address

The _data virtual address_, or _DVA_ is the name 
given to the combination of the vdev and offset portions of the block pointer,
for example the combination of vdev1 and offset1 make up a DVA (dva1).
ZFS provides the capability of storing up to three copies of the data pointed to by the block pointer,
each pointed to by a unique DVA (dva1, dva2, or dva3). 
The data stored in each of these copies is identical.
The number of DVAs used per block pointer is purely a policy decision
and is called the “wideness” of the block pointer:
single wide block pointer (1 DVA), 
double wide block pointer (2 DVAs), 
and triple wide block pointer (3 DVAs).

The _vdev_ portion of each DVA is a 32 bit integer 
which uniquely identifies the vdev ID containing this block. 
The offset portion of the DVA is a 63 bit integer value
holding the offset (starting after the vdev labels (L0 and L1) and boot block)
within that device where the data lives. 
Together, 
the vdev and offset uniquely identify the block address of the data it points to.

The value stored in offset is the offset in terms of sectors (512 byte blocks).
To find the physical block byte offset from the beginning of a slice,
the value inside offset must be shifted over ($\ll$) by $9$ ($2^9 =512$)
and this value must be added to $0x400000$
(size of two vdev_labels and boot block).

$$
physical\ block\ address = (\mathit{offset} \ll 9) + 0x400000~(4MB)
$$

## GRID

Raid-Z layout information, reserved for future use.

## GANG

A _gang block_ is a block whose contents contain block pointers. 
Gang blocks are used when the amount of space requested is not available in a contiguous block.
In a situation of this kind,
several smaller blocks will be allocated
(totaling up to the size requested) 
and a gang block will be created to contain the block pointers for the allocated blocks.
A pointer to this gang block is returned to the requester, 
giving the requester the perception of a single block.
