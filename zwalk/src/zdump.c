#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include <lz4.h>
#include <json-c/json.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/vdev_impl.h>
#include <sys/dnode.h>
#include <sys/dmu_objset.h>
#include <sys/zap_impl.h>
#include <sys/zap_leaf.h>
#include <sys/dsl_dir.h>
#include <sys/blkptr.h>

#define ZDUMP_BLOCK_COMPRESSED    1
#define ZDUMP_BLOCK_UNCOMPRESSED  0

typedef enum zdump_object {
        ZDUMP_HELP = 0,
        ZDUMP_BLKPTR_CHAIN,
        ZDUMP_LABEL,
        ZDUMP_MOS,
        ZDUMP_MOS_OBJDIR,
        ZDUMP_MOS_ROOTDS,
        ZDUMP_MOS_CHILDMAP,
        ZDUMP_MOS_HEADDS,
        ZDUMP_HEADDS_MASTERNODE,
        ZDUMP_ROOT_DIR,
        ZDUMP_DIR,
        ZDUMP_TEXT,
        ZDUMP_OBJECT_NUM
} zdump_object_t;

static char *zdump_titles[ZDUMP_OBJECT_NUM] = {
        [ZDUMP_HELP]              = "Help",
        [ZDUMP_BLKPTR_CHAIN]      = "Block Pointer Chain",
        [ZDUMP_LABEL]             = "Vdev Label",
        [ZDUMP_MOS]               = "MOS",
        [ZDUMP_MOS_ROOTDS]        = "MOS Root Dataset",
        [ZDUMP_MOS_CHILDMAP]      = "MOS Child Map",
        [ZDUMP_MOS_HEADDS]        = "MOS Head Dataset",
        [ZDUMP_HEADDS_MASTERNODE] = "Head Dataset Masternode",
        [ZDUMP_ROOT_DIR]          = "Root Dir",
        [ZDUMP_DIR]               = "Directory",
        [ZDUMP_TEXT]              = "Content",
};

typedef struct zdump_opts {
        zdump_object_t object;
        int            debug;
        char           device[PATH_MAX];
        uint64_t       offset;
        uint64_t       psize;
        uint64_t       lsize;
        union {
                int            level;
                int            objidx;
        } extra;
} zdump_opts_t;

typedef int (*zdump_func_t)(zdump_opts_t *, json_object *);

static struct option long_options[] = {
        { "debug",             no_argument       , 0       , 'd' },
        { "help",              no_argument       , 0       , ZDUMP_HELP },
        { "indirect-blkptr",   required_argument , 0       , ZDUMP_BLKPTR_CHAIN },
        { "label",             required_argument , 0       , ZDUMP_LABEL },
        { "mos",               required_argument , 0       , ZDUMP_MOS },
        { "mos-objdir",        required_argument , 0       , ZDUMP_MOS_OBJDIR },
        { "mos-rootds",        required_argument , 0       , ZDUMP_MOS_ROOTDS },
        { "mos-childmap",      required_argument , 0       , ZDUMP_MOS_CHILDMAP },
        { "mos-headds",        required_argument , 0       , ZDUMP_MOS_HEADDS },
        { "headds-masternode", required_argument , 0       , ZDUMP_HEADDS_MASTERNODE },
        { "root-dir",          required_argument , 0       , ZDUMP_ROOT_DIR },
        { "dir",               required_argument , 0       , ZDUMP_DIR },
        { "text",              required_argument , 0       , ZDUMP_TEXT },
};

static const char *objset_types[DMU_OST_NUMTYPES] = {
	"NONE", "META", "ZPL", "ZVOL", "OTHER", "ANY" };

static void print_raw(void *buf, size_t size, const char *label)
{
        uint64_t     *d      = (typeof(d))buf;
	unsigned int  nwords = size / sizeof (uint64_t);
	char         *c;

	(void) printf("\n%s\n%6s   %s  0123456789abcdef\n",
                      label, "", " 0 1 2 3 4 5 6 7   8 9 a b c d e f");

	for (int i = 0; i < nwords; i += 2) {
		(void) printf("%06llx:  %016llx  %016llx  ",
                              (u_longlong_t)(i * sizeof (uint64_t)),
                              (u_longlong_t)d[i],
                              (u_longlong_t)d[i+1]);

		c = (char *)&d[i];
		for (int j = 0; j < 2 * sizeof (uint64_t); j++)
			(void) printf("%c", isprint(c[j]) ? c[j] : '.');
		(void) printf("\n");
	}
}

