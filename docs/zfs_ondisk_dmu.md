# Data Management Unit {#sec:dmu}

The _Data Management Unit_ (_DMU_) consumes blocks and groups them into logical units called objects.
Objects can be further grouped by the DMU into object sets.
Both objects and object sets are described in this chapter.

## Objects

With the exception of a small amount of infrastructure,
described in chapters @sec:vdev and @sec:blkptr,
everything in ZFS is an object. The following table lists existing ZFS object types;
many of these types are described in greater detail in future chapters of this document.

--------------------------------------------------------------------------------------------------------
**Type**                                      `\thead{`{=latex}**Description**`}`{=latex}
---------------------------------------       ----------------------------------------------------------
\small DMU_OT_NONE                            Unallocated object
                                              
\small DMU_OT_OBJECT_DIRECTORY                DSL object directory ZAP object
                                              
\small DMU_OT_OBJECT_ARRAY                    Object used to store an array of object numbers.
                                              
\small DMU_OT_PACKED_NVLIST                   Packed nvlist object.
                                              
\small DMU_OT_SPACE_MAP                       SPA disk block usage list.
                                              
\small DMU_OT_INTENT_LOG                      Intent Log
                                              
\small DMU_OT_DNODE                           Object of dnodes (metadnode)
                                              
\small DMU_OT_OBJSET                          Collection of objects.
                                              
\small DMU_OT_DSL_DATASET_CHILD_MAP           DSL ZAP object containing child DSL directory information.
                                              
\small DMU_OT_DSL_OBJSET_SNAP_MAP             DSL ZAP object containing snapshot information for a dataset.
                                              
\small DMU_OT_DSL_PROPS                       DSL ZAP properties object containing properties for
                                              a DSL dir object.
                                              
\small DMU_OT_BPLIST                          Block pointer list – used to store the “deadlist”:
                                              list of block pointers deleted since the last snapshot,
                                              and the “deferred free list” used for sync to convergence.
                                              
\small DMU_OT_BPLIST_HDR                      BPLIST header: stores the bplist_phys_t structure.
                                              
\small DMU_OT_ACL                             ACL (Access Control List) object
                                              
\small DMU_OT_PLAIN_FILE                      ZPL Plain file
                                              
\small DMU_OT_DIRECTORY_CONTENTS              ZPL Directory ZAP Object
                                              
\small DMU_OT_MASTER_NODE                     ZPL Master Node ZAP object:
                                              head object used to identify root directory,
                                              delete queue, and version for a filesystem.
                                              
\small DMU_OT_DELETE_QUEUE                    The delete queue provides a list of deletes
                                              that were in-progress when the filesystem
                                              was force unmounted or as a result of a system failure
                                              such as a power outage.
                                              Upon the next mount of the filesystem,
                                              the delete queue is processed to remove the files/dirs
                                              that are in the delete queue.
                                              This mechanism is used to avoid
                                              leaking files and directories in the filesystem.
                                              
\small DMU_OT_ZVOL                            ZFS volume (ZVOL)
                                              
\small DMU_OT_ZVOL_PROP                       ZVOL properties

--------------------------------------------------------------------------------------------------------

