# Vdevs, Labels, and Boot Block

## Virtual Devices

ZFS storage pools are made up of a collection of virtual devices.
There are two types of virtual devices: physical virtual devices
(sometimes called leaf vdevs) 
and logical virtual devices
(sometimes called interior vdevs).
A physical vdev, 
is a writeable media block device (a disk, for example).
A logical vdev is a conceptual grouping of physical vdevs.

Vdevs are arranged in a tree with physical vdev existing as leaves of the tree.
All pools have a special logical vdev called the “root” vdev
which roots the tree.
All direct children of the “root” vdev (physical or logical) are called top-level vdevs.
@fig:vdev_sample shows a tree of vdevs
representing a sample pool configuration containing two mirrors.
The first mirror (labeled “M1”) contains two disk, 
represented by “vdev A” and “vdev B”.
Likewise, the second mirror “M2” contains two disks represented by “vdev C” and “vdev D”.
Vdevs A, B, C, and D are all physical vdevs.
“M1” and M2” are logical vdevs; 
they are also top-level vdevs since they originate from the “root vdev”.

```{.figure}
{
    "path"    : "Figures/zfs_vdev",
    "caption" : "Vdev Tree Sample Configuration",
    "label"   : "vdev_sample",
    "options" : "scale=1,center",
    "place"   : "ht"
}
```

## Vdev Labels

Each physical vdev within a storage pool contains a 256KB structure called a _vdev label_.
The vdev label contains information
describing this particular physical vdev and all other vdevs
which share a common top-level vdev as an ancestor. 
For example, 
the vdev label structure contained on vdev “C”, in the previous illustration, 
would contain information 
describing the following vdevs: 
"C", "D", and "M2". 
The contents of the vdev label are described in greater detail 
in @sec:vdev_detail, 
`\nameref{sec:vdev_detail}`{=latex}`Vdev Technical Details`{=html}.

The vdev label serves two purposes: 
it provides access to a pool's contents
and it is used to verify a pool's integrity and availability. 
To ensure that the vdev label is always available and always valid,
redundancy and a staged update model are used.
To provide redundancy, 
four copies of the label are written to each physical vdev within the pool. 
The four copies are identical within a vdev, 
but are not identical across vdevs in the pool.
During label updates,
a two staged transactional approach is used to ensure that
a valid vdev label is always available on disk.
Vdev label redundancy and the transactional update model are described in more detail below.

### Label Redundancy

Four copies of the vdev label are written to each physical vdev within a ZFS storage pool. 
Aside from the small time frame during label update (described below), 
these four labels are identical 
and any copy can be used to access and verify the contents of the pool.
When a device is added to the pool,
ZFS places two labels at the front of the device and two labels at the back of the device.
@fig:vdev_label shows the layout of these labels on a device of size N;
L0 and L1 represent the front two labels, 
L2 and L3 represent the back two labels.

```{.figure}
{
    "path"    : "Figures/zfs_vdev_label",
    "caption" : "Vdev Label layout on a block device of size N",
    "label"   : "vdev_label",
    "options" : "scale=1,center",
    "place"   : "ht"
}
```

Based on the assumption that 
corruption (or accidental disk overwrites) typically occurs in contiguous chunks,
placing the labels in non-contiguous locations (front and back) provides ZFS with a better probability
that some label will remain accessible in the case of media failure or accidental overwrite 
(e.g. using the disk as a swap device while it is still part of a ZFS storage pool).

### Transactional Two Staged Label Update

The location of the vdev labels are fixed 
at the time the device is added to the pool. 
Thus,
the vdev label does not have copy-on-write semantics like everything else in ZFS.
Consequently, 
when a vdev label is updated,
the contents of the label are overwritten. 
Any time on-disk data is overwritten,
there is a potential for error.
To ensure that ZFS always has access to its labels,
a staged approach is used during update. 
The first stage of the update writes the even labels (L0 and L2) to disk.
If, at any point in time,
the system comes down or faults during this update, 
the odd labels will still be valid.
Once the even labels have made it out to stable storage,
the odd labels (L1 and L3) are updated and written to disk.
This approach has been carefully designed to ensure that
a valid copy of the label remains on disk at all times.

## Vdev Technical Details {#sec:vdev_detail}

The contents of a vdev label are broken up into four pieces: 
8KB of blank space, 
8K of boot header information,
112KB of name-value pairs,
and 128KB of 1K sized uberblock structures. 
The drawing below shows an expanded view of the L0 label. 
A detailed description of each components follows:
blank space
(@sec:blankspace),
boot block header
(@sec:bootheader), 
name/value pair list
(@sec:nvlist), 
and 
uberblock array
array (@sec:ub).