static char *read_block(zdump_opts_t *opts, int compressed)
{
        char          *buf = NULL, *dbuf = NULL;
        int            fd;
        struct stat64  statbuf;
        uint32_t       bufsize;

        if ((fd = open64(opts->device, O_RDONLY)) < 0) {
                fprintf(stderr, "[%s@%d]: open64 (%s) failed.\n%s\n",
                        __func__, __LINE__
                        , opts->device, strerror(errno));
                goto out;
        }
        if (fstat64_blk(fd, &statbuf) != 0) {
                fprintf(stderr, "[%s@%d]: fstat64_blk (%s) failed\n%s\n",
                        __func__, __LINE__
                        , opts->device, strerror(errno));
                goto close_fd;
        }

        /*
         * TODO: check page alignment, and etc, see zdb.c:dump_label
         */
        buf = malloc(opts->psize);
        if (buf == NULL) {
                fprintf(stderr, "[%s@%d]: malloc failed\n%s\n",
                        __func__, __LINE__, strerror(errno));
                goto close_fd;
        }
        dbuf = buf;

        if (pread64(fd, buf, opts->psize, opts->offset) == -1) {
                fprintf(stderr, "[%s@%d]: pread64 (%s-%d-%lu-%lu) failed\n%s\n"
                        , __func__, __LINE__
                        , opts->device
                        , fd
                        , opts->psize
                        , opts->offset
                        , strerror(errno));

                goto free_buf;
        }

        if (compressed) {
                dbuf = malloc(opts->lsize);
                if (dbuf == NULL) {
                        fprintf(stderr, "[%s@%d]: malloc failed\n%s\n",
                                __func__, __LINE__, strerror(errno));
                        goto free_buf;
                }
                bufsize = BE_IN32(buf);
                if (LZ4_decompress_safe(buf + sizeof(bufsize),
                                        dbuf, bufsize, opts->lsize) < 0) {
                        fprintf(stderr, "[%s@%d]: decompressing failed!\n",
                                __func__, __LINE__);
                        goto free_dbuf;
                }
                free(buf);
        }

        close(fd);
        return dbuf;

free_dbuf:
        free(dbuf);
free_buf:
        free(buf);
close_fd:
        close(fd);
out:
        return NULL;
}

static json_object *zdump_blkptr(const blkptr_t *bp)
{
        char         vdev_hexstr[8];
        char         offset_hexstr[16];
        char         asize_hexstr[8];
        char         psize_hexstr[8];
        char         lsize_hexstr[8];
        json_object *json_bp;

        snprintf(vdev_hexstr, sizeof(vdev_hexstr)
                 , "%llu", DVA_GET_VDEV(&bp->blk_dva[0]));
        snprintf(offset_hexstr, sizeof(offset_hexstr)
                 , "%llx", DVA_GET_OFFSET(&bp->blk_dva[0]));
        snprintf(asize_hexstr, sizeof(asize_hexstr)
                 , "%llx", DVA_GET_ASIZE(&bp->blk_dva[0]));
        snprintf(lsize_hexstr, sizeof(lsize_hexstr)
                 , "%llx", BP_GET_LSIZE(bp));
        snprintf(psize_hexstr, sizeof(psize_hexstr)
                 , "%llx", BP_GET_PSIZE(bp));

        json_bp = json_object_new_object();
        json_object_object_add(json_bp, "vdev",
                               json_object_new_string(vdev_hexstr));
        json_object_object_add(json_bp, "offset",
                               json_object_new_string(offset_hexstr));
        json_object_object_add(json_bp, "asize",
                               json_object_new_string(asize_hexstr));
        json_object_object_add(json_bp, "psize",
                               json_object_new_string(psize_hexstr));
        json_object_object_add(json_bp, "lsize",
                               json_object_new_string(lsize_hexstr));
        json_object_object_add(json_bp, "compressed",
                               json_object_new_string(
                                       BP_GET_COMPRESS(bp) < ZIO_COMPRESS_FUNCTIONS ?
                                       zio_compress_table[BP_GET_COMPRESS(bp)].ci_name : "Error!"));

        return json_bp;
}

static json_object *zdump_dnode(const dnode_phys_t *dn)
{
        json_object *json_dn        = json_object_new_object();
        json_object *json_dn_blkptr = json_object_new_array();

        json_object_object_add(json_dn, "dn_type",
                               dn->dn_type < DMU_OT_NUMTYPES ?
                               json_object_new_string(dmu_ot[dn->dn_type].ot_name)
                               : json_object_new_string(dmu_ot[dn->dn_bonustype].ot_name));
        json_object_object_add(json_dn, "dn_bonustype",
                               json_object_new_int(dn->dn_bonustype));
        json_object_object_add(json_dn, "dn_indblkshift",
                               json_object_new_int(dn->dn_indblkshift));
        json_object_object_add(json_dn, "dn_nlevels",
                               json_object_new_int(dn->dn_nlevels));
        json_object_object_add(json_dn, "dn_nblkptr",
                               json_object_new_int(dn->dn_nblkptr));
        for (int i = 0; i < dn->dn_nblkptr; i ++)
                json_object_array_add(json_dn_blkptr, zdump_blkptr(&dn->dn_blkptr[i]));

        json_object_object_add(json_dn, "dn_blkptr", json_dn_blkptr);
        return json_dn;
}

