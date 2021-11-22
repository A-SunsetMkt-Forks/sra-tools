/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include <kapp/main.h>
#include <kapp/args.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>
#include <vdb/vdb-priv.h>
#include <kdb/manager.h>
#include <kdb/meta.h>
#include <kfs/directory.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/rc.h>
#include <klib/data-buffer.h>
#include <klib/printf.h>
#include <sra/sradb.h>

#include <stdarg.h>
#include <assert.h>
#include <inttypes.h>

#include <sysexits.h>
#include <unistd.h>
#include <sys/mman.h> 

#define TEMP_MAIN_OBJECT_NAME "out"

extern "C" {
    rc_t KMDataNodeAddr(const KMDataNode *self, const void **addr, size_t *size);
}

struct CellData {
    void const *data;
    uint32_t count;
    uint32_t elem_bits;

    CellData()
    : data(nullptr)
    , count(0)
    , elem_bits(0)
    {}

    template<typename T>
    T const &value() const {
        assert(sizeof(T) * 8 == elem_bits);
        auto const p = reinterpret_cast<T const *>(data);
        return *p;
    }
    template<typename T>
    void copyTo(std::vector<T> &dst) const {
        assert(sizeof(T) * 8 == elem_bits);
        auto const p = reinterpret_cast<T const *>(data);
        dst.assign(p, p + count);
    }
};

static CellData cellData(char const *const colName, int const col, int64_t const row, VCursor const *const curs);
static uint64_t rowCount(VCursor const *const curs, int64_t *const first, uint32_t cid);
static uint32_t addColumn( char const *const name
                         , char const *const type
                         , VCursor const *const curs);
static void openCursor(VCursor const *const curs, char const *const name);
static void openRow(int64_t const row, VCursor const *const out);
static void commitRow(int64_t const row, VCursor *const out);
static void closeRow(int64_t const row, VCursor *const out);
static void commitCursor(VCursor *const out);
static VDBManager *manager();
static Args *getArgs(int argc, char *argv[]);
static char const *getOptArgValue(int opt, Args *const args);
static char const *getParameter(Args *const args, char const *);
static int pathType(VDBManager const *mgr, char const *path);
static void tblSchemaInfo(VTable const *tbl, char **name, VSchema *schema);
static void dbSchemaInfo(VDatabase const *db, char **name, VSchema *schema);

struct Inputs {
    VTable const *sequence = nullptr;
    VTable const *primaryAlignment = nullptr;
    VTable const *secondaryAlignment = nullptr;
    std::string schemaType;
    bool noDb = false;
};

struct Outputs {
    VTable *sequence;
    VTable *primaryAlignment;
    VTable *secondaryAlignment;
};

static Inputs openInputs(char const *input, VDBManager const *mgr, VSchema *schema);
static bool processSequenceTables(VTable *const output, VTable const *const input, bool const aligned, char const *&readFilterColName);
static void processAlignmentTables(VTable *const output, VTable const *const input, bool &has_offset_type, bool const isPrimary);
static Outputs createOutputs(Args *const args, VDBManager *const mgr, Inputs const &inputs, VSchema const *schema);
static VSchema *makeSchema(VDBManager *mgr);

static void OUT_OF_MEMORY()
{
    LogErr(klogFatal, RC(rcExe, rcFile, rcReading, rcMemory, rcExhausted), "OUT OF MEMORY!!!");
    exit(EX_TEMPFAIL);
}

static CellData cellData(char const *const colName, int const col, int64_t const row, VCursor const *const curs)
{
    CellData result;

    if (col) {
        uint32_t bit_offset = 0;
        rc_t rc = VCursorCellDataDirect(curs, row, col, &result.elem_bits, &result.data, &bit_offset, &result.count);
        if (rc) {
            pLogErr(klogFatal, rc, "Failed to read $(col) at row $(row)", "col=%s,row=%ld", colName, row);
            exit(EX_DATAERR);
        }
        assert(bit_offset == 0);
        assert((result.elem_bits & 0x7) == 0);
    }
    return result;
}

static uint64_t rowCount(VCursor const *const curs, int64_t *const first, uint32_t cid)
{
    uint64_t count = 0;
    rc_t const rc = VCursorIdRange(curs, cid, first, &count);
    assert(rc == 0);
    return count;
}

