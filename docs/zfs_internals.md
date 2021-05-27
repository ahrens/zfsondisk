---
title:	ZFS On-disk Format
author:	Luke Huang \<bwonder475@tutanota.com\>
documentclass:	memoir
lang:	en-US
secPrefix: Section.
tblPrefix: Table.
figPrefix: Figure.
---

`<p><a href="zfs_internals.md.pdf">Click here</a> for a printer friendly PDF version.</p>`{=html}

#   On-disk Format

##  Vdev

ZFS storage pools are essentially a collection of _virtual devices_, or _vdevs_,
which are arranged in a tree
with physical vdevs sitting at leaves.
@fig:vdev illustrates such a tree representing
a sample pool configuration containing two mirrors,
each of which has two disks.

```{.figure}
{
    "path"    : "Figures/zfs_vdev",
    "caption" : "Vdev Tree",
    "label"   : "vdev",
    "options" : "scale=1,center",
    "place"   : "ht"
}
```

##  Label and Uberblock

Each physical vdev within a storage pool contains four identical copies of 256KB structure
called a vdev _label_ (`vdev_label_t`).
The vdev label
contains information describing this physical vdev
and all other vdevs which share a common top-level vdev as an ancestor.
For example,
in @fig:vdev,
the vdev label on D3
would contain information describing D3, D4, and M2.
Any copy of the labels can be used to access and verify the pool.
There are two labels at the front of the device
and two labels at the back.
@fig:label illustrates the physical layout of vdev labels.

```{.figure}
{
    "path"    : "Figures/zfs_label",
    "caption" : "From label to MOS",
    "label"   : "label",
    "options" : "width=\\textwidth",
    "place"   : "ht"
}
```

An _uberblock_ (`uberblock_t`) contains information
necessary to access the contents of the pool.
Only one uberblock in the pool is _active_ at any time.
The uberblock with the highest transaction group number
and valid checksum is the active uberblock.
@fig:ub illustrates the layout of an active uberblock,
and how accessing the storage pool can start from it.

```{.figure}
{
    "path"    : "Figures/zfs_ub",
    "caption" : "From Uberblock to MOS",
    "label"   : "ub",
    "options" : "width=\\textwidth",
    "place"   : "ht"
}
```

##  Block Pointer

Data is transferred between disk and main memory in units called blocks.
A block pointer (`blkptr_t`) is a 128 byte ZFS structure
used to physically locate, verify, and describe blocks of data on disk.
The layout of a normal blkptr is shown as @fig:blkptr.
Note that,
the size of a block is described by three different fields:
_psize_, _lsize_, and _asize_.

```{.figure}
{
    "path"    : "Figures/zfs_blkptr",
    "caption" : "Block Pointer Layout",
    "label"   : "blkptr",
    "options" : "scale=.8,center",
    "place"   : "ht"
}
```

* _lsize_: Logical size.
  The size of the data without compression, raidz or gang overhead.