static json_object *zdump_objset(const char *buf, int debug)
{
        const objset_phys_t *os  = (typeof(os))buf;
        const dnode_phys_t  *dn  = &os->os_meta_dnode;
        json_object         *json_dn, *json_os = NULL;

        if ((json_dn = zdump_dnode(dn)) == NULL)
                goto out;

        json_os = json_object_new_object();
        json_object_object_add(json_os, "os_type", json_object_new_string(objset_types[os->os_type]));
        json_object_object_add(json_os, "dnonde", json_dn);
out:
        return json_os;
}

static json_object *zdump_fzap_leaf(zdump_opts_t *opts, json_object *json_parent)
{
        json_object *json_fzap_leaf = NULL, *json_leaf_header, *json_fzap_leaf_entries;
        const zap_leaf_phys_t  *zap_leaf;
        const zap_leaf_chunk_t *zap_leaf_chunk;
        int max_tries = 64, entries = 0;
        char magic_hexstr[18], type_hexstr[18];
        char        *buf = read_block(opts, ZDUMP_BLOCK_COMPRESSED);

        if (buf == NULL)
                goto out;

        zap_leaf = (typeof(zap_leaf))buf;
        json_fzap_leaf   = json_object_new_object();
        json_leaf_header = json_object_new_object();

        snprintf(type_hexstr, sizeof(type_hexstr), "0x%016lx",
                 zap_leaf->l_hdr.lh_block_type); // ZBT_LEAF
        snprintf(magic_hexstr, sizeof(magic_hexstr), "0x%08x",
                 zap_leaf->l_hdr.lh_magic); // 0x2AB1EAF
        json_object_object_add(json_leaf_header, "lh_block_type",
                               json_object_new_string(type_hexstr));
        json_object_object_add(json_leaf_header, "lh_magic",
                               json_object_new_string(magic_hexstr));
        json_object_object_add(json_fzap_leaf, "Header", json_leaf_header);

        json_fzap_leaf_entries = json_object_new_array();

        // The leaf  hash table  has  hard-coded block size / 2^5 (32)
        // entries, see zap_leaf.h.
        zap_leaf_chunk = (typeof(zap_leaf_chunk)) \
                (((uint8_t *)zap_leaf) + sizeof(zap_leaf->l_hdr)
                 + sizeof(zap_leaf->l_hash) * (opts->lsize/32));
        for (int i = 0; i < max_tries && entries < zap_leaf->l_hdr.lh_nentries; i++) {
                json_object *json_fzap_leaf_chunk;
                const zap_leaf_chunk_t *tmp = &zap_leaf_chunk[i];
                // FIXME: walk the zap_leaf_array for the complete (long) name or value.
                char                   *name;
                uint64_t               *value;
                if (tmp->l_entry.le_type != ZAP_CHUNK_ENTRY)
                        continue;

                entries++;
                name  = (typeof(name))
                        zap_leaf_chunk[tmp->l_entry.le_name_chunk].l_array.la_array;
                value = (typeof(value))
                        &zap_leaf_chunk[tmp->l_entry.le_value_chunk].l_array.la_array[0];
                json_fzap_leaf_chunk = json_object_new_object();
                json_object_object_add(json_fzap_leaf_chunk, "name"
                                       , json_object_new_string(name));
                json_object_object_add(json_fzap_leaf_chunk, "value"
                                       , json_object_new_int(BE_IN64(value)));
                json_object_array_add(json_fzap_leaf_entries, json_fzap_leaf_chunk);
        }
        json_object_object_add(json_fzap_leaf, "Entries", json_fzap_leaf_entries);
        json_object_object_add(json_parent, "FZAP leaf", json_fzap_leaf);
        free(buf);
out:
        return json_fzap_leaf;

}