static uint32_t addColumn( char const *const name
                         , char const *const type
                         , VCursor const *const curs)
{
    uint32_t cid = 0;
    rc_t const rc = VCursorAddColumn(curs, &cid, "(%s)%s", type, name);
    if (rc == 0)
        return cid;

    pLogErr(klogFatal, rc, "Failed to open $(name) column", "name=%s", name);
    exit(EX_NOINPUT);
}

static uint32_t addColumn(  char const *const name
                          , char const *const type
                          , VCursor const *const curs
                          , bool &have)
{
    uint32_t cid = 0;
    rc_t const rc = VCursorAddColumn(curs, &cid, "(%s)%s", type, name);
    have = rc == 0;
    return rc == 0 ? cid : 0;
}

static uint32_t addColumn(  char const *const name
                          , char const *const altname
                          , char const *const type
                          , VCursor const *const curs
                          , char const *&used)
{
    auto have = false;
    auto cid = addColumn(used = name, type, curs, have);
    if (have)
        return cid;
    cid = addColumn(used = altname, type, curs, have);
    if (have)
        return cid;
    used = nullptr;
    return 0;
}

static void openCursor(VCursor const *const curs, char const *const name)
{
    rc_t const rc = VCursorOpen(curs);
    if (rc) {
        pLogErr(klogFatal, rc, "Failed to open $(name) cursor", "name=%s", name);
        exit(EX_NOINPUT);
    }
}

static VTable const *openTable(char const *const name, VDBManager const *const mgr)
{
    VTable const *in = NULL;
    rc_t const rc = VDBManagerOpenTableRead(mgr, &in, NULL, "%s", name);
    if (rc == 0)
        return in;

    LogErr(klogFatal, rc, "can't open input table");
    exit(EX_SOFTWARE);
}

static VTable const *openInputTable(char const *const name, VDBManager const *const mgr, std::string &schemaType, VSchema *schema)
{
    VTable const *const in = openTable(name, mgr);
    char *schemaName;

    tblSchemaInfo(in, &schemaName, schema);
    schemaType.assign(schemaName);
    free(schemaName);

    return in;
}

static VDatabase const *openDatabase(char const *const input, VDBManager const *const mgr)
{
    VDatabase const *db = NULL;
    rc_t const rc = VDBManagerOpenDBRead(mgr, &db, NULL, "%s", input);
    if (rc == 0)
        return db;
        
    LogErr(klogFatal, rc, "can't open input database");
    exit(EX_SOFTWARE);
}

static void getSchemaInfo(KMetadata const *const meta, char **type, VSchema *schema)
{
    KMDataNode const *root = NULL;
    KMDataNode const *node = NULL;
    size_t valueLen = 0;
    char *value = NULL;
    {
        rc_t const rc = KMetadataOpenNodeRead(meta, &root, NULL);
        KMetadataRelease(meta);
        if (rc != 0) {
            LogErr(klogFatal, rc, "can't get database metadata");
            exit(EX_SOFTWARE);
        }
    }
    {
        rc_t const rc = KMDataNodeOpenNodeRead(root, &node, "schema");
        KMDataNodeRelease(root);
        if (rc != 0) {
            LogErr(klogFatal, rc, "can't get database schema");
            exit(EX_SOFTWARE);
        }
    }
    {
        rc_t const rc = KMDataNodeAddr(node, (const void **)&value, &valueLen);
        if (rc != 0) {
            LogErr(klogFatal, rc, "can't get database schema");
            exit(EX_SOFTWARE);
        }
    }
    {
        rc_t const rc = VSchemaParseText(schema, NULL, value, valueLen);
        if (rc != 0) {
            LogErr(klogFatal, rc, "can't get database schema");
            exit(EX_SOFTWARE);
        }
    }
    {
        char dummy = 0;
        rc_t const rc = KMDataNodeReadAttr(node, "name", &dummy, 0, &valueLen);
        assert(GetRCObject(rc) == (int)rcBuffer && GetRCState(rc) == (int)rcInsufficient);
        if (!(GetRCObject(rc) == (int)rcBuffer && GetRCState(rc) == (int)rcInsufficient)) {
            LogErr(klogFatal, rc, "can't get database schema");
            exit(EX_SOFTWARE);
        }
    }
    value = (char *)malloc(valueLen + 1);
    if (value == NULL)
        OUT_OF_MEMORY();
    {
        rc_t const rc = KMDataNodeReadAttr(node, "name", value, valueLen + 1, &valueLen);
        KMDataNodeRelease(node);
        if (rc != 0) {
            LogErr(klogFatal, rc, "can't get database schema");
            exit(EX_SOFTWARE);
        }
    }
    value[valueLen] = '\0';
    *type = value;
    pLogMsg(klogInfo, "Schema type is $(type)", "type=%s", value);
}

