# Block Pointers and Indirect Blocks {#sec:blkptr}

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

Gang blocks are identified by the “G” bit.

-----------------------------------
**“G” bit value**  **Description**
---------------    ----------------
0                  non-gang block

1                  gang block

-----------------------------------

Table: Gang Block Values {#tbl:gang_values}

Gang blocks are 512 byte sized,
self checksumming blocks.
A gang block contains up to 3 block pointers
followed by a 32 byte checksum.
The format of the gang block is described by the following structures.

```C
typedef struct zio_gbh {
	blkptr_t		zg_blkptr[SPA_GBH_NBLKPTRS];
	uint64_t		zg_filler[SPA_GBH_FILLER];
	zio_eck_t		zg_tail;
} zio_gbh_phys_t;
```

zg_blkptr`~`{=latex}:
  ~ Array of block pointers. Each 512 byte gang block can hold up to 3 block pointers.

zg\_filler`~`{=latex}:
  ~ The filler fields pads out the gang block so that it is nicely byte aligned.

```C
typedef struct zio_eck {
	uint64_t	zec_magic;
	zio_cksum_t	zec_cksum;
} zio_eck_t;
```

zec_magic:
  ~ ZIO block tail magic number.
    The value is `0x210da7ab10c7a11` (zio-data-bloc-tail).

```C
typedef struct zio_cksum {
	uint64_t	zc_word[4];
} zio_cksum_t;
```

zc_word:
  ~ Four 8 byte words containing the checksum for this gang block.

## Checksum

By default ZFS checksums all of its data and metadata.
ZFS supports several algorithms for checksumming including fletcher2, fletcher4,
and SHA-256 (256-bit Secure Hash Algorithm in FIPS 180-2,
available at [http://csrc.nist.gov/cryptval](http://csrc.nist.gov/cryptval)).
The algorithm used to checksum this block
is identified by the 8 bit integer stored in the cksum portion of the block pointer. The
following table pairs each integer with a description and algorithm
used to checksum this block's contents.

---------------------------------------------
**Description**    **Value**    **Algorithm**
----------------- -----------  --------------
on                  1           fletcher2

off                 2                none

label               3             SHA-256

gang header         4             SHA-256

zilog               5           fletcher2

fletcher2           6           fletcher2

fletcher4           7           fletcher4

SHA-256             8             SHA-256

zilog2              9

noparity           10

SHA-512            11

skein              12

---------------------------------------------

Table: Checksum Values and associated algorithms {#tbl:chksum_values}

A 256 bit checksum of the data is computed for each block
using the algorithm identified in cksum.
If the cksum value is 2 (off),
a checksum will not be computed
and checksum[0], checksum[1], checksum[2], and checksum[3] will be zero.
Otherwise,
the 256 bit checksum computed for this block is stored
in the checksum[0], checksum[1], checksum[2], and checksum[3] fields.

_Note:
The computed checksum is always of the data,
even if this is a gang block.
Gang blocks (see above) and zilog blocks (see Chapter 8) are self checksumming._

## Compression

ZFS supports several algorithms for compression.
The type of compression used to compress this block is stored
in the comp portion of the block pointer.

-----------------------------------------------
**Description**      **Value**    **Algorithm**
-----------------   ----------   --------------
on                  1                lz4

off                 2                none

lzjb                3                lzjb

empty               4                empty

gzip level 1\~9     5\~13            gzip1\~9

zle                 14               zle

lz4                 15               lz4

zstd                16               zstd

------------------------------------------------

Table: Compression Values and associated algorithms {#tbl:comp_values}

## Block Size

The size of a block is described by three different fields in the block pointer;
_psize_, _lsize_, and _asize_.

lsize:
  ~ Logical size. The size of the data without compression, raidz or gang overhead.

psize:
  ~ Physical size of the block on disk after compression.

asize:
  ~ Allocated size,
    total size of all blocks allocated to hold this data
    including any gang headers or raid-Z parity information

If compression is turned off and ZFS is not on Raid-Z storage,
lsize, asize, and psize will all be equal.

All sizes are stored as the number of 512 byte sectors (minus one)
needed to represent the size of this block.

## Endian

ZFS is an adaptive-endian filesystem
(providing the restrictions described in Chapter One)
that allows for moving pools across machines with different architectures:
little endian vs. big endian.
The “E” portion of the block pointer indicates
which format this block has been written out in.
Block are always written out in the machine's native endian format.

----------------------------
**Endian**         **Value**
---------------   ----------
Little Endian            1

Big Endian               2

----------------------------

Table: Endian Values {#tbl:endian_values}

If a pool is moved to a machine with a different endian format,
the contents of the block are byte swapped on read.

## Type

The _type_ portion of the block pointer indicates what type of data this block holds.
The type can be the following values.
More detail is provided in chapter @sec:dmu regarding object types.

---------------------------------------------------
\centering                                **Value**
**Type**
---------------------------------------  ----------
DMU_OT_NONE                                0

DMU_OT_OBJECT_DIRECTORY                    1

DMU_OT_OBJECT_ARRAY                        2

DMU_OT_PACKED_NVLIST                       3

DMU_OT_NVLIST_SIZE                         4

DMU_OT_BPLIST                              5

DMU_OT_BPLIST_HDR                          6

DMU_OT_SPACE_MAP_HEADER                    7

DMU_OT_SPACE_MAP                           8

DMU_OT_INTENT_LOG                          9

DMU_OT_DNODE                              10

DMU_OT_OBJSET                             11

DMU_OT_DSL_DATASET                        12

DMU_OT_DSL_DATASET_CHILD_MAP              13

DMU_OT_OBJSET_SNAP_MAP                    14

DMU_OT_DSL_PROPS                          15

DMU_OT_DSL_OBJSET                         16

DMU_OT_ZNODE                              17

DMU_OT_ACL                                18

DMU_OT_PLAIN_FILE_CONTENTS                19

DMU_OT_DIRECTORY_CONTENTS                 20

DMU_OT_MASTER_NODE                        21

DMU_OT_DELETE_QUEUE                       22

DMU_OT_ZVOL                               23

DMU_OT_ZVOL_PROP                          24

---------------------------------------------------

Table: Object Types {#tbl:obj_types}

## Level

The _level_ portion of the block pointer is the number of levels
(number of block pointers
which need to be traversed to arrive at this data.)
See Chapter @sec:dmu for a more complete definition of level.

## Fill

The _fill_ count describes the number of non-zero block pointers under this block pointer.
The fill count for a data block pointer is 1,
as it does not have any block pointers beneath it.
The fill count is used slightly differently for block pointers of type DMU_OT_DNODE.
For block pointers of this type,
the fill count contains the number of free dnodes beneath this block pointer.
For more information on dnodes see Chapter @sec:dmu.

## Birth Transaction

The birth transaction stored in the “physical birth txg” and “logical birth txg”
block pointer field is a 64 bit integer
containing the transaction group number
in which this block pointer was allocated.

## Padding

The two padding fields in the block pointer are space reserved for future use.
