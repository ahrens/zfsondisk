---
title:	ZFS On-disk Format
author:	
 - Matthew Ahrens \<mahrens@delphix.com\>
documentclass:	memoir
lang:	en-US
secPrefix: Section.
tblPrefix: Table.
figPrefix: Illustration.
---

`<p><a href="zfs_internals.md.pdf">Click here</a> for a printer friendly PDF version.</p>`{=html}

# Introduction

ZFS is a new filesystem technology 
that provides immense capacity (128-bit), 
provable data integrity, 
always-consistent on-disk format, 
self-optimizing performance, 
and real-time remote replication.

ZFS departs from traditional filesystems by eliminating the concept of volumes.
Instead,
ZFS filesystems share a common storage pool
consisting of writeable storage media.
Media can be added or removed from the pool 
as filesystem capacity requirements change. 
Filesystems dynamically grow and shrink as needed
without the need to re-partition underlying storage.

ZFS provides a truly consistent on-disk format,
but using a _copy on write_ (_COW_) transaction model.
This model ensures that on disk data is never overwritten
and all on disk updates are done atomically.

The ZFS software is comprised of seven distinct pieces: 
the _SPA_ (_Storage Pool Allocator_), 
the _DSL_ (_Dataset and Snapshot Layer_),
the _DMU_ (_Data Management Layer_),
the _ZAP_ (_ZFS Attribute Processor_),
the _ ZPL_ (_ZFS Posix layer_),
the _ZIL_ (_ZFS Intent Log_),
and _ZVOL_ (_ZFS Volume_).
The on-disk structures associated with each of these pieces are explained in the following chapters: 
SPA (Chapters 1 and 2), 
DSL (Chapter 5), 
DMU (Chapter 3), 
ZAP (Chapter 4), 
ZPL (Chapter 6), 
ZIL (Chapter 7), 
ZVOL (Chapter 8).