static void dbSchemaInfo(VDatabase const *const db, char **name, VSchema *schema)
{
    KMetadata const *meta = NULL;
    {
        rc_t const rc = VDatabaseOpenMetadataRead(db, &meta);
        if (rc != 0) {
            LogErr(klogFatal, rc, "can't get database metadata");
            exit(EX_SOFTWARE);
        }
    }
    getSchemaInfo(meta, name, schema);
}

static void tblSchemaInfo(VTable const *const tbl, char **name, VSchema *schema)
{
    KMetadata const *meta = NULL;
    {
        rc_t const rc = VTableOpenMetadataRead(tbl, &meta);
        if (rc != 0) {
            LogErr(klogFatal, rc, "can't get database metadata");
            exit(EX_SOFTWARE);
        }
    }
    getSchemaInfo(meta, name, schema);
}

static VTable const *dbOpenTable(  VDatabase const *db
                                 , char const *const table
                                 , VDBManager const *const mgr
                                 , std::string &schemaType
                                 , VSchema *schema
                                 , bool optional = false
                                 )
{
    VTable const *in = NULL;
    rc_t const rc = VDatabaseOpenTableRead(db, &in, "%s", table);
    if (rc == 0 && schema) {
        char *schemaName;
        dbSchemaInfo(db, &schemaName, schema);
        schemaType.assign(schemaName);
        free(schemaName);
    }
    if (rc == 0 || optional)
        return in;

    LogErr(klogFatal, rc, "can't open input table");
    exit(EX_NOINPUT);
}

#define PATH_TYPE_ISA_DATABASE(TYPE) ((TYPE | kptAlias) == (kptDatabase | kptAlias))
#define PATH_TYPE_ISA_TABLE(TYPE) ((TYPE | kptAlias) == (kptTable | kptAlias))

static VDBManager *manager()
{
    VDBManager *mgr = NULL;
    rc_t const rc = VDBManagerMakeUpdate(&mgr, NULL);
    if (rc == 0)
        return mgr;
    
    LogErr(klogFatal, rc, "VDBManagerMake failed!");
    exit(EX_TEMPFAIL);
}

static int pathType(VDBManager const *mgr, char const *path)
{
    KDBManager const *kmgr = 0;
    rc_t rc = VDBManagerOpenKDBManagerRead(mgr, &kmgr);
    if (rc == 0) {
        int const type = KDBManagerPathType(kmgr, "%s", path);
        KDBManagerRelease(kmgr);
        return type;
    }
    LogErr(klogFatal, rc, "VDBManagerOpenKDBManager failed!");
    exit(EX_TEMPFAIL);
}

static void openRow(int64_t const row, VCursor const *const out)
{
    rc_t const rc = VCursorOpenRow(out);
    if (rc) {
        pLogErr(klogFatal, rc, "Failed to open a new row $(row)", "row=%li", row);
        exit(EX_IOERR);
    }
}

static void commitRow(int64_t const row, VCursor *const out)
{
    rc_t const rc = VCursorCommitRow(out);
    if (rc) {
        pLogErr(klogFatal, rc, "Failed to commit row $(row)", "row=%li", row);
        exit(EX_IOERR);
    }
}

static void closeRow(int64_t const row, VCursor *const out)
{
    rc_t const rc = VCursorCloseRow(out);
    if (rc) {
        pLogErr(klogFatal, rc, "Failed to close row $(row)", "row=%li", row);
        exit(EX_IOERR);
    }
}

static void commitCursor(VCursor *const out)
{
    rc_t const rc = VCursorCommit(out);
    if (rc) {
        LogErr(klogFatal, rc, "Failed to commit cursor");
        exit(EX_IOERR);
    }
}

