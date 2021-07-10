# Dataset and Snapshot Layer {#sec:dsl}

The _DSL_ (_Dataset and Snapshot Layer_) provides a mechanism
for describing and managing relationships-between and properties-of object sets.
Before describing the DSL and the relationships it describes,
a brief overview of the various flavors of object sets is necessary.

## Object Set Overview

ZFS provides the ability to create four kinds of object sets: 
_filesystems_, _clones_, _snapshots_, and _volumes_.

* ZFS filesystems: 
  A filesystem stores and organizes objects in an easily accessible, POSIX compliant manner.
* ZFS clone:
  A clone is identical to a filesystem with the exception of its origin.
  Clones originate from snapshots and their initial contents are identical to 
  that of the snapshot from which it originated.
* ZFS snapshot:
  A snapshot is a read-only version of a filesystem, clone, 
  or volume at a particular point in time.
* ZFS volume:
  A volume is a logical volume that is exported by ZFS as a block device.

ZFS supports several operations and/or configurations 
which cause interdependencies amongst object sets.
The purpose of the DSL is to manage these relationships.
The following is a list of such relationships.

* Clones:
  A clone is related to the snapshot from which it originated.
  Once a clone is created,
  the snapshot in which it originated can not be deleted unless the clone is also deleted.
* Snapshots:
  A snapshot is a point-in-time image of the data in the object set in which it was created.
  A filesystem, clone, or volume can not be destroyed unless its snapshots are also destroyed.
* Children:
  ZFS support hierarchically structured object sets;
  object sets within object sets.
  A child is dependent on the existence of its parent.
  A parent can not be destroyed without first destroying all children.

## DSL Infrastructure

Each object set is represented in the DSL as a dataset.
A dataset manages space consumption statistics for an object set,
contains object set location information,
and keeps track of any snapshots inter-dependencies.

Datasets are grouped together hierarchically into collections called Dataset Directories.
Dataset Directories manage a related grouping of datasets and the properties
associated with that grouping.
A DSL directory always has exactly one “active dataset”.
All other datasets under the DSL directory are related to the “active” dataset 
through snapshots, clones, or child/parent dependencies.

The following picture shows the DSL infrastructure 
including a pictorial view of 
how object set relationships are described via the DSL datasets and DSL directories.
The top level DSL Directory can be seen at the top/center of this figure.
Directly below the DSL Directory is the “active dataset”.
The active dataset represents the live  filesystem.
Originating from the active dataset is a linked list of snapshots 
which have been taken at different points in time.
Each dataset structure points to a DMU Object Set 
which is the actual object set containing object data.
To the left of the top level DSL Directory is a child ZAP object 
containing a listing of all child/parent dependencies.
To the right of the DSL directory is a properties ZAP object 
containing properties for the datasets within this DSL directory.
A listing of all properties can be seen in \textcolor{red}{Table 12? TBD} below.

A detailed description of Datasets and DSL Directories are described
in the `\nameref{sec:ds_internals}`{=latex}`Dataset Internals`{=html}
(section @sec:ds_internals) 
and `\nameref{sec:dsl_dir_internals}`{=latex}`DSL Directories Internals`{=html}.
(section @sec:dsl_dir_internals) 
sections below.

```{.figure}
{
    "path"    : "Figures/zfs_dsl_infra",
    "caption" : "DSL Infrastructure",
    "label"   : "zfs_dsl_infra",
    "options" : "scale=.8,center",
    "place"   : "ht"
}
```

## DSL Implementation Details

\textcolor{red}{TBD}

## Dataset Internals {#sec:ds_internals}

\textcolor{red}{TBD}

## DSL Directory Internals {#sec:dsl_dir_internals}

\textcolor{red}{TBD}