static json_object *zdump_fzap(zdump_opts_t *opts, json_object *json_parent)
{
        json_object *json_fzap = NULL, *json_zap_table;
        const zap_phys_t *zap;
        char magic_hexstr[18];
        char *buf = read_block(opts, ZDUMP_BLOCK_COMPRESSED);

        if (buf == NULL)
                goto out;

        zap = (typeof(zap))buf;
        json_fzap = json_object_new_object();
        json_object_object_add(json_fzap, "zap_block_type"
                               , json_object_new_string("ZBT_HEADER"));
        snprintf(magic_hexstr, sizeof(magic_hexstr), "0x%016lx", zap->zap_magic);
        json_object_object_add(json_fzap, "zap_magic"
                               , json_object_new_string(magic_hexstr));
        json_object_object_add(json_fzap, "zap_num_entries"
                               , json_object_new_int(zap->zap_num_entries));
        json_zap_table = json_object_new_object();
        json_object_object_add(json_zap_table, "zt_blk"
                               , json_object_new_int(zap->zap_ptrtbl.zt_blk));
        json_object_object_add(json_fzap, "zap_table_phys", json_zap_table);
        json_object_object_add(json_parent, "FZAP", json_fzap);
        if (opts->debug)
                print_raw((void*)buf, opts->lsize, "FZAP block");
        free(buf);
out:
        return json_fzap;
}

// WARNING: embedded blkptr only!
static json_object *zdump_embedded_dir(const dnode_phys_t *dn)
{
        json_object           *json_mzap = NULL;
        const mzap_phys_t     *mzap;
        const mzap_ent_phys_t *mze;
        char                  *zbuf;
        const blkptr_t        *bp       = (typeof(bp))dn->dn_blkptr;

        if (!BP_IS_EMBEDDED(bp))
                goto out;

        zbuf = malloc(BPE_GET_LSIZE(bp));
        if (zbuf == NULL)
                goto out;

        decode_embedded_bp(bp, zbuf, BPE_GET_LSIZE(bp));
        mzap = (typeof(mzap))zbuf;
        mze  = mzap->mz_chunk;

        /*
         * ``sbin'', ``var'',  ``usr'' expected, `4'  means directory,
         * `8` means regular file.
         */
        json_mzap = json_object_new_array();
        for (int i = 0; i < 3; i++) {
                json_object *json_mze = json_object_new_object();
                json_object_object_add(json_mze, "mze_name"
                                       , json_object_new_string(mze[i].mze_name));
                json_object_object_add(json_mze, "mze_value"
                                       , json_object_new_int(mze[i].mze_value));
                json_object_array_add(json_mzap, json_mze);
        }

        free(zbuf);
out:
        return json_mzap;
}

int zdump_help(zdump_opts_t *opts, json_object *json_parent)
{
        printf("Usage:\n");
        return 0;
}

int zdump_blkptr_chain(zdump_opts_t *opts, json_object *json_parent)
{
        int             ret = 0;
        char           *buf;
        json_object    *json_blkptr_array;
        char            level[5];
        const blkptr_t *bp;

        if (opts->extra.level < 0)
                goto out;
        if ((buf = read_block(opts, ZDUMP_BLOCK_COMPRESSED)) == NULL) {
                ret = 1;
                goto out;
        }
        bp = (typeof(bp))buf;
        snprintf(level, sizeof(level), "[L%d]", opts->extra.level);

        json_blkptr_array = json_object_new_array();
        for (int i = 0; i < 8 /*maximum:1024*/; i++, bp++)
                json_object_array_add(json_blkptr_array, zdump_blkptr(bp));

        json_object_object_add(json_parent, level, json_blkptr_array);

        bp = (typeof(bp))buf;
        opts->offset = DVA_GET_OFFSET(&bp->blk_dva[0]) + VDEV_LABEL_START_SIZE;
        opts->lsize  = BP_GET_LSIZE(bp);
        opts->psize  = BP_GET_PSIZE(bp);
        opts->extra.level--;
        ret = zdump_blkptr_chain(opts, json_parent);
        free(buf);
out:
        return ret;
}

int zdump_text(zdump_opts_t *opts, json_object *json_parent)
{
        int ret = 0;
        const dnode_phys_t *dn;
        const blkptr_t     *bp;
        char               *buf = read_block(opts, ZDUMP_BLOCK_COMPRESSED);

        if (buf == NULL) {
                ret = 1;
                goto out;
        }

        dn = (typeof(dn))buf + + opts->extra.objidx % (opts->lsize / sizeof(dnode_phys_t));
        json_object_object_add(json_parent, "dnode", zdump_dnode(dn));
        bp = (typeof(bp))&dn->dn_blkptr[0];
        opts->offset = DVA_GET_OFFSET(&bp->blk_dva[0]) + VDEV_LABEL_START_SIZE;
        opts->lsize  = BP_GET_LSIZE(bp);
        opts->psize  = BP_GET_PSIZE(bp);

        free(buf);
        buf = read_block(opts, ZDUMP_BLOCK_UNCOMPRESSED);
        printf("%s", buf);
        if (opts->debug)
                print_raw(buf, opts->lsize, "file content:");
        free(buf);
out:
        return ret;
}