static void writeRow(  int64_t const row
                      , CellData const &data
                      , uint32_t const cid
                      , VCursor *const out)
{
    if (cid) {
        rc_t const rc = VCursorWrite(out, cid, data.elem_bits, data.data, 0, data.count);
        if (rc) {
            pLogErr(klogFatal, rc, "Failed to write row $(row)", "row=%li", row);
            exit(EX_IOERR);
        }
    }
}

static KDirectory *rootDir()
{
    KDirectory *ndir = NULL;
    rc_t const rc = KDirectoryNativeDir(&ndir);
    if (rc) {
        LogErr(klogFatal, rc, "Can't get a directory!!!");
        exit(EX_SOFTWARE);
    }
    return ndir;
}

static KDirectory *openDirUpdate(char const *const path, ...)
{
    va_list va;
    KDirectory *ndir = rootDir();
    KDirectory *result = NULL;
    rc_t rc;

    va_start(va, path);
    rc = KDirectoryVOpenDirUpdate(ndir, &result, false, path, va);
    va_end(va);
    KDirectoryRelease(ndir);
    if (rc == 0)
        return result;

    LogErr(klogFatal, rc, "Can't get directory");
    exit(EX_SOFTWARE);
}

static KDirectory const *openDirRead(char const *const path, ...)
{
    va_list va;
    KDirectory *ndir = rootDir();
    KDirectory const *result = NULL;
    rc_t rc;

    va_start(va, path);
    rc = KDirectoryVOpenDirRead(ndir, &result, false, path, va);
    va_end(va);
    KDirectoryRelease(ndir);
    if (rc == 0)
        return result;

    LogErr(klogFatal, rc, "Can't get directory");
    exit(EX_SOFTWARE);
}

static KMDataNode const *openNodeRead(VTable const *const tbl, char const *const path, ...)
{
    va_list va;
    KMetadata const *meta = NULL;
    KMDataNode const *node = NULL;
    rc_t rc = VTableOpenMetadataRead(tbl, &meta);

    if (rc) {
        LogErr(klogFatal, rc, "can't open table metadata!!!");
        exit(EX_SOFTWARE);
    }

    va_start(va, path);
    rc = KMetadataVOpenNodeRead(meta, &node, path, va);
    va_end(va);
    KMetadataRelease(meta);
    if (rc) {
        LogErr(klogFatal, rc, "can't get metadata node to read");
        exit(EX_SOFTWARE);
    }
    
    return node;
}

static KMDataNode *openNodeUpdate(KMetadata *meta, char const *const path, va_list va) {
    KMDataNode *node = NULL;

    auto const rc = KMetadataVOpenNodeUpdate(meta, &node, path, va);
    if (rc) {
        LogErr(klogFatal, rc, "can't get metadata node to update");
        exit(EX_DATAERR);
    }

    return node;
}

static KMDataNode *openNodeUpdate(VDatabase *const db, char const *const path, ...)
{
    va_list va;
    KMetadata *meta = NULL;
    rc_t rc = VDatabaseOpenMetadataUpdate(db, &meta);

    if (rc) {
        LogErr(klogFatal, rc, "can't open database metadata!!!");
        exit(EX_SOFTWARE);
    }

    va_start(va, path);
    KMDataNode *const node = openNodeUpdate(meta, path, va);
    va_end(va);
    KMetadataRelease(meta);

    return node;
}

static KMDataNode *openNodeUpdate(VTable *const tbl, char const *const path, ...)
{
    va_list va;
    KMetadata *meta = NULL;
    rc_t rc = VTableOpenMetadataUpdate(tbl, &meta);

    if (rc) {
        LogErr(klogFatal, rc, "can't open table metadata!!!");
        exit(EX_SOFTWARE);
    }
    
    va_start(va, path);
    KMDataNode *const node = openNodeUpdate(meta, path, va);
    va_end(va);
    KMetadataRelease(meta);

    return node;
}

static void copyNodeValue(KMDataNode *const dst, KMDataNode const *const src)
{
    extern rc_t KMDataNodeAddr(const KMDataNode *self, const void **addr, size_t *size);
    void const *data = NULL;
    size_t data_size = 0;
    rc_t rc = KMDataNodeAddr(src, &data, &data_size);
    if (rc) {
        LogErr(klogFatal, rc, "can't read metadata");
        exit(EX_DATAERR);
    }

    rc = KMDataNodeWrite(dst, data, data_size);
    KMDataNodeRelease(src);
    KMDataNodeRelease(dst);
    if (rc) {
        LogErr(klogFatal, rc, "can't write metadata");
        exit(EX_DATAERR);
    }
}