* _psize_: Physical size of the block on disk after compression
* _asize_: Allocated size,
  total size of all blocks allocated to hold this data
  including any gang headers or raid-Z parity information

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
    "options" : "scale=.8,center",
    "place"   : "ht"
}
```

##  DMU

The _Data Management Unit_,
or _DMU_ groups blocks into logical units
called _objects_.
Almost everything in ZFS is an object.
Objects are defined by 512 bytes structures called _dnodes_ (`dnode_phys_t`).
A dnode describes and organizes a collection of blocks making up an object,
for example,
`dn_type` determines the type of the object,
`dn_blkptr` stores the block pointers for block addressing.
A dnode has a limited number (`dn_nblkptr`) of block pointers to describe an object's data.
For a dnode using the largest data block size (128KB)
and containing the maximum number of block pointers (3),
the largest object size it can represent is 384 KB.
To allow larger objects,
indirect blocks are introduced,
the largest indirect block (128KB) can hold up to 1024 block pointers,
so that 384MB object can be represented without the next level of indirection.
The `dn_nlevel` field tells total levels of addressing.
@fig:indirect illustrates a 3-levels indirect addressing.

```{.figure}
{
    "path"    : "Figures/zfs_indirect",
    "caption" : "Indirect block addressing",
    "label"   : "indirect",
    "options" : "scale=1,center",
    "place"   : "ht"
}
```

Related objects can be further grouped by the DMU into _object sets_.
ZFS allows users to create four kinds of object sets:
_filesystems_, _clones_, _snapshots_, and _volumes_.

##  DSL

The _Dataset and Snapshot Layer_, or _DSL_
is for describing
and managing relationships-between and properties-of object sets.

Each object set is represented in the DSL as a _dataset_ (`dsl_dataset_phys_t`).
Datasets are grouped into collections called _Dataset Dircectories_,
which manages a related grouping of datasets
and the properties associated with that grouping.
A DSL directory always has exactly one _active_ dataset.
All other datasets under the directory are related to the active dataset
through _snapshots_, _clones_, or _child/parent dependencies_.

##  MOS

The DSL is implementd as an object set of type `DMU_OST_META`,
which is often called the _Meta Object Set_, or _MOS_.
There is a single distinguished object in the Meta Object Set,
called the _object directory_.
Object directory is always located in the $1^{st}$ element of the dnode array
(index starts from $0$).
All other objects can be located by traversing
through a set of object references starting at this object.

The object directory is implemented as a _ZAP_ object
that is made up of name/value pairs.
The object directory contains
_root\_dataset_, _config_, _free\_bpobj_,
and some other attribute pairs.

@fig:mos illustrates a realistic layout of the MOS of a sample pool,
from which a data set (e.g., file system) can be accessed.

```{.figure}
{
    "path"    : "Figures/zfs_mos",
    "caption" : "MOS",
    "label"   : "mos",
    "options" : "width=\\textwidth",
    "place"   : "ht"
}
```

@fig:fs illustrates the path from a data set object to
user data (contents of the sample file `/sbin/zdump`).

```{.figure}
{
    "path"    : "Figures/zfs_fs",
    "caption" : "From the data set to user data",
    "label"   : "fs",
    "options" : "width=\\textwidth",
    "place"   : "ht"
}
```

##  ZAP

The _ZFS Attributes Processors_, or _ZAPs_ are objects
used to store attributes in the form of name-value pairs.
ZAPs come in two forms:
_micro ZAP_ for small number of attributes
and _fat ZAP_ for large number of attributes.
Both of them are arranged
based on hash of the attribute's name.
Directories in ZFS are implemented as ZAP objects.

#  On-Disk Data Walk (Or: Where's My Data)

This part (title and content) was inspired by
Max Bruning's great demostration --
[ZFS On-Disk Data Walk (Or: Where's My Data)](http://www.osdevcon.org/2008/files/osdevcon2008-max.pdf)
, and even better,
his
[training material](https://www.yumpu.com/en/document/view/3947186/zfs-on-disk-data-walk-or-wheres-my-data-bruning-systems).
He used the modified[^mod] `mdb`, `zdb`,
and `dd` to read and dump ZFS data structure from physical devices
to illustrate the ZFS on-disk layout,
from vdev label to content of a file.

There is not `mdb` equivalent on Linux,
and I don't want to switch among tools from time to time,
so that I wrote a simple tool to do all the things,
reading (via `open` and `read` system calls),
decompressing (by calling _liblz4_ functions) data from the physical vdev,
and dumping the ZFS physical data structures as JSON format.
It doesn't call any function of the ZFS libraries.
A few helpers are still used
because I was too lazy to write my own,
perhaps I will remove all of them in the future.
However the core functions
such as `spa_xxxx`, `dmu_xxxx`, `dsl_xxxx`, `zio_xxxx`, are avoided.

[^mod]: by himself `👍`{=html}`\lower.6ex\hbox{\includegraphics{Figures/1F44D}}`{=latex}

## Environment

```bash
$ cat /etc/os-release
NAME="Ubuntu"
VERSION="20.04.2 LTS (Focal Fossa)"
.... output omitted ....
$ sudo apt install libzpool2linux libzfs2linux libzfslinux-dev
$ sudo apt install libnvpair1linux libjson-c-dev liblz4-dev
```

## Preparation

#. Clone and build `zdump` tool.
#. Create a new file system, with a very simple hierarchy.
   Note that,
   as the `zdump` tool only supports lz4,
   the default compression algorithm of ZFS on Linux,
   don't set the `compression` property for creating ZFS.

   ```bash
   $ sudo zfs create -V 4G dpool/zvol0
   $ sudo fdisk -l /dev/zd0 | head -1
   Disk /dev/zd0: 4 GiB, 4294967296 bytes, 8388608 sectors
   $ sudo dmsetup create zdisk0 --table '0 8388607 linear /dev/zd0 0'
   $ sudo mkdir /mnt/zwalk
   $ sudo zpool create -f -m /mnt/zwalk zwalk /dev/mapper/zdisk0
   $ sudo zfs list
   NAME         USED  AVAIL     REFER  MOUNTPOINT
   zwalk        840K  3.62G      192K  /mnt/zwalk
   $ sudo mkdir /mnt/zwalk/{sbin,var,usr}
   $ sudo su -c 'printf "#!/bin/bash\n\necho Hello ZFS\n" >/mnt/zwalk/sbin/zdump'
   $ cat /mnt/zwalk/sbin/zdump
   #!/bin/bash

   echo Hello ZFS
   ```

   To clean up after the walk:

   ```bash
   $ sudo zpool destroy zwalk
   $ sudo dmsetup remove zdisk0
   $ sudo zfs destroy dpool/zvol0
   ```

#. Add the user into `disk` group
   so that `sudo` is not needed to read the block device file.

   ```bash
   $ sudo usermod -aG disk $(whoami)
   ```

## Walk the data.

#. The first step is dumping the label and active uberblock.
   Note that the `offset`, `psize`, and `lsize` are hexadecimal.

   ```bash
   $ zdump --label /dev/mapper/zdisk0:0:40000/40000
   {
     "Vdev Label":{
       "name":"zwalk",
       "version":5000,
       "uberblock":{
         "magic":"0x0000000000bab10",
         "version":5000,
         "txg":10,
         "rootbp":{
           "vdev":"0",
           "offset":"3801e000",
           "asize":"2000",
           "psize":"1000",
           "lsize":"1000",
           "compressed":"uncompressed"
         }
       }
     }
   }
   ```

#. Dump dnode of the MOS,
   using the `offset` ($3801e000$),
   `psize` ($1000$), and `lsize` ($1000$) we got from the previous step.

   ```bash
   $ zdump --mos /dev/mapper/zdisk0:"3801e000":1000/1000
   {
     "MOS":{
       "os_type":"META",
       "dnonde":{
         "dn_type":"DMU dnode",
         "dn_bonustype":0,
         "dn_indblkshift":17,
         "dn_nlevels":2,
         "dn_nblkptr":3,
         "dn_blkptr":[
           {
             "vdev":"0",
             "offset":"4e000",
             "asize":"2000",
             "psize":"2000",
             "lsize":"20000",
             "compressed":"lz4"
           },
           .... output omitted ....
         ]
       }
     }
   }
   ```

#. From output of the previous step,
   we notice that the MOS uses $2$-levels indirect addressing (`dn_nlevels` was $2$[^zvol]),
   so we need to find the $0^{th}$ level block pointer to access to the data block.
   The `lsize` of each block pointer is $16$K,
   that can contain $32$ dnodes.

   [^zvol]: If we create the ZVOL with `-s` option,
   there will only one level of block pointer.

   ```bash
   $ zdump --indirect-blkptr /dev/mapper/zdisk0:"4e000":20000/2000:2
   {
     "[L0]":[
       {
         "vdev":"0",
         "offset":"28008000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"4000",
         "compressed":"lz4"
       },
       {
         "vdev":"0",
         "offset":"2802c000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"4000",
         "compressed":"lz4"
       },
       {
         "vdev":"0",
         "offset":"0",
         "asize":"0",
         "psize":"200",
         "lsize":"200",
         "compressed":"inherit"
       },
       {
         "vdev":"0",
         "offset":"0",
         "asize":"0",
         "psize":"200",
         "lsize":"200",
         "compressed":"inherit"
       },
       {
         "vdev":"0",
         "offset":"28006000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"4000",
         "compressed":"lz4"
       },
       .... output omitted ....
     ]
   }
   ```

#. Dump the MOS object directory,
   which is the $1^{st}$ object (the $0^{th}$ is not used) in the dnode array.
   The MOS is a fat ZAP object,
   whose entries will be dumped as well as its dnode.
   We will use the `root_dataset` object to move forward.

   ```bash
   $ zdump --mos-objdir /dev/mapper/zdisk0:"28008000":4000/2000
   {
     "dnode":{
       "dn_type":"object directory",
       "dn_bonustype":0,
       "dn_indblkshift":17,
       "dn_nlevels":1,
       "dn_nblkptr":3,
       "dn_blkptr":[
         {
           "vdev":"0",
           "offset":"10000",
           "asize":"2000",
           "psize":"2000",
           "lsize":"4000",
           "compressed":"lz4"
         },
         {
           "vdev":"0",
           "offset":"12000",
           "asize":"2000",
           "psize":"2000",
           "lsize":"4000",
           "compressed":"lz4"
         },
         {
           "vdev":"0",
           "offset":"0",
           "asize":"0",
           "psize":"200",
           "lsize":"200",
           "compressed":"inherit"
         }
       ]
     },
     "FZAP":{
       "zap_block_type":"ZBT_HEADER",
       "zap_magic":"0x00000002f52ab2a",
       "zap_num_entries":13,
       "zap_table_phys":{
         "zt_blk":0
       }
     },
     "FZAP leaf":{
       "entries":[
         {
           "name":"root_dataset",
           "value":32
         },
         .... output omitted ....
         {
           "name":"config",
           "value":61
         },
         .... output omitted ....
       ]
     }
   }
   ```

#. Dump the root data set.
   From the previous step,
   we knew that it's the $32^{nd}$ item in the dnode array,
   therefore,
   we seek it in the $1^{st}$ block (with the offset $2802c000$).
   `dsl_dir_phys_t` is stored
   in the `dn_bonus` field of dnode of the root data set object.
   We can see that the head data set object is the $54^{th}$ object,
   located in the same block as the root data set's.

   ```bash
   $ zdump --mos-rootds /dev/mapper/zdisk0:"2802c000":4000/2000:32
   {
     "dnode":{
       "dn_type":"DSL directory",
       "dn_bonustype":12,
       "dn_indblkshift":17,
       "dn_nlevels":1,
       "dn_nblkptr":1,
       "dn_blkptr":[
         {
           "vdev":"0",
           "offset":"0",
           "asize":"0",
           "psize":"200",
           "lsize":"200",
           "compressed":"inherit"
         }
       ]
     },
     "DSL":{
       "dd_creation_time":"....",
       "dd_child_dir_zapobj":34,
       "dd_head_dataset_obj":54
     }
   }
   ```

#. Dump the childmap and head data set.
   The later will be used to move forward to ZPL.

   ```bash
   $ zdump --mos-childmap /dev/mapper/zdisk0:"2802c000":4000/2000:34
   {
     "Embedded Block Pointer":{
       "type":0,
       "psize":78,
       "lsize":512,
       "compressed":"lz4",
       "Micro ZAP":[
         {
           "mze_name":"$MOS",
           "mze_value":35
         },
         {
           "mze_name":"$FREE",
           "mze_value":38
         },
         {
           "mze_name":"$ORIGIN",
           "mze_value":42
         }
       ]
     }
   }
   $ zdump --mos-headds /dev/mapper/zdisk0:"2802c000":4000/2000:54
   {
     "Head data set":{
       "ds_dir_obj":32,
       "ds_creation_time":"Tue May 18 08:51:28",
       "ds_create_txg":1,
       "ds_bp":{
         "vdev":"0",
         "offset":"8028000",
         "asize":"2000",
         "psize":"1000",
         "lsize":"1000",
         "compressed":"uncompressed"
       }
     },
     "Object Set":{
       "os_type":"ZPL",
       "dnonde":{
         "dn_type":"DMU dnode",
         "dn_bonustype":0,
         "dn_indblkshift":17,
         "dn_nlevels":6,
         "dn_nblkptr":3,
         "dn_blkptr":[
           {
             "vdev":"0",
             "offset":"8024000",
             "asize":"2000",
             "psize":"2000",
             "lsize":"20000",
             "compressed":"lz4"
           },
           .... output omitted ....
         ]
       }
     }
   }
   ```

#. Note that 6-levels indirect block pointer is used,
   we need to walk down to the L0 block pointer first.

   ```bash
   $ zdump --indirect-blkptr /dev/mapper/zdisk0:"8024000":20000/2000:6
   {
     "[L4]":[
       {
         "vdev":"0",
         "offset":"3801c000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"20000",
         "compressed":"lz4"
       },
       .... output omitted ....
     ],
     "[L3]":[
       {
         "vdev":"0",
         "offset":"2802a000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"20000",
         "compressed":"lz4"
       },
       .... output omitted ....
     ],
     "[L2]":[
       {
         "vdev":"0",
         "offset":"8022000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"20000",
         "compressed":"lz4"
       },
       .... output omitted ....
     ],
     "[L1]":[
       {
         "vdev":"0",
         "offset":"4c000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"20000",
         "compressed":"lz4"
       },
       .... output omitted ....
     ],
     "[L0]":[
       {
         "vdev":"0",
         "offset":"46000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"4000",
         "compressed":"lz4"
       },
       {
         "vdev":"0",
         "offset":"4a000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"4000",
         "compressed":"lz4"
       },
       {
         "vdev":"0",
         "offset":"0",
         "asize":"0",
         "psize":"200",
         "lsize":"200",
         "compressed":"inherit"
       },
       {
         "vdev":"0",
         "offset":"0",
         "asize":"0",
         "psize":"200",
         "lsize":"200",
         "compressed":"inherit"
       },
       {
         "vdev":"0",
         "offset":"48000",
         "asize":"2000",
         "psize":"2000",
         "lsize":"4000",
         "compressed":"lz4"
       },
       .... output omitted ....
     ]
   }
   ```

#. Dump the master node,
   which is a micro ZAP object and fixed in the $1^{st}$ dnode in the array.

   ```bash
   $ zdump --headds-masternode /dev/mapper/zdisk0:"46000":4000/2000
   {
     "dnode":{
       "dn_type":"ZFS master node",
       "dn_bonustype":0,
       "dn_indblkshift":17,
       "dn_nlevels":1,
       "dn_nblkptr":3,
       "dn_blkptr":[
         {
           "vdev":"0",
           "offset":"2000",
           "asize":"2000",
           "psize":"200",
           "lsize":"200",
           "compressed":"uncompressed"
         },
         .... output omitted ....
       ]
     },
     "Micro ZAP":[
       .... output omitted ....
       {
         "mze_name":"ROOT",
         "mze_value":34
       }
     ]
   }
   ```

#. The ROOT object ("/") is the $34^{th}$ object,
   located in the $1^{st}$ block (offset $4a000$).
   In this simplest case,
   the ROOT's dnode contains _embedded block pointer_,
   it is a micro ZAP object.

   ```bash
   $ zdump --root-dir /dev/mapper/zdisk0:"4a000":4000/2000:34
   {
     "Micro ZAP":[
       {
         "mze_name":"sbin",
         "mze_value":2
       },
       {
         "mze_name":"var",
         "mze_value":3
       },
       {
         "mze_name":"usr",
         "mze_value":4
       }
     ]
   }
   ```

#. Dump the `sbin` directory,
   the $2^{nd}$ object in the $0^{th}$ block,
   from the output of indirect block pointer dumping,
   we knew that the block is located at $46000$,
   the object contains embedded block pointer again.

   ```bash
   $ zdump --dir /dev/mapper/zdisk0:"46000":4000/2000:2
   {
     "Micro ZAP":[
       {
         "mze_name":"zdump",
         "mze_value":128
       },
       {
         "mze_name":"",
         "mze_value":0
       },
       {
         "mze_name":"",
         "mze_value":0
       }
     ]
   }
   ```

#. The `/sbin/zdump` file is the$128^{th}$ object,
   located in the $4^{th}$ block (offset $48000$),
   let's dump it.

   ```bash
   $ zdump --text /dev/mapper/zdisk0:"48000":4000/2000:128
   #!/bin/bash

   echo Hello ZFS
   ```

#  Recovering removed file

\textcolor{red}{TBD}