int zdump_dir(zdump_opts_t *opts, json_object *json_parent)
{
        int                 ret = 0;
        const dnode_phys_t *dn;
        char               *buf = read_block(opts, ZDUMP_BLOCK_COMPRESSED);

        if (buf == NULL) {
                ret = 1;
                goto out;
        }

        dn = (typeof(dn))buf + opts->extra.objidx % (opts->lsize / sizeof(dnode_phys_t));
        json_object_object_add(json_parent, "Micro ZAP", zdump_embedded_dir(dn));
        free(buf);
out:
        return ret;
}

int zdump_root_dir(zdump_opts_t *opts, json_object *json_parent)
{
        int                 ret = 0;
        const dnode_phys_t *dn;
        char               *buf = read_block(opts, ZDUMP_BLOCK_COMPRESSED);

        if (buf == NULL) {
                ret = 1;
                goto out;
        }

        dn = (typeof(dn))buf + opts->extra.objidx % (opts->lsize / sizeof(dnode_phys_t));
        json_object_object_add(json_parent, "dnode", zdump_dnode(dn));
        json_object_object_add(json_parent, "Micro ZAP", zdump_embedded_dir(dn));
        free(buf);
out:
        return ret;
}

int zdump_headds_masternode(zdump_opts_t *opts, json_object *json_parent)
{
        int                    ret = 0;
        json_object           *json_mzap;
        const dnode_phys_t    *dn;
        const blkptr_t        *bp;
        const mzap_phys_t     *mzap;
        const mzap_ent_phys_t *mze;
        size_t                 max;
        char                  *buf;

        if ((buf = read_block(opts, ZDUMP_BLOCK_COMPRESSED)) == NULL) {
                ret = 1;
                goto out;
        }

        dn = (typeof(dn))buf + 1;
        json_object_object_add(json_parent, "dnode", zdump_dnode(dn));

        bp = (typeof(bp))dn->dn_blkptr;
        opts->offset = DVA_GET_OFFSET(&bp->blk_dva[0]) + VDEV_LABEL_START_SIZE;
        opts->lsize  = BP_GET_LSIZE(bp);
        opts->psize  = BP_GET_PSIZE(bp);

        free(buf);
        buf = read_block(opts, ZDUMP_BLOCK_UNCOMPRESSED);
        if (buf == NULL) {
                ret = 1;
                goto out;
        }

        json_mzap = json_object_new_array();
        // it's uncompressed
        mzap = (typeof(mzap))buf;
        mze  = mzap->mz_chunk;
        max  = (opts->lsize-offsetof(typeof(*mzap), mz_chunk))/sizeof(*mze); /* 7 */
        for (int i = 0; i < max; i ++) {
                json_object *json_mze = json_object_new_object();
                json_object_object_add(json_mze, "mze_name"
                                       , json_object_new_string(mze[i].mze_name));
                json_object_object_add(json_mze, "mze_value"
                                       , json_object_new_int(mze[i].mze_value));
                json_object_array_add(json_mzap, json_mze);
        }
        json_object_object_add(json_parent, "Micro ZAP", json_mzap);

        free(buf);

out:
        return ret;
}

int zdump_mos_headds(zdump_opts_t *opts, json_object *json_parent)
{
        int                       ret = 0;
        const dnode_phys_t       *headds_dn;
        const dsl_dataset_phys_t *ds;
        const blkptr_t           *bp;
        char                      tbuf[20];
        json_object              *json_head_ds;
        char                     *buf = read_block(opts, ZDUMP_BLOCK_COMPRESSED);

        if (buf == NULL) {
                ret = 1;
                goto out;
        }

        headds_dn = (typeof(headds_dn))buf
                + opts->extra.objidx % (opts->lsize / sizeof(dnode_phys_t));
        // The dsl  is stored  in the  dn_bonus, dumping  dnode itself
        // looks not  so helpful.
        // json_object_object_add(json_parent, "dnode", zdump_dnode(headds_dn));
        ds = (typeof(ds))headds_dn->dn_bonus;
        json_head_ds = json_object_new_object();
        json_object_object_add(json_head_ds, "ds_dir_obj"
                               , json_object_new_int(ds->ds_dir_obj));
        snprintf(tbuf, sizeof(tbuf), "%s", ctime((const time_t *)&ds->ds_creation_time));
        json_object_object_add(json_head_ds, "ds_creation_time"
                               , json_object_new_string(tbuf));
        json_object_object_add(json_head_ds, "ds_create_txg"
                               , json_object_new_int(ds->ds_creation_txg));
        json_object_object_add(json_head_ds, "ds_bp", zdump_blkptr(&ds->ds_bp));
        json_object_object_add(json_parent, "Head data set", json_head_ds);

        bp = &ds->ds_bp;
        opts->offset = DVA_GET_OFFSET(&bp->blk_dva[0]) + \
                VDEV_LABEL_START_SIZE;
        opts->lsize  = BP_GET_LSIZE(bp);
        opts->psize  = BP_GET_PSIZE(bp);

        free(buf);
        // FIXME: hard-coded
        buf = read_block(opts, ZDUMP_BLOCK_UNCOMPRESSED);
        if (buf == NULL) {
                ret = 1;
                goto out;
        }

        json_object_object_add(json_parent, "Object Set", zdump_objset(buf, opts->debug));
        free(buf);
out:
        return ret;
}