static void writeChildNode(KMDataNode *const node, char const *const name, size_t const size, void const *const data)
{
    KMDataNode *child = NULL;
    {
        rc_t const rc = KMDataNodeOpenNodeUpdate(node, &child, "%s", name);
        if (rc) {
            LogErr(klogFatal, rc, "can't update metadata");
            exit(EX_DATAERR);
        }
    }
    {
        rc_t const rc = KMDataNodeWrite(child, data, size);
        KMDataNodeRelease(child);
        if (rc) {
            LogErr(klogFatal, rc, "can't write metadata");
            exit(EX_DATAERR);
        }
    }
}

static VTable *openUpdateTbl(char const *const name, VDBManager *const mgr)
{
    VTable *tbl = NULL;
    rc_t const rc = VDBManagerOpenTableUpdate(mgr, &tbl, NULL, "%s", name);
    if (rc) {
        LogErr(klogFatal, rc, "can't open table for update");
        exit(EX_DATAERR);
    }
    return tbl;
}

static VDatabase *openUpdateDb(char const *const name, VDBManager *const mgr)
{
    VDatabase *db = NULL;
    rc_t rc = VDBManagerOpenDBUpdate(mgr, &db, NULL, "%s", name);
    if (rc) {
        LogErr(klogFatal, rc, "can't open database for update");
        exit(EX_DATAERR);
    }
    return db;
}

static VTable *openUpdateDb(char const *const name, char const *const table, VDBManager *const mgr)
{
    VTable *tbl = NULL;
    VDatabase *db = openUpdateDb(name, mgr);
    rc_t rc = VDatabaseOpenTableUpdate(db, &tbl, "%s", table);
    VDatabaseRelease(db);
    if (rc) {
        LogErr(klogFatal, rc, "can't open table for update");
        exit(EX_DATAERR);
    }
    return tbl;
}

static VTable const *openReadDb(char const *const name, char const *const table, VDBManager const *const mgr)
{
    VTable const *tbl = NULL;
    VDatabase const *db = NULL;
    rc_t rc = VDBManagerOpenDBRead(mgr, &db, NULL, "%s", name);
    if (rc) {
        LogErr(klogFatal, rc, "can't open database for read");
        exit(EX_DATAERR);
    }
    rc = VDatabaseOpenTableRead(db, &tbl, "SEQUENCE");
    VDatabaseRelease(db);
    if (rc) {
        LogErr(klogFatal, rc, "can't open table for read");
        exit(EX_DATAERR);
    }
    return tbl;
}

static VTable const *openReadTbl(char const *const name, VDBManager const *const mgr)
{
    return openTable(name, mgr);
}

static bool dropColumn(VTable *const tbl, char const *const name)
{
    auto const rc = VTableDropColumn(tbl, "%s", name);
    auto const notFound = GetRCObject(rc) == (int)rcPath && GetRCState(rc) == (int)rcNotFound;
    if (notFound) {
        pLogMsg(klogDebug, "column $(column) doesn't exist", "column=%s", name);
        return false;
    }
    if (rc) {
        pLogErr(klogInfo, rc, "can't drop $(column) column", "column=%s", name);
        exit(EX_SOFTWARE);
    }
    return true;
}

static void dropColumn(VTable *const tbl, char const *const name, char const *const altname, char const *&used)
{
    if (dropColumn(tbl, used = name))
        return;
    if (dropColumn(tbl, used = altname))
        return;
    used = nullptr;
}

static void removeTempDir(char const *const temp)
{
    KDirectory *ndir = rootDir();
    rc_t rc;
    
    rc = KDirectoryRemove(ndir, true, "%s", temp);
    KDirectoryRelease(ndir);
    if (GetRCState(rc) == (int)rcBusy && GetRCObject(rc) == (int)rcPath) {
        pLogErr(klogWarn, rc, "failed to delete temp object directory $(path), remove it manually", "path=%s", temp);
    }
    else if (rc) {
        LogErr(klogFatal, rc, "failed to delete temp object directory");
        exit(EX_DATAERR);
    }
    else {
        pLogMsg(klogInfo, "Deleted temp object directory $(temp)", "temp=%s", temp);
    }
}