Table: DMU Ojbect Types {#tbl:dmu_obj_types}

Objects are defined by $512$ bytes structures called dnodes[^dnode].
A dnode describes and organizes a collection of blocks making up an object.
The dnode (`dnode_phys_t` structure), 
seen in the illustration below, 
contains several fixed length fields and two variable length fields.
Each of these fields are described in detail below.

```{.figure}
{
    "path"    : "Figures/zfs_dnode_phys",
    "caption" : "dnode\\_phys\\_t structure",
    "label"   : "zfs_dnode_phys",
    "options" : "scale=1,center",
    "place"   : "ht"
}
```

[^dnode]: A dnode is similar to an _inode_ in UFS.

dn_type:
  ~ An $8$-bit numeric value indicating an object's type.
    See Table 8\textcolor{red}{???TBD} 
    for a list of valid object types and their associated 8 bit identifiers.

dn_indblkshift and dn_datablkszsec:
  ~ ZFS supports variable data and indirect 
    (see `dn_nlevels` below for a description of indirect blocks)
    block sizes ranging from $512$ bytes to $128$ Kbytes.
    
    dn_indblkshift:
      ~ $8$-bit numeric value containing the log (base $2$) of the size (in bytes) 
        of an indirect block for this object.
        
    dn_datablkszsec:
      ~ $16$-bit numeric value containing the data block size (in bytes)
        divided by $512$ (size of a disk sector). 
        This value can range between $1$ (for a $512$ byte block) and $256$ 
        (for a $128$ Kbyte block).

dn_nblkptr and dn_blkptr:
  ~ dn_blkptr is a variable length field that can contains between one and three block pointers.
    The number of block pointers that 
    the dnode contains is set at object allocation time
    and remains constant throughout the life of the dnode.
    
    dn_nblkptr:
      ~ $8$-bit numeric value containing the number of block pointers in this dnode.

    dn_blkptr: 
      ~ block pointer array containing dn_nblkptr block pointers

dn_nlevels:
  ~ dn_nlevels is an $8$-bit numeric value containing
    the number of levels 
    that make up this object.
    These levels are often referred to as levels of indirection.

    Indirection
      ~ 
      
      A dnode has a limited number (`dn_nblkptr`, see above) of block pointers
      to describe an object's data. 
      For a dnode using the largest data block size ($128$KB) 
      and containing the maximum number of block pointers ($3$),
      the largest object size it can represent (without indirection) is 
      $384$ KB: $3 \times 128$KB = $384$KB.
      To allow for larger objects, indirect blocks are used.
      An indirect block is a block containing block pointers.
      The number of block pointers that
      an indirect block can hold is dependent on the indirect block size
      (represented by `dn_indblkshift`)
      and can be calculated by dividing the indirect block size by the size of a blkptr ($128$ bytes).
      The largest indirect block ($128$KB) can hold up to $1024$ block pointers.
      As an object's size increases,
      more indirect blocks and levels of indirection are created.
      A new level of indirection is created once 
      an object grows so large that it exceeds the capacity of the current level.
      ZFS provides up to six levels of indirection to support files up to $2^{64}$ bytes long.
      
      The illustration below shows an object with 3 levels of blocks (level 0, level 1, and level 2).
      This object has triple wide block pointers
      (dva1, dva2, and dva3) for metadata
      and single wide block pointers for its data
      (see Chapter @sec:blkptr for a description of block pointer wideness).
      The blocks at level $0$ are data blocks.
      
      ```{.figure}
      {
          "path"    : "Figures/zfs_3levels",
          "caption" : "Object with 3 levels. Triple wide block pointers used for metadata; single wide block pointers used for data",
          "label"   : "blkptr_three_levels",
          "options" : "width=\\textwidth,center",
          "place"   : "ht"
      }
      ```

dn_maxblkid
  ~ An object's blocks are identified by block ids.
    The blocks in each level of indirection are numbered from $0$ to $N$,
    where the first block at a given level is given an id of $0$,
    the second an id of $1$, and so forth.

    The dn_maxblkid field in the dnode is set to 
    the value of the largest data (level zero) block id for this object.
    
    ---- ------------------------------------------------------------------------
         Note on Block Ids:
         Given a block id and level,
         ZFS can determine the exact branch of indirect blocks which contain the block.
         This calculation is done using the block id, block level,
         and number of block pointers in an indirect block.
         For example,
         take an object which has $128$KB sized indirect blocks.
         An indirect block of this size can hold 1024 block pointers.
         Given a level 0 block id of $16360$, 
         it can be determined that block $15$ (block id $15$) of level $1$ contains
         the block pointer for level $0$ blkid $16360$.
         
         $$
	     level\ 1\ blkid = 16360 \% 1024 = 15
         $$
         This calculation can be performed recursively up 
         the tree of indirect blocks 
         until the top level of indirection has been reached.
    ------------------------------------------------------------------------------

dn_secphys:
  ~ The sum of all asize values for all block pointers (data and indirect) for this object.

dn_bonus, dn_bonuslen, and dn_bonustype:
  ~ The bonus buffer (dn_bonus) is defined as 
    the space following a dnode's block pointer array (dn_blkptr).
    The amount of space is dependent on object type and can range between $64$ and $320$ bytes.
    
    dn_bonus_len:
      ~ Length (in bytes) of the bonus buffer 
    dn_bonus:
      ~ `dn_bonuslen` sized chunk of data.
      The format of this data is defined by dn_bonustype.
    dn_bonustype: 
      ~ $8$-bit numeric value identifying 
        the type of data contained within the bonus buffer. 
        The following table shows valid bonus buffer types
        and the structures which are stored in the bonus buffer.
        The contents of each of these structures will be discussed later in this specification.

        --------------------------------------------------------------------------------------------------------
        **Bonus Type**                       **Description**                        **Metadata**       **Value**
                                                                                    **Structure**
        -------------------------            ----------------------------           --------------    ----------
        \small DMU_OT_PACKED_NVLIST_SIZE     Bonus buffer type containing           uint64_t                   4
                                             size in bytes of a
                                             DMU_OT_PACKED_NVLIST
                                             object
                                                                                                
        \small DMU_OT_SPACE_MAP_HEADER       Spa space map header                   space_map_obj_t            7
        
        \small DMU_OT_DSL_DIR                DSL Directory  object used             dsl_dir_phys_t            12
                                             to define relationships and
                                             properties between related 
                                             datasets
        
        \small DMU_OT_DSL_DATASET            DSL dataset object used to             dsl_dataset_phys_t        16
                                             organize snapshot and usage
                                             static information for 
                                             objects of type 
                                             DMU_OT_OBJSET.

        \small DMU_OT_ZNODE                  ZPL metadata                           znode_phys_t              17
        --------------------------------------------------------------------------------------------------------

        Table: Bonus Buffer  Types and associated structures
        
## Object Sets

The DMU organizes objects into groups called object sets.
Object sets are used in ZFS to group related objects,
such as objects in a filesystem, snapshot, clone, or volume.

Object sets are represented by a $1$K byte `objset_phys_t` structure.
Each member of this structure is defined in detail below.

\textcolor{red}{TBD}