int zdump_mos_childmap(zdump_opts_t *opts, json_object *json_parent)
{
        int                    ret = 0;
        const dnode_phys_t    *child_map_dn;
        const blkptr_t        *bp;
        const mzap_phys_t     *mzap;
        const mzap_ent_phys_t *mze;
        json_object           *json_embedded_blkptr;
        json_object           *json_childmap_entries;
        char                  *zbuf;
        char                  *buf;

        if ((buf  = read_block(opts, ZDUMP_BLOCK_COMPRESSED)) == NULL) {
                ret = 1;
                goto out;
        }
        child_map_dn = (typeof(child_map_dn))buf
                + opts->extra.objidx % (opts->lsize / sizeof(dnode_phys_t));
        json_object_object_add(json_parent, "dnode", zdump_dnode(child_map_dn));
        bp = child_map_dn->dn_blkptr;
        // TODO: else
        if (BP_IS_EMBEDDED(bp)) {
                json_embedded_blkptr = json_object_new_object();
                json_object_object_add(json_embedded_blkptr, "type"
                                       , json_object_new_int(BPE_GET_ETYPE(bp)));
                json_object_object_add(json_embedded_blkptr, "psize"
                                       , json_object_new_int(BPE_GET_PSIZE(bp)));
                json_object_object_add(json_embedded_blkptr, "lsize"
                                       , json_object_new_int(BPE_GET_LSIZE(bp)));
                json_object_object_add(json_embedded_blkptr, "compressed"
                                       , json_object_new_string(
                                               zio_compress_table[BP_GET_COMPRESS(bp)].ci_name));

                zbuf = malloc(BPE_GET_LSIZE(bp));
                if (zbuf == NULL)
                        goto free_buf;
                json_childmap_entries = json_object_new_array();
                decode_embedded_bp(bp, zbuf, BPE_GET_LSIZE(bp));
                mzap = (typeof(mzap))zbuf;
                mze  = mzap->mz_chunk;
                // $MOS, $FREE, and $ORIGIN are expected
                for (int i = 0; i < 3; i++) {
                        json_object *json_mze = json_object_new_object();
                        json_object_object_add(json_mze, "mze_name"
                                               , json_object_new_string(mze[i].mze_name));
                        json_object_object_add(json_mze, "mze_value"
                                               , json_object_new_int(mze[i].mze_value));
                        json_object_array_add(json_childmap_entries, json_mze);
                }
                json_object_object_add(json_embedded_blkptr, "Micro ZAP"
                                       , json_childmap_entries);
                json_object_object_add(json_parent, "Embedded Block Pointer"
                                       , json_embedded_blkptr);
        }

free_buf:
        free(buf);
out:
        return ret;
}

int zdump_mos_rootds(zdump_opts_t *opts, json_object *json_parent)
{
        int                       ret = 0;
        json_object              *json_dsl;
        const dnode_phys_t       *rootds_dn;
        const dsl_dir_phys_t     *dsl;
        char                      tbuf[20];
        char                     *buf;

        if ((buf  = read_block(opts, ZDUMP_BLOCK_COMPRESSED)) == NULL) {
                ret = 1;
                goto out;
        }

        rootds_dn = (typeof(rootds_dn))buf
                + opts->extra.objidx % (opts->lsize / sizeof(dnode_phys_t));
        json_object_object_add(json_parent, "dnode", zdump_dnode(rootds_dn));

        dsl      = (typeof(dsl))&rootds_dn->dn_bonus;
        json_dsl = json_object_new_object();
        snprintf(tbuf, sizeof(tbuf), "%s", ctime((const time_t *)&dsl->dd_creation_time));
        json_object_object_add(json_dsl, "dd_creation_time"
                               , json_object_new_string(tbuf));
        json_object_object_add(json_dsl, "dd_child_dir_zapobj"
                               , json_object_new_int(dsl->dd_child_dir_zapobj));
        json_object_object_add(json_dsl, "dd_head_dataset_obj"
                               , json_object_new_int(dsl->dd_head_dataset_obj));
        json_object_object_add(json_parent, "DSL", json_dsl);

        free(buf);
out:
        return ret;
}

