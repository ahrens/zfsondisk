#. check nvlist_xxxx return value,
   otherwise, segfault if block is invalid
#. zdump_dnode: check dn\_type and dn\_bonustype range,
   otherwise, zdump\_dnode crashes.
#. Remove all dependencies on ZFS libraries,
   only data structure definition needed.
#. support multiple blocks file.