### Blank Space{#sec:blankspace}

ZFS supports both VTOC (Volume Table of Contents) and EFI disk labels
as valid methods of describing disk layout.[^disklayout] 
While EFI labels are not written as part of a slice
(they have their own reserved space), 
VTOC labels must be written to the first 8K of slice 0.
Thus, 
to support VTOC labels, 
the first 8k of the vdev_label is left empty to prevent potentially overwriting a VTOC disk label.

[^disklayout]: Disk labels describe disk partition and slice information. 
See `fdisk`(1M) and/or `format`(1M) for more information on disk partitions and slices. 
It should be noted that disk labels are a completely separate entity from vdev labels
and while their naming is similar,
they should not be confused as being similar.

### Boot Block Header{#sec:bootheader}

The boot block header is an 8K structure that is reserved for future use.
The contents of this block will be described in a future appendix of this paper.

### Name-Value Pair List{#sec:nvlist}

The next 112KB of the label holds a collection of name-value pairs
describing this vdev and all of it's _related vdevs_. 
Related vdevs are defined as all vdevs within the subtree rooted at this vdev's top-level vdev. 
For example, 
the vdev label on device “A” 
(seen in @fig:vdev_sample) would contain information 
describing the subtree highlighted:
including vdevs “A”, “B”, and “M1” (top-level vdev).

All name-value pairs are stored in _XDR_ encoded nvlists. 
For more information on XDR encoding or nvlists, 
see the `libnvpair` (3LIB) and `nvlist_free` (3NVPAIR) man pages. 
The following name-value pairs (@tbl:nvpair_vdev_label) 
are contained within this 112KB portion of the vdev_label.

--------------------------------------------------------------------------------------------------
            Name          Value                  Description
--------    ------------- -------------------    -------------------------------------------------
Version     ``version”    DATA_TYPE_UINT64       On disk format version. 
                                                 Current value is “1” (5000?).
                                          
Name        ``name”       DATA_TYPE_STRING       Name of the pool in which this vdev belongs.

State       ``state”      DATA_TYPE_UINT64       State of this pool. The following table shows all 
                                                 existing pool states. The @tbl:pool_states shows
                                                 all existing pool states.

Tranaction  ``txg”        DATA_TYPE_UINT64       Transaction group number in which this label was 
                                                 written to disk.
                                             
Pool Guid   ``pool_guid”  DATA_TYPE_UINT64       Global unique identifier (guid) for the pool.

Top Guid    ``top_guid”   DATA_TYPE_UINT64       Global unique identifier for the top-level vdev 
                                                 of this subtree.
                                                
Vdev Tree   ``vdev_tree”  DATA_TYPE_NVLIST       The vdev_tree is a nvlist structure which is used 
                                                 recursively to describe the hierarchical nature of 
                                                 the vdev tree as seen in illustrations one and four.
                                                 The vdev\_tree recursively describes each “related” 
                                                 vdev witin this vdev's subtree. 
---------   ------------- -------------------    -------------------------------------------------