int zdump_mos_objdir(zdump_opts_t *opts, json_object *json_parent)
{
        int                 ret = 0;
        const dnode_phys_t *dn;
        const blkptr_t     *bp;
        char               *buf = read_block(opts, ZDUMP_BLOCK_COMPRESSED);

        if (buf == NULL) {
                ret = 1;
                goto out;
        }

        dn = (typeof(dn))buf + 1;
        json_object_object_add(json_parent, "dnode", zdump_dnode(dn));

        bp = &dn->dn_blkptr[0];
        opts->offset = DVA_GET_OFFSET(&bp->blk_dva[0]) + \
                VDEV_LABEL_START_SIZE;
        opts->lsize  = BP_GET_LSIZE(bp);
        opts->psize  = BP_GET_PSIZE(bp);
        zdump_fzap(opts, json_parent);

        /*
         * In this simple case, blk id in all buckets are 1.
         */
        bp = &dn->dn_blkptr[1];
        opts->offset = DVA_GET_OFFSET(&bp->blk_dva[0]) + \
                VDEV_LABEL_START_SIZE;
        opts->lsize  = BP_GET_LSIZE(bp);
        opts->psize  = BP_GET_PSIZE(bp);
        zdump_fzap_leaf(opts, json_parent);

        free(buf);
out:
        return ret;
}

int zdump_mos(zdump_opts_t *opts, json_object *json_parent)
{
        int   ret = 0;
        char *buf = read_block(opts, ZDUMP_BLOCK_UNCOMPRESSED);
        if (buf == NULL) {
                ret = 1;
                goto out;
        }

        json_object_object_add(json_parent, zdump_titles[opts->object],
                               zdump_objset(buf, opts->debug));
        free(buf);
out:
        return ret;
}

// TODO: check nvlist_xxxx return value.
int zdump_label_uberblock(zdump_opts_t *opts, json_object *json_parent)
{
        int                 ret = 0;
        json_object        *json_ub, *json_label;
        vdev_t              vd;
        const vdev_label_t *label;
        const vdev_phys_t  *vdev;
        char               *buf, *nvbuf, *type, *name, *path;
        char                magic_hexstr[18];
        size_t              nvbuflen;
        nvlist_t           *config;
        uberblock_t        *winner;
        nvlist_t           *vdev_tree            = NULL;
        uint64_t            ashift, version, txg = 0;

        if ((buf = read_block(opts, ZDUMP_BLOCK_UNCOMPRESSED)) == NULL) {
                ret = 1;
                goto out;
        }

        label    = (typeof(label))buf;
        vdev     = &label->vl_vdev_phys;
        nvbuf    = (typeof(nvbuf))vdev->vp_nvlist;
        nvbuflen = sizeof(vdev->vp_nvlist);

        nvlist_unpack(nvbuf, nvbuflen, &config, 0);
        if ((nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE, &vdev_tree) != 0)
            || (nvlist_lookup_uint64(vdev_tree, ZPOOL_CONFIG_ASHIFT, &ashift) != 0))
                ashift = SPA_MINBLOCKSHIFT;

        if (opts->debug)
                print_raw(buf, opts->psize, "label");

        nvlist_lookup_uint64(config, ZPOOL_CONFIG_VERSION, &version);
        nvlist_lookup_string(config, ZPOOL_CONFIG_POOL_NAME, &name);
        nvlist_lookup_string(vdev_tree, ZPOOL_CONFIG_TYPE, &type);
        nvlist_lookup_string(vdev_tree, ZPOOL_CONFIG_PATH, &path);

        vd.vdev_ashift = ashift;
        vd.vdev_top    = &vd;
        for (int i = 0; i < VDEV_UBERBLOCK_COUNT(&vd); i++) {
                uint64_t     uoff = VDEV_UBERBLOCK_OFFSET(&vd, i);
                uberblock_t *ub   = (void *)((char *)label + uoff);

                if (uberblock_verify(ub))
                        continue;

                if (ub->ub_txg > txg) {
                        winner = ub;
                        txg    = ub->ub_txg;
                }
        }

        json_label = json_object_new_object();
        json_object_object_add(json_label, "name",    json_object_new_string(name));
        json_object_object_add(json_label, "version", json_object_new_int(version));

        json_ub = json_object_new_object();
        snprintf(magic_hexstr, sizeof(magic_hexstr), "0x%016lx", winner->ub_magic);
        json_object_object_add(json_ub, "magic",      json_object_new_string(magic_hexstr));
        json_object_object_add(json_ub, "version",    json_object_new_int(winner->ub_version));
        json_object_object_add(json_ub, "txg",        json_object_new_int(winner->ub_txg));
        json_object_object_add(json_ub, "rootbp",     zdump_blkptr(&winner->ub_rootbp));
        json_object_object_add(json_label, "uberblock", json_ub);

        json_object_object_add(json_parent, zdump_titles[opts->object], json_label);

        nvlist_free(config);
        free(buf);
