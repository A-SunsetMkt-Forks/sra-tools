/*==============================================================================
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
* ==============================================================================
*
*/
#include "test-sra.h" /* endpoint_to_string */
#include <kfg/config.h> /* KConfigReadString */
#include <kfs/directory.h> /* KDirectoryRelease */
#include <kfs/file.h> /* KFile */
#include <klib/out.h> /* KOutMsg */
#include <klib/printf.h> /* string_vprintf */
#include <klib/rc.h>
#include <klib/text.h> /* String */
#include <kns/endpoint.h> /* KNSManagerInitDNSEndpoint */
#include <kns/http.h> /* KHttpRequest */
#include <kns/manager.h> /* KNSManager */
#include <kns/kns-mgr-priv.h> /* KNSManagerMakeReliableHttpFile */
#include <kns/stream.h> /* KStream */
#include <vfs/manager.h> /* VFSManagerOpenDirectoryRead */
#include <vfs/path.h> /* VFSManagerMakePath */
#include <ctype.h> /* isprint */
#include <limits.h> /* PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)
typedef struct {
    const KConfig * kfg;
    const KNSManager * kmgr;
    const struct VFSManager * vmgr;
    int n [ 6 ];
    int ended;
    int total;
} STest;
static void STestInit ( STest * self, const KConfig * kfg,
    const KNSManager * kmgr, const struct VFSManager * vmgr )
{
    assert ( self );
    memset ( self, 0, sizeof * self );
    self -> ended = -1;
    self -> kfg = kfg;
    self -> kmgr = kmgr;
    self -> vmgr = vmgr;
}
static void STestFini ( STest * self ) {
    assert ( self );
    if ( self -> n [ 0 ] == 0 || self -> n [ 1 ] != 0 || self -> ended != 0 )
        OUTMSG ( ( "= TEST WAS NOT COMPLETED\n" ) );
    OUTMSG ( ( "= %d (%d) tests were performed\n",
               self -> n [ 0 ], self -> total ) );
}
static rc_t STestVStart ( STest * self, bool checking,
                          const char * fmt, va_list args  )
{
    char b [ 512 ] = "";
    rc_t rc = string_vprintf ( b, sizeof b, NULL, fmt, args );
    if ( rc != 0 )
        OUTMSG ( ( "CANNOT PRINT: %R\n", rc ) );
    else {
        assert ( self );
        int i = 0;
        if ( self -> ended == -1 ) {
            bool found = false;
            self -> ended = sizeof self -> n / sizeof self -> n [ 0 ] - 1;
            for ( i = 0; i < sizeof self -> n / sizeof self -> n [ 0 ]; ++ i )
                if ( self -> n [ i ] == 0 ) {
                    self -> ended = i;
                    found = true;
                    break;
                }
            assert ( found );
        }
        assert ( self -> ended >= 0 );
        ++ self -> n [ self -> ended ];
        self -> ended = -1;
        OUTMSG ( ( "> %d", self -> n [ 0 ] ) );
        for ( i = 1; i < sizeof self -> n / sizeof self -> n [ 0 ]; ++ i )
            if ( self -> n [ i ] == 0 )
                break;
            else
                OUTMSG ( ( ".%d", self -> n [ i ] ) );
    }
    if ( rc == 0 )
        rc = OUTMSG ( ( " %s%s%s", checking ? "Checking " : "", b,
                        checking ? "...\n" : " " ) );
    return rc;
}
typedef enum {
    eFAIL,
    eOK,
    eMGS,
    eEND,
    eDONE,
} EOK;
static rc_t STestVEnd ( STest * self, EOK ok,
                        const char * fmt, va_list args )
{
    int i = 0;
    assert ( self );
    if ( ok != eMGS ) {
        if ( self -> ended != -1 )
            -- self -> ended;
        else {
            ++ self -> total;
            self -> ended = sizeof self -> n / sizeof self -> n [ 0 ] - 1;
            for ( i = 0; i < sizeof self -> n / sizeof self -> n [ 0 ]; ++ i )
                if ( self -> n [ i ] == 0 ) {
                    self -> ended = i - 1;
                    break;
                }
        }
    }
    char b [ 512 ] = "";
    rc_t rc = string_vprintf ( b, sizeof b, NULL, fmt, args );
    if ( rc != 0 )
        OUTMSG ( ( "CANNOT PRINT: %R", rc ) );
    else {
        if ( ok == eFAIL || ok == eOK || ok == eDONE ) {
            rc = OUTMSG ( ( "< %d", self -> n [ 0 ] ) );
            for ( i = 1; i <= self -> ended; ++ i )
                OUTMSG ( ( ".%d", self -> n [ i ] ) );
            for ( ; i < sizeof self -> n / sizeof self -> n [ 0 ]; ++ i )
                if ( self -> n [ i ] != 0 )
                    self -> n [ i ] = 0;
                else
                    break;
            OUTMSG ( ( " " ) );
        }
        OUTMSG ( ( b ) );
        switch ( ok ) {
            case eFAIL: OUTMSG ( ( ": FAILURE\n" ) ); break;
            case eOK  : OUTMSG ( ( ": OK\n"      ) ); break;
            case eEND :
            case eDONE: OUTMSG ( (     "\n"      ) ); break;
            default   :                               break;
        }
    }
    return rc;
}
static rc_t STestEnd ( STest * self, EOK ok, const char * fmt, ...  )  {
    va_list args;
    va_start ( args, fmt );
    rc_t rc = STestVEnd ( self, ok, fmt, args );
    va_end ( args );
    return rc;
}
static rc_t STestStart ( STest * self, bool checking,
                         const char * fmt, ...  )
{
    va_list args;
    va_start ( args, fmt );
    rc_t rc = STestVStart ( self, checking, fmt, args );
    va_end ( args );
    return rc;
}
typedef struct {
    VPath * vpath;
    const String * acc;
} Data;
static rc_t DataInit ( Data * self, const struct VFSManager * mgr,
                       const char * path )
{
    assert ( self );
    memset ( self, 0, sizeof * self );
    rc_t rc = VFSManagerMakePath ( mgr, & self -> vpath, path );
    if ( rc != 0 )
        OUTMSG ( ( "VFSManagerMakePath(%s) = %R\n", path, rc ) );
    else {
        VPath * vacc = NULL;
        rc = VFSManagerExtractAccessionOrOID ( mgr, & vacc, self -> vpath );
        if ( rc != 0 )
            rc = 0;
        else {
            String acc;
            rc = VPathGetPath ( vacc, & acc );
            if ( rc == 0 )
                StringCopy ( & self -> acc, & acc );
            else
                OUTMSG ( ( "Cannot VPathGetPath"
                           "(VFSManagerExtractAccessionOrOID(%R))\n", rc ) );
        }
    }
    return rc;
}
static rc_t DataFini ( Data * self ) {
    assert ( self );
    free ( ( void * ) self -> acc );
    rc_t rc = VPathRelease ( self -> vpath );
    memset ( self, 0, sizeof * self );
    return rc;
}
static const ver_t HTTP_VERSION = 0x01010000;
static rc_t STestCheckFileSize ( STest * self, const String * path,
                                 uint64_t * sz )
{
    rc_t rc = 0;
    const KFile * file = NULL;
    assert ( self );
    STestStart ( self, false,
                 "KFile = KNSManagerMakeReliableHttpFile(%S):", path );
    rc = KNSManagerMakeReliableHttpFile ( self -> kmgr, & file, NULL,
                                          HTTP_VERSION, "%S", path );
    if ( rc != 0 )
        STestEnd ( self, eEND, "FAILURE: %R", rc );
    else {
        if ( rc == 0 ) {
            STestEnd ( self, eEND, "OK" );
            rc = STestStart ( self, false, "KFileSize(KFile(%S)) =", path );
            rc = KFileSize ( file, sz );
            if ( rc == 0 )
                STestEnd ( self, eEND, "%lu: OK", * sz );
            else
                STestEnd ( self, eEND, "FAILURE: %R", rc );
        }
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    KFileRelease ( file );
    file = NULL;
    return rc;
}
static
rc_t STestCheckRanges ( STest * self, const Data * data, uint64_t sz )
{
    assert ( self && data );
    rc_t rc = STestStart ( self, true, "Support of Range requests" );
    KClientHttp * http = NULL;
    assert ( self && data );
    String host;
    rc = VPathGetHost ( data -> vpath, & host );
    if ( rc != 0 )
        OUTMSG ( ( "Cannot VPathGetHost(%R)\n", rc ) );
    String scheme;
    if ( rc == 0 )
        rc = VPathGetScheme ( data -> vpath, & scheme );
    if ( rc != 0 )
        OUTMSG ( ( "Cannot VPathGetScheme(%R)\n", rc ) );
    bool https = false;
    if ( rc == 0 ) {
        String sHttps;
        String sHttp;
        CONST_STRING ( & sHttp, "http" );
        CONST_STRING ( & sHttps, "https" );
        if ( StringEqual ( & scheme, & sHttps ) )
            https = true;
        else if ( StringEqual ( & scheme, & sHttp ) )
            https = false;
        else {
            OUTMSG ( ( "Unexpected scheme '(%S)'\n", & scheme ) );
            return 0;
        }
    }
    if ( rc == 0 ) {
        if ( https ) {
            STestStart ( self, false, "KClientHttp = "
                         "KNSManagerMakeClientHttps(%S):", & host );
            rc = KNSManagerMakeClientHttps ( self -> kmgr, & http, NULL,
                                             HTTP_VERSION, & host, 0 );
        }
        else {
            STestStart ( self, false, "KClientHttp = "
                         "KNSManagerMakeClientHttp(%S):", & host );
            rc = KNSManagerMakeClientHttp ( self -> kmgr, & http, NULL,
                                            HTTP_VERSION, & host, 0 );
        }
        if ( rc == 0 )
            STestEnd ( self, eEND, "OK" );
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    KHttpRequest * req = NULL;
    if ( rc == 0 ) {
        String path;
        rc = VPathGetPath ( data -> vpath, & path );
        if ( rc != 0 )
            OUTMSG ( ( "Cannot VPathGetPath(%R)\n", rc ) );
        else {
            rc = KHttpMakeRequest ( http, & req, "%S", & path );
            if ( rc != 0 )
                OUTMSG ( ( "KHttpMakeRequest(%S) = %R\n", & path, rc ) );
        }
    }
    KHttpResult * rslt = NULL;
    if ( rc == 0 ) {
        STestStart ( self, false, "KHttpResult = "
            "KHttpRequestHEAD(KHttpMakeRequest(KClientHttp)):" );
        rc = KHttpRequestHEAD ( req, & rslt );
        if ( rc == 0 )
            STestEnd ( self, eEND, "OK" );
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    char buffer [ 1024 ] = "";
    size_t num_read = 0;
    if ( rc == 0 ) {
        STestStart ( self, false,
                     "KHttpResultGetHeader(KHttpResult, Accept-Ranges) =" );
        rc = KHttpResultGetHeader ( rslt, "Accept-Ranges",
                                    buffer, sizeof buffer, & num_read );
        if ( rc == 0 ) {
            const char bytes [] = "bytes";
            if ( string_cmp ( buffer, num_read, bytes, sizeof bytes - 1,
                              sizeof bytes - 1 ) == 0 )
            {
                STestEnd ( self, eEND, "'%.*s': OK",
                                        ( int ) num_read, buffer );
            }
            else {
                STestEnd ( self, eEND, "'%.*s': FAILURE",
                                        ( int ) num_read, buffer );
                rc = RC ( rcExe, rcFile, rcOpening, rcFunction, rcUnsupported );
            }
        }
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    KHttpResultRelease ( rslt );
    rslt = NULL;
    uint64_t pos = 0;
    size_t bytes = 4096;
    size_t ebytes = bytes;
    if ( sz < ebytes )
        ebytes = sz;
    if ( sz > bytes * 2 )
        pos = sz / 2;
    if ( rc == 0 ) {
        STestStart ( self, false, "KHttpResult = KHttpRequestByteRange"
                        "(KHttpMakeRequest, %lu, %zu):", pos, bytes );
        rc = KHttpRequestByteRange ( req, pos, bytes );
        if ( rc == 0 )
            STestEnd ( self, eEND, "OK" );
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    if ( rc == 0 ) {
        STestStart ( self, false,
            "KHttpResult = KHttpRequestGET(KHttpMakeRequest(KClientHttp)):" );
        rc = KHttpRequestGET ( req, & rslt );
        if ( rc == 0 )
            STestEnd ( self, eEND, "OK" );
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    if ( rc == 0 ) {
        uint64_t po = 0;
        size_t byte = 0;
        rc = KClientHttpResultRange ( rslt, & po, & byte );
        if ( rc == 0 ) {
            if ( po != pos || ( ebytes > 0 && byte != ebytes ) ) {
                STestStart ( self, false,
                             "KClientHttpResultRange(KHttpResult,&p,&b):" );
                STestEnd ( self, eEND, "FAILURE: expected:{%lu,%zu}, "
                            "got:{%lu,%zu}", pos, ebytes, po, byte );
                rc = RC ( rcExe, rcFile, rcReading, rcRange, rcOutofrange );
            }
        }
        else {
            STestStart ( self, false, "KClientHttpResultRange(KHttpResult):" );
            STestEnd ( self, eEND, "FAILURE: %R", rc );
        }
    }
    if ( rc == 0 ) {
        STestStart ( self, false,
                     "KHttpResultGetHeader(KHttpResult, Content-Range) =" );
        rc = KHttpResultGetHeader ( rslt, "Content-Range",
                                    buffer, sizeof buffer, & num_read );
        if ( rc == 0 )
            STestEnd ( self, eEND, "'%.*s': OK",
                                    ( int ) num_read, buffer );
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    KHttpResultRelease ( rslt );
    rslt = NULL;
    KHttpRequestRelease ( req );
    req = NULL;
    KHttpRelease ( http );
    http = NULL;
    STestEnd ( self, rc == 0 ? eOK : eFAIL, "Support of Range requests" );
    return rc;
}
static rc_t STestCheckStreamRead ( STest * self, const KStream * stream,
    uint64_t sz, bool print, const char * exp, size_t esz )
{
    rc_t rc = 0;
    size_t total = 0;
    STestStart ( self, false, "KStreamRead(KHttpResult):" );
    char buffer [ 1024 ] = "";
    while ( rc == 0 ) {
        size_t num_read = 0;
        rc = KStreamRead ( stream, buffer, sizeof buffer, & num_read );
        if ( rc != 0 )
            STestEnd ( self, eEND, "FAILURE: %R", rc );
        else if ( num_read != 0 ) {
            if ( total == 0 && esz > 0 ) {
                int i = 0;
                int s = esz;
                if ( num_read < esz )
                    s = num_read;
                STestEnd ( self, eMGS, "'" );
                for ( i = 0; i < s; ++ i ) {
                    if ( isprint ( buffer [ i ] ) )
                        STestEnd ( self, eMGS, "%c", buffer [ i ] );
                    else if ( buffer [ i ] == 0 )
                        STestEnd ( self, eMGS, "\\0" );
                    else
                        STestEnd ( self, eMGS, "\\%03o",
                                               ( unsigned char ) buffer [ i ] );
                }
                STestEnd ( self, eMGS, "': " );
                if ( string_cmp ( buffer, num_read, exp, esz, esz ) != 0 ) {
                    STestEnd ( self, eEND, " FAILURE: bad content" );
                    rc = RC ( rcExe, rcFile, rcReading, rcString, rcUnequal );
                }
            }
            total += num_read;
        }
        else {
            assert ( num_read == 0 );
            if ( total == sz ) {
                if ( print ) {
                    if ( total >= sizeof buffer )
                        buffer [ sizeof buffer - 1 ] = '\0';
                    else {
                        buffer [ total ] = '\0';
                        while ( total > 0 ) {
                            -- total;
                            if ( buffer [ total ] == '\n' )
                                buffer [ total ] = '\0';
                            else
                                break;
                        }
                    }
                    STestEnd ( self, eMGS, "%s: ", buffer );
                }
                STestEnd ( self, eEND, "OK" );
            }
            else
                STestEnd ( self, eEND, "%s: SIZE DO NOT MATCH (%zu)\n", total );
            break;
        }
    }
    return rc;
}
static rc_t STestCheckHttpUrl ( STest * self, const Data * data, bool print,
                                const char * exp, size_t esz )
{
    KHttpRequest * req = NULL;
    KHttpResult * rslt = NULL;
    assert ( self && data );
    const String * full = NULL;
    rc_t rc = VPathMakeString ( data -> vpath, & full );
    if ( rc != 0 )
        OUTMSG ( ( "CANNOT VPathMakeString: %R\n", rc ) );
    if ( rc == 0 )
        rc = STestStart ( self, true, "Access to '%S'", full );
    uint64_t sz = 0;
    if ( rc == 0 )
        rc = STestCheckFileSize ( self, full, & sz );
    if ( rc == 0 )
        rc = STestCheckRanges ( self, data, sz );
    if ( rc == 0 ) {
        STestStart ( self, false,
                     "KHttpRequest = KNSManagerMakeRequest(%S):", full );
        rc = KNSManagerMakeRequest ( self -> kmgr, & req,
                                     HTTP_VERSION, NULL, "%S", full );
        if ( rc == 0 )
            STestEnd ( self, eEND, "OK"  );
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    if ( rc == 0 ) {
        STestStart ( self, false,
                     "KHttpResult = KHttpRequestGET(KHttpRequest):" );
        rc = KHttpRequestGET ( req, & rslt );
        if ( rc == 0 )
            STestEnd ( self, eEND, "OK" );
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    if ( rc == 0 ) {
        uint32_t code = 0;
        STestStart ( self, false, "KHttpResultStatus(KHttpResult) =" );
        rc = KHttpResultStatus ( rslt, & code, NULL, 0, NULL );
        if ( rc != 0 )
            STestEnd ( self, eEND, "FAILURE: %R", rc );
        else {
            STestEnd ( self, eMGS, "%u: ", code );
            if ( code == 200 )
                STestEnd ( self, eEND, "OK" );
            else {
                STestEnd ( self, eEND, "FAILURE" );
                rc = RC ( rcExe, rcFile, rcReading, rcFile, rcInvalid );
            }
        }
    }
    if ( rc == 0 ) {
        KStream * stream = NULL;
        rc = KHttpResultGetInputStream ( rslt, & stream );
        if ( rc != 0 )
            OUTMSG ( (
                "KHttpResultGetInputStream(KHttpResult) = %R\n", rc ) );
        else
            rc = STestCheckStreamRead ( self, stream, sz, print, exp, esz );
        KStreamRelease ( stream );
        stream = NULL;
    }
    STestEnd ( self, rc == 0 ? eOK : eFAIL, "Access to '%S'", full );
    free ( ( void * ) full );
    full = NULL;
    return rc;
}
bool DataIsAccession ( const Data * self ) {
    assert ( self );
    if ( self -> acc == NULL )
        return false;
    else
        return self -> acc -> size != 0;
}
static rc_t STestCheckVfsUrl ( STest * self, const Data * data ) {
    assert ( self && data );
    if ( ! DataIsAccession ( data ) )
        return 0;
    String path;
    rc_t rc = VPathGetPath ( data -> vpath, & path );
    if ( rc != 0 ) {
        OUTMSG ( ( "Cannot VPathGetPath(%R)", rc ) );
        return rc;
    }
    const KDirectory * d = NULL;
    STestStart ( self, false, "VFSManagerOpenDirectoryRead(%S):", & path );
    rc = VFSManagerOpenDirectoryRead ( self -> vmgr, & d, data -> vpath );
    if ( rc == 0 )
        STestEnd ( self, eEND, "OK"  );
    else
        STestEnd ( self, eEND, "FAILURE: %R", rc );
    RELEASE ( KDirectory, d );
    return rc;
}
static rc_t STestCheckUrlImpl ( STest * self, const Data * data, bool print,
                                const char * exp, size_t esz )
{
    rc_t rc = STestCheckHttpUrl ( self, data, print, exp, esz );
    rc_t r2 = STestCheckVfsUrl  ( self, data );
    return rc != 0 ? rc : r2;
}
static rc_t STestCheckUrl ( STest * self, const Data * data, bool print,
                            const char * exp, size_t esz )
{
    assert ( data );
    String path;
    rc_t rc = VPathGetPath ( data -> vpath, & path );
    if ( rc != 0 ) {
        OUTMSG ( ( "Cannot VPathGetPath(%R)", rc ) );
        return rc;
    }
    if ( path . size == 0 ) /* does not exist */
        return 0;
    return STestCheckUrlImpl ( self, data, print, exp, esz );
}
static String * KConfig_Resolver ( const KConfig * self ) {
    String * s = NULL;
    rc_t rc = KConfigReadString ( self,
                                  "tools/test-sra/diagnose/resolver-cgi", & s );
    if ( rc != 0 ) {
        String str;
        CONST_STRING ( & str,
                       "https://www.ncbi.nlm.nih.gov/Traces/names/names.cgi" );
        rc = StringCopy ( ( const String ** ) & s, & str );
        assert ( rc == 0 );
    }
    assert ( s );
    return s;
}
static const char * STestCallCgi ( STest * self, const String * acc,
    char * response, size_t response_sz, size_t * resp_read )
{
    const char * url = NULL;
    assert ( self );
    KHttpRequest * req = NULL;
    STestStart ( self, true, "Access to '%S'", acc );
    const String * cgi = KConfig_Resolver ( self -> kfg );
    STestStart ( self, false,
        "KHttpRequest = KNSManagerMakeReliableClientRequest(%S):", cgi );
    rc_t rc = KNSManagerMakeReliableClientRequest ( self -> kmgr, & req,
        HTTP_VERSION, NULL, "%S", cgi);
    if ( rc == 0 )
        STestEnd ( self, eEND, "OK"  );
    else
        STestEnd ( self, eEND, "FAILURE: %R", rc );
    if ( rc == 0 ) {
        const char param [] = "accept-proto";
        rc = KHttpRequestAddPostParam ( req, "%s=https,http,fasp", param );
        if ( rc != 0 )
            OUTMSG ( ( "KHttpRequestAddPostParam() = %R\n", rc ) );
    }
    if ( rc == 0 ) {
        const char param [] = "object";
        rc = KHttpRequestAddPostParam ( req, "%s=0||%S", param, acc );
        if ( rc != 0 )
            OUTMSG ( ( "KHttpRequestAddPostParam() = %R\n", rc ) );
    }
    if ( rc == 0 ) {
        const char param [] = "version";
        rc = KHttpRequestAddPostParam ( req, "%s=3.0", param );
        if ( rc != 0 )
            OUTMSG ( ( "KHttpRequestAddPostParam() = %R\n", rc ) );
    }
    KHttpResult * rslt = NULL;
    if ( rc == 0 ) {
        STestStart ( self, false, "KHttpRequestPOST(KHttpRequest(%S)):", cgi );
        rc = KHttpRequestPOST ( req, & rslt );
        if ( rc == 0 )
            STestEnd ( self, eEND, "OK"  );
        else
            STestEnd ( self, eEND, "FAILURE: %R", rc );
    }
    rc_t rs = 0;
    if ( rc == 0 ) {
        uint32_t code = 0;
        STestStart ( self, false, "KHttpResultStatus(KHttpResult(%S)) =", cgi );
        rc = KHttpResultStatus ( rslt, & code, NULL, 0, NULL );
        if ( rc != 0 )
            STestEnd ( self, eEND, "FAILURE: %R", rc );
        else {
            STestEnd ( self, eMGS, "%u: ", code );
            if ( code == 200 )
                STestEnd ( self, eEND, "OK" );
            else {
                STestEnd ( self, eEND, "FAILURE" );
                rs = RC ( rcExe, rcFile, rcReading, rcFile, rcInvalid );
            }
        }
    }
    KStream * stream = NULL;
    if ( rc == 0 ) {
        rc = KHttpResultGetInputStream ( rslt, & stream );
        if ( rc != 0 )
            OUTMSG ( ( "KHttpResultGetInputStream() = %R\n", rc ) );
    }
    if ( rc == 0 ) {
        assert ( resp_read );
        STestStart ( self, false, "KStreamRead(KHttpResult(%S)) =", cgi );
        rc = KStreamRead ( stream, response, response_sz, resp_read );
        if ( rc != 0 )
            STestEnd ( self, eEND, "FAILURE: %R", rc );
        else {
            if ( * resp_read > response_sz - 4 ) {
                response [ response_sz - 4 ] = '.';
                response [ response_sz - 3 ] = '.';
                response [ response_sz - 2 ] = '.';
                response [ response_sz - 1 ] = '\0';
            }
            else {
                response [ * resp_read + 1 ] = '\0';
                for ( ; * resp_read > 0 && ( response [ * resp_read ] == '\n' ||
                                             response [ * resp_read ] == '\0' );
                      --  ( * resp_read ) )
                {
                    response [ * resp_read ] = '\0';
                }
            }
            STestEnd ( self, eEND, "'%s'", response );
            if ( rs == 0 ) {
                int i = 0;
                int p = 0;
                for ( i = 0; p < * resp_read ; ++ i ) {
                    char * n = string_chr ( response + p,
                                            * resp_read - p, '|' );
                    if ( n != NULL )
                        p = n - response + 1;
                    if ( i == 6 ) {
                        url = n + 1;
                        break;
                    }
                }
            }
        }
    }
    if ( rc == 0 )
        rc = rs;
    KHttpResultRelease ( rslt );
    rslt = NULL;
    KHttpRequestRelease ( req );
    req = NULL;
    free ( ( void * ) cgi );
    cgi = NULL;
    return url;
}
static rc_t STestCheckAcc ( STest * self, const Data * data,
                            bool print, const char * exp, size_t esz )
{
    rc_t rc = 0;
    assert ( self && data );
    char response [ 4096 ] = "";
    size_t resp_len = 0;
    const char * url = NULL;
    String acc;
    memset ( & acc, 0, sizeof acc );
    if ( DataIsAccession ( data ) ) {
        acc = * data -> acc;
        url = STestCallCgi ( self, & acc,
                             response, sizeof response, & resp_len );
    }
    bool checked = false;
    if ( url != NULL ) {
        char * p = string_chr ( url, resp_len - ( url - response ), '|' );
        if ( p == NULL ) {
            OUTMSG (( "UNEXPECTED RESOLVER RESPONSE\n" ));
            rc = RC ( rcExe, rcString ,rcParsing, rcString, rcIncorrect );
        }
        else {
            const String * full = NULL;
            rc_t rc = VPathMakeString ( data -> vpath, & full );
            if ( rc != 0 )
                OUTMSG ( ( "CANNOT VPathMakeString: %R\n", rc ) );
            char * d = string_chr ( url, resp_len - ( url - response ), '$' );
            if ( d == NULL )
                d = p;
            while ( d != NULL && d <= p ) {
                if ( ! checked && full != NULL && string_cmp ( full -> addr,
                                    full -> size, url, d - url, d - url ) == 0 )
                {
                    checked = true;
                }
                * d = '\0';
                if ( * url == 'h' ) {
                    Data dt;
                    if ( rc == 0 )
                        rc = DataInit ( & dt, self -> vmgr, url );
                    if ( rc == 0 ) {
                        rc_t r1 = STestCheckUrl ( self, & dt, print, exp, esz );
                        if ( rc == 0 )
                            rc = r1;
                    }
                    DataFini ( & dt );
                }
                if ( d == p )
                    break;
                url = d + 1;
                d = string_chr ( d, resp_len - ( d - response ), '$' );
                if ( d > p )
                    d = p;
            }
            free ( ( void * ) full );
            full = NULL;
        }
    }
    if ( ! checked ) {
        rc_t r1 = STestCheckUrl ( self, data, print, exp, esz );
        if ( rc == 0 )
            rc = r1;
    }
    if ( acc . size != 0 )
        STestEnd ( self, rc == 0 ? eOK : eFAIL, "Access to '%S'", & acc );
    return rc;
}
/******************************************************************************/
static rc_t STestCheckNetwork ( STest * self, const Data * data,
    const char * exp, size_t esz, const Data * data2,
    const char * fmt, ... )
{
    KEndPoint ep;
    va_list args;
    va_start ( args, fmt );
    char b [ 512 ] = "";
    rc_t rc = string_vprintf ( b, sizeof b, NULL, fmt, args );
    if ( rc != 0 )
        OUTMSG ( ( "CANNOT PREPARE MEGGAGE: %R\n", rc ) );
    va_end ( args );
    assert ( self && data );
    STestStart ( self, true, b );
    String host;
    rc = VPathGetHost ( data -> vpath, & host );
    if ( rc != 0 )
        OUTMSG ( ( "Cannot VPathGetHost(%R)", rc ) );
    else {
        uint16_t port = 443;
        STestStart ( self, false, "KNSManagerInitDNSEndpoint(%S:%hu) =",
                                  & host, port );
        rc = KNSManagerInitDNSEndpoint ( self -> kmgr, & ep, & host, port );
        if ( rc != 0 )
            STestEnd ( self, eEND, "FAILURE: %R", rc );
        else {
            char endpoint [ 1024 ] = "";
            rc_t rx = endpoint_to_string ( endpoint, sizeof endpoint, & ep );
            if ( rx != 0 )
                STestEnd ( self, eEND, "CANNOT CONVERT TO STRING" );
            else
                STestEnd ( self, eEND, "'%s': OK", endpoint );
        }
        port = 80;
        STestStart ( self, false, "KNSManagerInitDNSEndpoint(%S:%hu) =",
                                  & host, port );
        rc_t r1 = KNSManagerInitDNSEndpoint ( self -> kmgr, & ep,
                                              & host, port );
        if ( r1 != 0 )
            STestEnd ( self, eEND, "FAILURE: %R", r1 );
        else {
            char endpoint [ 1024 ] = "";
            rc_t rx = endpoint_to_string ( endpoint, sizeof endpoint, & ep );
            if ( rx != 0 )
                STestEnd ( self, eEND, "CANNOT CONVERT TO STRING" );
            else
                STestEnd ( self, eEND, "'%s': OK", endpoint );
        }
        if ( rc == 0 ) {
            rc = STestCheckAcc ( self, data, false, exp, esz );
            if ( data2 != NULL ) {
                rc_t r2 = STestCheckAcc ( self, data2, true, 0, 0 );
                if ( rc == 0 )
                    rc = r2;
            }
        }
        if ( rc == 0 )
            rc = r1;
    }
    STestEnd ( self, rc == 0 ? eOK : eFAIL, b );
    return rc;
}
rc_t MainQuickCheck ( const KConfig * kfg, const KNSManager * kmgr,
                      const struct VFSManager * vmgr )
{
    const char exp [] = "NCBI.sra\210\031\003\005\001\0\0\0";
    rc_t rc = 0;
    rc_t r1 = 0;
    STest t;
    STestInit ( & t, kfg, kmgr, vmgr );
    rc = STestStart ( & t, true, "Network" );
    {
#undef  HOST
#define HOST "www.ncbi.nlm.nih.gov"
        String h;
        CONST_STRING ( & h, HOST );
        Data d;
        rc_t r2 = DataInit ( & d, vmgr, "https://" HOST );
        if ( r2 == 0 )
            r2 = STestCheckNetwork ( & t, & d, 0, 0,
                                     NULL, "Access to '%S'", & h );
        if ( r1 == 0 )
            r1 = r2;
        DataFini ( & d );
    }
    {
#undef  HOST
#define HOST "sra-download.ncbi.nlm.nih.gov"
        String h;
        CONST_STRING ( & h, HOST );
        Data d;
        rc_t r2 = DataInit ( & d, vmgr, "https://" HOST "/srapub/SRR042846" );
        if ( r2 == 0 )
            r2 = STestCheckNetwork ( & t, & d, exp, sizeof exp - 1,
                                     NULL, "Access to '%S'", & h );
        if ( r1 == 0 )
            r1 = r2;
        DataFini ( & d );
    }
    {
#undef  HOST
#define HOST "ftp-trace.ncbi.nlm.nih.gov"
        String h;
        CONST_STRING ( & h, HOST );
        Data d;
        Data v;
        rc_t r2 = DataInit ( & d, vmgr,
            "https://" HOST "/sra/refseq/KC702174.1" );
        if ( r2 == 0 )
            r2 = DataInit ( & v, vmgr,
                "https://" HOST "/sra/sdk/current/sratoolkit.current.version" );
        if ( r2 == 0 )
            r2 = STestCheckNetwork ( & t, & d, exp, sizeof exp - 1,
                                     & v, "Access to '%S'", & h );
        if ( r1 == 0 )
            r1 = r2;
        DataFini ( & d );
    }
    {
#undef  HOST
#define HOST "gap-download.ncbi.nlm.nih.gov"
        String h;
        CONST_STRING ( & h, HOST );
        Data d;
        rc_t r2 = DataInit ( & d, vmgr, "https://" HOST );
        if ( r2 == 0 )
            r2 = STestCheckNetwork ( & t, & d, NULL, 0, 
                                     NULL, "Access to '%S'", & h );
        if ( r1 == 0 )
            r1 = r2;
        DataFini ( & d );
    }
    STestEnd ( & t, r1 == 0 ? eOK : eFAIL, "Network" );
    if ( rc == 0 )
        rc = r1;
/*SRR042846 !!!
0 13769 => SRR053325 Twice weekly longitudinal vaginal sampling
1 13997 => SRR045450 2010 Human Microbiome Project filtered data is public 
srapath SRR015685 200000000
      0 30461 => SRR013401 2011 proteobacteria Brucella genome shotg. sequencing
srapath SRR010945
      0 31609 => SRR002749 2008 454 sequencing of Human immunodeficiency virus 1
srapath SRR002682
      0 57513 => SRR000221 Nitrosopumilus maritimus SCM1 FGBT genomic fragment*/
    //if ( r1 == 0 )        OUTMSG ( ( "< 1 Network: OK\n" ) );    else        OUTMSG ( ( "< 1 Network: FAILURE\n" ) );
    /*OUTMSG ( ( ">>> Checking configuration:\n" ) );
//  OUTMSG ( ( "  Site repository: " ) );    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( ">> Checking remote repository: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking main/resolver-cgi node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking protected/resolver-cgi node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking /repository/remote/disabled node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking that caching is enabled: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "/repository/user/cache-disabled" ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( ">> Checking main user repository: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking /user/main/root node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking user/main/apps/sra node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking user/main/apps/refseq node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking user/main/apps/wgs node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking user/main/apps/file node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking user/main/apps/nannot node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "> Checking user/main/apps/nakmer node: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  Protected repositories: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "Network:\n" ) );
    OUTMSG ( ( "  Proxy: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  Access to NCBI toolkit version file: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "    InitDNSEndpoint: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "    Toolkit version: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  Toolkit version CGI: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  Resolver CGI: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  Access to SRA download site: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  HTTP download test: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  Ascp download test: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  Site resolving: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  Remote resolving: " ) );
    OUTMSG ( ( "\n" ) );
    OUTMSG ( ( "  Cache resolving: " ) );
    OUTMSG ( ( "\n" ) );*/
/*    rc_t rc2 = 0;
    const char acc[] = "SRR000001";
    const char path[] = "/repository/remote/protected/CGI/resolver-cgi";
    const KConfigNode *node = NULL;
    assert(self);
    rc = KConfigOpenNodeRead(self->cfg, &node, "%s", path);
    if (rc == 0) {
        OUTMSG(("configuration: found\n"));
    }
    else {
        OUTMSG(("ERROR: configuration not found or incomplete\n"));
    }
    if (rc == 0) {
        rc_t rc3 = MainCallCgi(self, node, acc);
        if (rc3 != 0 && rc2 == 0) {
            rc2 = rc3;
        }
        rc3 = MainQuickResolveQuery(self, acc);
        if (rc3 != 0 && rc2 == 0) {
            rc2 = rc3;
        }
    }
    RELEASE(KConfigNode, node);
    if (rc2 != 0 && rc == 0) {
        rc = rc2;
    }
        */
    STestFini ( & t );
    return rc;
}