Table: Name-Value Pairs within vdev_label {#tbl:nvpair_vdev_label}

----------------------------------------
State                       Value
------------------------    ------------
POOL_STATE_ACTIVE           0

POOL_STATE_EXPORTED         1

POOL_STATE_DESTROYED        2
------------------------    ------------

Table: Pool States {#tbl:pool_states}

Each vdev_tree nvlist contains the elements as described in the @tbl:vdevtree_nvlist_entries.
Note that not all nvlist elements are applicable to all vdevs types. 
Therefore, 
a vdev_tree nvlist may contain only a subset of the elements described below.
The @fig:vdev_tree below shows what the ``vdev_tree” entry might look 
like for “vdev A” as shown in @fig:vdev_label
earlier in this document.

```{.figure}
{
    "path"    : "Figures/zfs_vdev_tree",
    "caption" : "Vdev Tree Nvlist Entries",
    "label"   : "vdev_tree",
    "options" : "scale=1,center",
    "place"   : "ht"
}
```

------------------------------------------------------------------------------------------------
Name               Value                           Description
------------------ ----------------------------    -------------------------------------------------
``type”            DATA_TYPE_UINT64                The id is the index of this vdev in its parent's 
                                                   children array.
                                                   
``guid”            DATA_TYPE_UINT64                Global Unique Identifier for this vdev_tree element.
                                                   
``path”            DATA_TYPE_STRING                Device path. Only used for leaf vdevs.
                                                   
``devid”           DATA_TYPE_STRING                Device ID for this vdev_tree element. Only used for 
                                                   vdevs of type disk.
                                                   
 “metaslab_array”  DATA_TYPE_UINT64                Object number of an object containing an array of object
                                                   numbers. Each element of this array (ma[i]) is, in turn,
                                                   an object number of a space map for metaslab 'i'.
                                                   
“metaslab_shift”   DATA_TYPE_UINT64                Log base 2 of the metaslab size.
                                                   
 “ashift”          DATA_TYPE_UINT64                Log base 2 of the minimum allocatable unit for this 
                                                   top level vdev. This is currently '10' for a RAIDz 
                                                   configuration, `9' otherwise.
                                                   
“asize”            DATA_TYPE_UINT64                Amount of space that can be allocated from this top level 
                                                   vdev
                                                   
 “children”        DATA_TYPE_NVLIST_ARRAY          Array of vdev_tree nvlists for each child of this 
                                                   vdev_tree element.
------------------ ----------------------------    --------------------------------------------------

Table: Vdev Tree NV List Entries {#tbl:vdevtree_nvlist_entries}

### The Uberblock{#sec:ub}

Immediately following the nvpair lists in the vdev label is an array of _uberblocks_. 
The uberblock is the portion of the label 
containing information necessary to access the contents of the pool[^similar].
Only one uberblock in the pool is active at any point in time. 
The uberblock with the highest transaction group number and valid SHA-256 checksum
is the active uberblock.

[^similar]: The uberblock is similar to the superblock in UFS.

To ensure constant access to the active uberblock,
the active uberblock is never overwritten.
Instead, all updates to an uberblock are done 
by writing a modified uberblock to another element of the uberblock array.
Upon writing the new uberblock,
the transaction group number and timestamps are incremented
thereby making it the new active uberblock in a single atomic action.
Uberblocks are written in a round robin fashion across the various vdevs with the pool.
The @fig:ub_expanded has an expanded view of two uberblocks within an uberblock array.

```{.figure}
{
    "path"    : "Figures/zfs_ub_expanded",
    "caption" : "Uberblock array showing uberblock contents",
    "label"   : "ub_expanded",
    "options" : "width=\\textwidth,center",
    "place"   : "ht"
}
```

#### Uberblock Technical Details

The uberblock is stored in the machine's native endian format and has the following contents:

* **ub_magic:**
  The uberblock magic number is a 64 bit integer 
  used to identify a device as containing ZFS data. 
  The value of the ub_magic is $0x00bab10c$ (oo-ba-block). 
  The @tbl:endianess_ub shows the ub_magic number as seen on disk.
  
  ---------------------------------------
  Machine Endianess      Uberblock Value
  -------------------    ----------------
  Big Endian             `0x00bab10c`
  
  Little Endian          `0x0cb1ba00`
  -------------------    ----------------
  
  Table: Uberblock values per machine endian type {#tbl:endianess_ub}
  
* **ub_version:**
  The version field is used to identify the on-disk format in which this data is laid out. 
  The current on-disk format version number is 0x1 (5000?). 
  This field contains the same value 
  as the “version” element of the name/value pairs described in @sec:nvlist.
  
* **ub_txg:**
  All writes in ZFS are done in transaction groups. 
  Each group has an associated transaction group number. 
  The ub_txg value reflects the transaction group in which
  this uberblock was written. 
  The ub_txg number must be greater than or equal to the “txg” number 
  stored in the nvlist for this label to be valid.

* **ub_guid_sum:**
  The ub_guid_sum is used to verify the availability of vdevs within a pool. 
  When a pool is opened, 
  ZFS traverses all leaf vdevs within the pool
  and totals a running sum of all the GUIDs
  (a vdev's guid is stored in the guid nvpair entry, see @sec:nvlist) it encounters. 
  This computed sum is checked against the ub_guid_sum 
  to verify the availability of all vdevs within this pool.
  
* **ub_timestamp:**
  Coordinated Universal Time (UTC) when
  this uberblock wasf written in seconds since January 1st 1970 (GMT).
  
* **ub_rootbp:**
  The ub_rootbp is a blkptr structure containing the location of the MOS. 
  Both the MOS and blkptr structures are described in later chapters of this document:
  Chapters 4 and 2 respectively.
  
## Boot Block

Immediately following the L0 and L1 labels is a 3.5MB chunk reserved for future use 
(see @fig:vdev_label).
The contents of this block will be described in a future appendix of this paper.