out:
        return ret;
}

static int gen_opts(int argc, char **argv, zdump_opts_t *opts)
{
        int   ret = 0;
        int   c;
        char *offsize, *tok;

        if (opts == NULL) {
                ret = 1;
                goto out;
        }

        while (1) {
                int option_index = 0;
                // No confusing short options
                c = getopt_long(argc, argv, "", long_options, &option_index);
                if (c == -1)
                        break;
                switch (c) {
                case 'd':
                        opts->debug = 1;
                        break;
                default:
                        opts->object = c;
                        offsize = strdup(optarg);
                        // device
                        tok     = strtok(offsize, ":");
                        if (tok == NULL) {
                                free(opts);
                                opts = NULL;
                                goto out;
                        }
                        strncpy(opts->device, tok, sizeof(opts->device));
                        // offset
                        tok = strtok(NULL, ":");
                        opts->offset = strtoull(tok ? tok : "", NULL, 16);
                        // skip 4MB label
                        if (c != ZDUMP_LABEL)
                                opts->offset += VDEV_LABEL_START_SIZE;
                        // lsize
                        tok = strtok(NULL, "/");
                        opts->lsize = strtoull(tok ? tok : "", NULL, 16);
                        // psize
                        tok = strtok(NULL, "/");
                        opts->psize = tok ? strtoull(tok, NULL, 16) : opts->lsize;
                        opts->object = c;
                        // level, for dumping indirect blkptr
                        tok = strtok(tok, ":");
                        tok = strtok(NULL, ":");

                        /*
                         * -= 2 because the  passed argument offset is
                         * at the top level,  and counting starts from
                         * zero.  E.g.,  6-level indirect  addressing,
                         * the  argument (offset)  is  at  L5, dumping
                         * starts from L4 down to L0
                         */
                        if (c == ZDUMP_BLKPTR_CHAIN)
                                opts->extra.level = tok ? strtoull(tok, NULL, 10)-2 : 0;
                        if (c == ZDUMP_MOS_ROOTDS
                            || c == ZDUMP_MOS_CHILDMAP
                            || c == ZDUMP_MOS_HEADDS
                            || c == ZDUMP_ROOT_DIR
                            || c == ZDUMP_DIR
                            || c == ZDUMP_TEXT)
                                opts->extra.objidx  = tok ? strtoull(tok, NULL, 10) : 0;

                        free(offsize);
                        break;
                }
        }
out:
        return ret;
}

static zdump_func_t zdump_funcs[ZDUMP_OBJECT_NUM] = {
        [ZDUMP_HELP]              = zdump_help,
        [ZDUMP_BLKPTR_CHAIN]      = zdump_blkptr_chain,
        [ZDUMP_LABEL]             = zdump_label_uberblock,
        [ZDUMP_MOS]               = zdump_mos,
        [ZDUMP_MOS_OBJDIR]        = zdump_mos_objdir,
        [ZDUMP_MOS_ROOTDS]        = zdump_mos_rootds,
        [ZDUMP_MOS_CHILDMAP]      = zdump_mos_childmap,
        [ZDUMP_MOS_HEADDS]        = zdump_mos_headds,
        [ZDUMP_HEADDS_MASTERNODE] = zdump_headds_masternode,
        [ZDUMP_ROOT_DIR]          = zdump_root_dir,
        [ZDUMP_DIR]               = zdump_dir,
        [ZDUMP_TEXT]              = zdump_text,
};

int main(int argc, char **argv)
{
        int          ret  = 1;
        zdump_opts_t opts = {};

        if (gen_opts(argc, argv, &opts) == 0
            && opts.object < ZDUMP_OBJECT_NUM
            && zdump_funcs[opts.object] != NULL) {
                json_object  *json_root  = json_object_new_object();
                zdump_func_t  zdump_func = zdump_funcs[opts.object];

                zdump_func(&opts, json_root);
                printf("%s\n",
                       json_object_to_json_string_ext(json_root, JSON_C_TO_STRING_PRETTY));
                json_object_put(json_root);
                ret = 0;
        }
        return ret;
}

/*
 * Local Variables:
 * mode: c
 * c-file-style: "linux"
 * End:
 */
