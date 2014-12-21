/******************************************************************************
 *
 * File:        utils.c        
 *
 * Created:     12/2012
 *
 * Author:      Pavel Sakov
 *              Bureau of Meteorology
 *
 * Description:
 *
 * Revisions:
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <values.h>
#include <execinfo.h>
#include "ncw.h"
#include "definitions.h"
#include "version.h"
#include "nan.h"
#include "utils.h"

#define FILE_FIND_INC 10
#define EPS_DOUBLE 4.0e-15
#define EPS_FLOAT  1.0e-7
#define BACKTRACE_SIZE 50

/**
 */
void enkf_init(int* argc, char*** argv)
{
#if defined(MPI)
#include <unistd.h>
    int size;

    MPI_Init(argc, argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocesses);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    enkf_printf("  MPI: initialised %d processes\n", nprocesses);
    fflush(stdout);
    MPI_Barrier(MPI_COMM_WORLD);
    printf("  MPI: rank = %d, PID = %d\n", rank, getpid());
    {
        int flag = 0;

        while (rank == 0 && flag)
            sleep(1);
        MPI_Barrier(MPI_COMM_WORLD);
    }

    MPI_Type_size(MPI_INTEGER, &size);
    assert(size == sizeof(int));
#endif

    ncw_set_quitfn(enkf_quit);
}

/**
 */
void enkf_finish(void)
{
#if defined(MPI)
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    enkf_printtime("  ");
    enkf_printf("  finished\n");
    enkf_flush();
#if defined(MPI)
    MPI_Finalize();
#endif
}

/**
 */
void enkf_quit(char* format, ...)
{
    va_list args;

    fflush(stdout);

    fprintf(stderr, "\n\n  ERROR: enkf: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n\n");
    enkf_printtime("  ");

    if (enkf_exitaction == EXITACTION_BACKTRACE) {
        void* buffer[BACKTRACE_SIZE];
        size_t size;
        char** strings;
        size_t i;

        fprintf(stderr, "\n  I am %d, now printing the backtrace:\n\n", rank);
        size = backtrace(buffer, BACKTRACE_SIZE);
        strings = backtrace_symbols(buffer, size);
        fprintf(stderr, "  obtained %zd stack frames:\n", size);
        for (i = 0; i < size; i++)
            fprintf(stderr, "%s\n", strings[i]);
        free(strings);
    } else if (enkf_exitaction == EXITACTION_SEGFAULT) {
        fprintf(stderr, "\n  now generating a segfault:\n\n");
        fflush(NULL);           /* flush all streams */
    }
#if defined(MPI)
    MPI_Abort(MPI_COMM_WORLD, 1);       /* kill all MPI jobs */
#endif

    exit(1);
}

/**
 */
int enkf_printf(const char* format, ...)
{
    va_list args;
    int status = 0;

    if (enkf_verbose && rank == 0) {
        va_start(args, format);
        status = vprintf(format, args);
        va_end(args);
    }

    return status;
}

/**
 */
void enkf_flush(void)
{
    if (rank == 0)
        fflush(stdout);
}

/**
 */
FILE* enkf_fopen(const char* fname, const char* mode)
{
    FILE* f = NULL;

    if ((f = fopen(fname, mode)) == NULL) {
        int errno_saved = errno;

        enkf_quit("  error: enkf_fopen(): could not open \"%s\": %s", fname, strerror(errno_saved));
    }

    return f;
}

/**
 */
void enkf_printtime(const char offset[])
{
    time_t t;
    struct tm tm;

    if (rank != 0)
        return;

    t = time(NULL);
    tm = *localtime(&t);

    printf("%s%d-%02d-%02d %02d:%02d:%02d\n", offset, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

/**
 */
void enkf_printcompileflags(const char offset[])
{
    enkf_printf("%scompile flags:\n", offset);
#if defined(CHECK_X5)
    enkf_printf("%s  CHECK_X5         = [+]\n", offset);
#else
    enkf_printf("%s  CHECK_X5         = [-]\n", offset);
#endif
#if defined(CHECK_G)
    enkf_printf("%s  CHECK_G          = [+]\n", offset);
#else
    enkf_printf("%s  CHECK_G          = [-]\n", offset);
#endif
#if defined(SHUFFLE_ROWS)
    enkf_printf("%s  SHUFFLE_ROWS     = [+]\n", offset);
#else
    enkf_printf("%s  SHUFFLE_ROWS     = [-]\n", offset);
#endif
#if defined(HE_VIAFILE)
    enkf_printf("%s  HE_VIAFILE       = [+]\n", offset);
#else
    enkf_printf("%s  HE_VIAFILE       = [-]\n", offset);
#endif
#if defined(GRIDNODES_WRITE)
    enkf_printf("%s  GRIDNODES_WRITE  = [+]\n", offset);
#else
    enkf_printf("%s  GRIDNODES_WRITE  = [-]\n", offset);
#endif
}

/**
 */
void enkf_printflags(const char offset[])
{
    enkf_printf("%sEnKF flags:\n", offset);
    if (enkf_exitaction == EXITACTION_BACKTRACE)
        enkf_printf("%s  enkf_exitaction  = [BACKTRACE]\n", offset);
    else if (enkf_exitaction == EXITACTION_SEGFAULT)
        enkf_printf("%s  enkf_exitaction  = [SEGFAULT]\n", offset);
    if (enkf_obstype == OBSTYPE_VALUE)
        enkf_printf("%s  enkf_obstype     = [VALUE]\n", offset);
    else if (enkf_obstype == OBSTYPE_INNOVATION)
        enkf_printf("%s  enkf_obstype     = [INNOVATION]\n", offset);
    enkf_printf("%s  enkf_separateout = [%c]\n", offset, (enkf_separateout) ? '+' : '-');
    enkf_printf("%s  enkf_directwrite = [%c]\n", offset, (enkf_directwrite) ? '+' : '-');
    enkf_printcompileflags(offset);
}

/**
 */
void enkf_printversion(void)
{
    enkf_printf("  EnKF version %s\n", ENKF_VERSION);
    enkf_printcompileflags("  ");
}

/** Finds files matching the template by using Unix "ls" command and
 ** adds them to the current list.
 */
void find_files(char* template, int* nfiles, char*** fnames)
{
    char command[MAXSTRLEN];

    FILE* in;
    char buf[MAXSTRLEN];
    char* eol;

    sprintf(command, "ls -1 %s", template);
    in = popen(command, "r");
    if (in == NULL)
        return;

    while (fgets(buf, MAXSTRLEN, in) != NULL) {
        if (*nfiles % FILE_FIND_INC == 0)
            *fnames = realloc(*fnames, (*nfiles + FILE_FIND_INC) * sizeof(char*));
        eol = strchr(buf, '\n');
        if (eol != NULL)
            *eol = 0;
        (*fnames)[*nfiles] = strdup(buf);
        (*nfiles)++;
    }

    pclose(in);
}

#ifdef PAPAL                    /* Pope Gregory XIII's decree */
#define LASTJULDATE 15821004L   /* last day to use Julian calendar */
#define LASTJULJDN  2299160L    /* jdn of same */
#else                           /* British-American usage */
#define LASTJULDATE 17520902L   /* last day to use Julian calendar */
#define LASTJULJDN  2361221L    /* jdn of same */
#endif

/**
 */
static long ymd_to_jdnl(int y, int m, int d, int julian)
{
    long jdn;

    if (julian < 0)             /* set Julian flag if auto set */
        julian = (((y * 100L) + m) * 100 + d <= LASTJULDATE);

    if (y < 0)                  /* adjust BC year */
        y++;

    if (julian)
        jdn = 367L * y - 7 * (y + 5001L + (m - 9) / 7) / 4 + 275 * m / 9 + d + 1729777L;
    else
        jdn = (long) (d - 32076)
            + 1461L * (y + 4800L + (m - 14) / 12) / 4 + 367 * (m - 2 - (m - 14) / 12 * 12) / 12 - 3 * ((y + 4900L + (m - 14) / 12) / 100) / 4 + 1;

    return jdn;
}

/**
 */
static int daydiff(int y1, int m1, int d1, int y2, int m2, int d2)
{
    int jday1, jday2;

    jday1 = ymd_to_jdnl(y1, m1, d1, 0);
    jday2 = ymd_to_jdnl(y2, m2, d2, 0);

    return jday1 - jday2;
}

/** Calculates transform from `tunits' to "days from BASEDAY/BASEMONTH/BASYEAR".
 * This is a very basic procedure, do not expect it to work smoothly if things
 * get complicated. The conversion equation is
 *
 * <new values> = <old values> * tunits_multiple + tunits_offset
 *
 * @param tunits - input, e.g. "seconds since 1981-01-01 00:00:00"
 * @param tunits_multiple - output, multiple
 * @param tunits_offset - output, offset
 */
void tunits_convert(char* tunits, double* tunits_multiple, double* tunits_offset)
{
    char* startdate;
    int year, month, day, h, m, s;
    char* token;
    char seps_date[] = " -\n";
    char seps_time[] = " :\n";

    if (strncasecmp(tunits, "fraction of a ", 14) == 0)
        tunits += 14;

    if (strncasecmp(tunits, "sec", 3) == 0)
        *tunits_multiple = 1.0 / 86400;
    else if (strncasecmp(tunits, "day", 3) == 0)
        *tunits_multiple = 1.0;
    else
        enkf_quit("can not interpret time units \"%s\"", tunits);

    startdate = strstr(tunits, "since");
    if (startdate == NULL)
        enkf_quit("can not interpret time units \"%s\"", tunits);
    startdate += strlen("since ");
    if ((token = strtok(startdate, seps_date)) == NULL)
        enkf_quit("can not interpret time units \"%s\"", tunits);
    if (!str2int(token, &year))
        enkf_quit("could not convert \"%s\" to time units", tunits);
    if ((token = strtok(NULL, seps_date)) == NULL)
        enkf_quit("can not interpret time units \"%s\"", tunits);
    if (!str2int(token, &month))
        enkf_quit("could not convert \"%s\" to time units", tunits);
    if ((token = strtok(NULL, seps_date)) == NULL)
        enkf_quit("can not interpret time units \"%s\"", tunits);
    if (!str2int(token, &day))
        enkf_quit("could not convert \"%s\" to time units", tunits);
    h = 0;
    m = 0;
    s = 0;
    if ((token = strtok(NULL, seps_time)) != NULL)
        if (!str2int(token, &h))
            enkf_quit("could not convert \"%s\" to time units", tunits);
    if ((token = strtok(NULL, seps_time)) != NULL)
        if (!str2int(token, &m))
            enkf_quit("could not convert \"%s\" to time units", tunits);
    if ((token = strtok(NULL, seps_time)) != NULL)
        if (!str2int(token, &s))
            enkf_quit("could not convert \"%s\" to time units", tunits);

    *tunits_offset = (double) daydiff(year, month, day, BASEYEAR, BASEMONTH, BASEDAY) - (double) h / 24.0 - (double) m / 1440.0 - (double) s / 86400.0;
}

/**
 */
int file_exists(char* fname)
{
    FILE* f;

    f = fopen(fname, "r");
    if (f == NULL)
        return 0;
    fclose(f);
    return 1;
}

/**
 */
void file_delete(char* fname)
{
    int status = -1;

    status = unlink(fname);
    if (status != 0) {
        int errno_saved = errno;

        enkf_quit("could not delete file \"%s\": %s", fname, strerror(errno_saved));
    }
}

/**
 */
int str2double(char* token, double* value)
{
    char* end = NULL;

    if (token == NULL) {
        *value = NaN;
        return 0;
    }

    *value = strtod(token, &end);

    if (end == token) {
        *value = NaN;
        return 0;
    }

    return 1;
}

/**
 */
int str2int(char* token, int* value)
{
    long int tmp;
    char* end = NULL;

    if (token == NULL) {
        *value = NaN;
        return 0;
    }

    tmp = strtol(token, &end, 10);

    if (end == token || tmp > MAXINT || tmp < MININT) {
        *value = MAXINT;
        return 0;
    }

    *value = (int) tmp;
    return 1;
}

/**
 */
int read_bool(char* token)
{
    int v;

    if (token[0] == 'y' || token[0] == 'Y')
        return 1;
    if (token[0] == 'n' || token[0] == 'N')
        return 0;
    if (!str2int(token, &v))
        return -1;
    if (v == 0 || v == 1)
        return v;
    return -1;
}

/** Allocates nj x ni matrix of something. It will be accessed as [i][j].
 * @param ni Outer dimension
 * @param nj Inner dimension
 * @param unitsize Size of one matrix element in bytes
 * @return Matrix
 */
void* alloc2d(int ni, int nj, size_t unitsize)
{
    size_t size;
    char* p;
    char** pp;
    int i;

    if (ni <= 0 || nj <= 0)
        enkf_quit("alloc2d(): invalid size (ni = %d, nj = %d)", ni, nj);

    size = ni * nj;
    if ((p = calloc(size, unitsize)) == NULL) {
        int errno_saved = errno;

        enkf_quit("alloc2d(): %s", strerror(errno_saved));
    }
    size = ni * sizeof(void*);
    if ((pp = malloc(size)) == NULL) {
        int errno_saved = errno;

        enkf_quit("alloc2d(): %s", strerror(errno_saved));
    }
    for (i = 0; i < ni; i++)
        pp[i] = &p[i * nj * unitsize];

    return pp;
}

/** Destroys a matrix.
 * @param pp Matrix
 */
void free2d(void* pp)
{
    void* p;

    p = ((void**) pp)[0];
    free(pp);
    free(p);
}

/** Allocates nk x nj x ni array of something. It will be accessed as [i][j][k].
 * @param ni Outer dimension
 * @param nj Middle dimension
 * @param nj Inner dimension
 * @param unitsize Size of one matrix element in bytes
 * @return Matrix
 */
void* alloc3d(int ni, int nj, int nk, size_t unitsize)
{
    size_t size;
    char* p;
    char** pp;
    char*** ppp;
    int i;

    if (ni <= 0 || nj <= 0 || nk <= 0)
        enkf_quit("alloc3d(): invalid size (ni = %d, nj = %d, nk = %d)", ni, nj, nk);

    size = ni * nj * nk;
    if ((p = calloc(size, unitsize)) == NULL) {
        int errno_saved = errno;

        enkf_quit("alloc3d(): %s", strerror(errno_saved));
    }

    size = ni * nj * sizeof(void*);
    if ((pp = malloc(size)) == NULL) {
        int errno_saved = errno;

        enkf_quit("alloc3d(): %s", strerror(errno_saved));
    }
    for (i = 0; i < ni * nj; i++)
        pp[i] = &p[i * nk * unitsize];

    size = ni * sizeof(void*);
    if ((ppp = malloc(size)) == NULL) {
        int errno_saved = errno;

        enkf_quit("alloc3d(): %s", strerror(errno_saved));
    }
    for (i = 0; i < ni; i++)
        ppp[i] = &pp[i * nj];

    return ppp;
}

/** Destroys a 3D array.
 * @param ppp 3D array
 */
void free3d(void* ppp)
{
    void** pp;
    void* p;

    p = ((void***) ppp)[0][0];
    pp = ((void***) ppp)[0];
    free(ppp);
    assert(pp != NULL);
    free(pp);
    assert(p != NULL);
    free(p);
}

/**
 */
int nc_isunlimdimid(int ncid, int dimid)
{
    int unlimdimid, status;

    status = nc_inq_unlimdim(ncid, &unlimdimid);
    if (status != NC_NOERR)
        return 0;
    if (dimid == unlimdimid)
        return 1;

    return 0;
}

/**
 */
int getnlevels(char fname[], char varname[])
{
    int ncid;
    int varid;
    int ndims;
    int dimids[4];
    size_t dimlen[4];
    int i;
    int containsrecorddim;

    ncw_open(fname, NC_NOWRITE, &ncid);
    ncw_inq_varid(fname, ncid, varname, &varid);
    ncw_inq_varndims(fname, ncid, varid, &ndims);
    ncw_inq_vardimid(fname, ncid, varid, dimids);
    for (i = 0; i < ndims; ++i)
        ncw_inq_dimlen(fname, ncid, dimids[i], &dimlen[i]);
    containsrecorddim = nc_isunlimdimid(ncid, dimids[0]);
    ncw_close(fname, ncid);

    if (ndims == 4) {
        assert(containsrecorddim);
        return (int) dimlen[1];
    }
    if (ndims == 3)
        return (containsrecorddim) ? 1 : (int) dimlen[0];
    if (ndims == 2)
        return (containsrecorddim) ? 0 : 1;

    return 0;
}

/** Reads one horizontal field (layer) for a variable from a NetCDF file.
 */
void readfield(char fname[], int k, char varname[], float* v)
{
    int ncid;
    int varid;
    int ndims;
    int dimids[4];
    size_t dimlen[4];
    size_t start[4], count[4];
    int i, n;
    int containsrecorddim;

    ncw_open(fname, NC_NOWRITE, &ncid);
    ncw_inq_varid(fname, ncid, varname, &varid);
    ncw_inq_varndims(fname, ncid, varid, &ndims);
    ncw_inq_vardimid(fname, ncid, varid, dimids);
    for (i = 0; i < ndims; ++i)
        ncw_inq_dimlen(fname, ncid, dimids[i], &dimlen[i]);

    containsrecorddim = nc_isunlimdimid(ncid, dimids[0]);

    if (ndims == 4) {
        assert(containsrecorddim);
        start[0] = 0;
        start[1] = k;
        start[2] = 0;
        start[3] = 0;
        count[0] = 1;
        count[1] = 1;
        count[2] = dimlen[2];
        count[3] = dimlen[3];
    } else if (ndims == 3) {
        if (!containsrecorddim) {
            start[0] = k;
            start[1] = 0;
            start[2] = 0;
            count[0] = 1;
            count[1] = dimlen[1];
            count[2] = dimlen[2];
        } else {
            assert(k <= 0);
            start[0] = 0;
            start[1] = 0;
            start[2] = 0;
            count[0] = 1;
            count[1] = dimlen[1];
            count[2] = dimlen[2];
        }
    } else if (ndims == 2) {
        if (containsrecorddim)
            enkf_quit("%s: can not read a layer from a 1D variable \"%s\"", fname, varname);
        if (k > 0)
            enkf_quit("%s: can not read layer %d from a 2D variable \"%s\"", fname, k, varname);
        start[0] = 0;
        start[1] = 0;
        count[0] = dimlen[0];
        count[1] = dimlen[1];
    } else
        enkf_quit("%s: can not read 2D field for \"%s\": # of dimensions = %d", fname, varname, ndims);

    ncw_get_vara_float(fname, ncid, varid, start, count, v);

    n = 1;
    for (i = 0; i < ndims; ++i)
        n *= count[i];

    if (ncw_att_exists(ncid, varid, "scale_factor")) {
        float scale_factor;

        ncw_get_att_float(fname, ncid, varid, "scale_factor", &scale_factor);
        for (i = 0; i < n; ++i)
            v[i] *= scale_factor;
    }

    if (ncw_att_exists(ncid, varid, "add_offset")) {
        float add_offset;

        ncw_get_att_float(fname, ncid, varid, "add_offset", &add_offset);

        for (i = 0; i < n; ++i)
            v[i] += add_offset;
    }

    ncw_close(fname, ncid);
}

/** Writes one horizontal field (layer) for a variable to a NetCDF file.
 */
void writefield(char fname[], int k, char varname[], float* v)
{
    int ncid;
    int varid;
    int ndims;
    int dimids[4];
    size_t dimlen[4];
    size_t start[4], count[4];
    int i, n;
    int containsrecorddim;

    ncw_open(fname, NC_WRITE, &ncid);
    ncw_inq_varid(fname, ncid, varname, &varid);
    ncw_inq_varndims(fname, ncid, varid, &ndims);
    ncw_inq_vardimid(fname, ncid, varid, dimids);
    for (i = 0; i < ndims; ++i)
        ncw_inq_dimlen(fname, ncid, dimids[i], &dimlen[i]);

    containsrecorddim = nc_isunlimdimid(ncid, dimids[0]);

    if (ndims == 4) {
        assert(containsrecorddim);
        start[0] = 0;
        start[1] = k;
        start[2] = 0;
        start[3] = 0;
        count[0] = 1;
        count[1] = 1;
        count[2] = dimlen[2];
        count[3] = dimlen[3];
    } else if (ndims == 3) {
        if (!containsrecorddim) {
            start[0] = k;
            start[1] = 0;
            start[2] = 0;
            count[0] = 1;
            count[1] = dimlen[1];
            count[2] = dimlen[2];
        } else {
            assert(k <= 0);
            start[0] = 0;
            start[1] = 0;
            start[2] = 0;
            count[0] = 1;
            count[1] = dimlen[1];
            count[2] = dimlen[2];
        }
    } else if (ndims == 2) {
        if (containsrecorddim)
            enkf_quit("%s: can not write a layer from a 1D variable \"%s\"", fname, varname);
        if (k > 0)
            enkf_quit("%s: can not write layer %d from a 2D variable \"%s\"", fname, k, varname);
        start[0] = 0;
        start[1] = 0;
        count[0] = dimlen[0];
        count[1] = dimlen[1];
    } else
        enkf_quit("%s: can not write 2D field for \"%s\": # of dimensions = %d", fname, varname, ndims);

    n = 1;
    for (i = 0; i < ndims; ++i)
        n *= count[i];

    if (ncw_att_exists(ncid, varid, "add_offset")) {
        float add_offset;

        ncw_get_att_float(fname, ncid, varid, "add_offset", &add_offset);

        for (i = 0; i < n; ++i)
            v[i] -= add_offset;
    }

    if (ncw_att_exists(ncid, varid, "scale_factor")) {
        float scale_factor;

        ncw_get_att_float(fname, ncid, varid, "scale_factor", &scale_factor);
        for (i = 0; i < n; ++i)
            v[i] /= scale_factor;
    }

    if (ncw_att_exists(ncid, varid, "valid_range")) {
        nc_type xtype;
        size_t len;

        ncw_inq_att(fname, ncid, varid, "valid_range", &xtype, &len);
        if (xtype == NC_SHORT) {
            int valid_range[2];
            int missing_value = MINSHORT;

            if (ncw_att_exists(ncid, varid, "missing_value"))
                ncw_get_att_int(fname, ncid, varid, "missing_value", &missing_value);

            ncw_get_att_int(fname, ncid, varid, "valid_range", valid_range);
            for (i = 0; i < n; ++i) {
                if (v[i] == missing_value)
                    continue;
                if ((int) v[i] < valid_range[0])
                    v[i] = (float) valid_range[0];
                else if ((int) v[i] > valid_range[1])
                    v[i] = (float) valid_range[1];
            }
        } else if (xtype == NC_FLOAT) {
            float valid_range[2];
            float missing_value = -MAXFLOAT;

            if (ncw_att_exists(ncid, varid, "missing_value"))
                ncw_get_att_float(fname, ncid, varid, "missing_value", &missing_value);

            ncw_get_att_float(fname, ncid, varid, "valid_range", valid_range);
            for (i = 0; i < n; ++i) {
                if (v[i] == missing_value)
                    continue;
                if (v[i] < valid_range[0])
                    v[i] = valid_range[0];
                else if (v[i] > valid_range[1])
                    v[i] = valid_range[1];
            }
        } else
            enkf_quit("%s: %s: output for types other than NC_SHORT and NC_FLOAT are not handled yet", fname, varname);
    }

    ncw_put_vara_float(fname, ncid, varid, start, count, v);
    ncw_close(fname, ncid);
}

/**
 */
void read3dfield(char* fname, char* varname, float* v)
{
    int ncid;
    int varid;
    int ndims;
    int dimids[4];
    size_t dimlen[4];
    size_t start[4], count[4];
    int i, n;
    int containsrecorddim;

    ncw_open(fname, NC_NOWRITE, &ncid);
    ncw_inq_varid(fname, ncid, varname, &varid);
    ncw_inq_varndims(fname, ncid, varid, &ndims);
    ncw_inq_vardimid(fname, ncid, varid, dimids);
    for (i = 0; i < ndims; ++i)
        ncw_inq_dimlen(fname, ncid, dimids[i], &dimlen[i]);

    containsrecorddim = nc_isunlimdimid(ncid, dimids[0]);

    if (ndims == 4) {
        assert(containsrecorddim);
        start[0] = 0;
        start[1] = 0;
        start[2] = 0;
        start[3] = 0;
        count[0] = 1;
        count[1] = dimlen[1];
        count[2] = dimlen[2];
        count[3] = dimlen[3];
    } else if (ndims == 3) {
        assert(!containsrecorddim);
        start[0] = 0;
        start[1] = 0;
        start[2] = 0;
        count[0] = dimlen[0];
        count[1] = dimlen[1];
        count[2] = dimlen[2];
    } else
        enkf_quit("%s: can not read 3D field for \"%s\": # of dimensions = %d", fname, varname, ndims);

    ncw_get_vara_float(fname, ncid, varid, start, count, v);

    n = 1;
    for (i = 0; i < ndims; ++i)
        n *= count[i];

    if (ncw_att_exists(ncid, varid, "scale_factor")) {
        float scale_factor;

        ncw_get_att_float(fname, ncid, varid, "scale_factor", &scale_factor);
        for (i = 0; i < n; ++i)
            v[i] *= scale_factor;
    }

    if (ncw_att_exists(ncid, varid, "add_offset")) {
        float add_offset;

        ncw_get_att_float(fname, ncid, varid, "add_offset", &add_offset);

        for (i = 0; i < n; ++i)
            v[i] += add_offset;
    }

    ncw_close(fname, ncid);
}

/**
 */
double date_str2dbl(char strdate[])
{
    char buf[MAXSTRLEN];
    char* token;
    char seps[] = " ";
    char seps2[] = "\n";
    double date, offset, multiple;

    strncpy(buf, strdate, MAXSTRLEN);
    if ((token = strtok(buf, seps)) == NULL)
        enkf_quit("could not understand date \"%s\"", strdate);
    if (!str2double(token, &date))
        enkf_quit("could not convert date \"%s\" to double", token);
    if ((token = strtok(NULL, seps2)) == NULL)
        enkf_quit("could not understand date \"%s\"", strdate);
    tunits_convert(token, &multiple, &offset);
    date = date * multiple + offset;

    return date;
}

/**
 */
int get_tshift(double date, double tstep)
{
    return (int) floor(date / tstep + 0.5);
}

/** For debugging purposes - to be called from GDB.
 */
void print_matrix_double(int n, int m, double** A, char offset[])
{
    int i, j;

    for (i = 0; i < n; ++i) {
        printf("%s", offset);
        for (j = 0; j < m; ++j)
            printf("%10.5g ", fabs(A[j][i]) < EPS_DOUBLE ? 0.0 : A[j][i]);
        printf("\n");
    }
}

/** For debugging purposes - to be called from GDB.
 */
void print_matrix_float(int n, int m, float** A, char offset[])
{
    int i, j;

    for (i = 0; i < n; ++i) {
        printf("%s", offset);
        for (j = 0; j < m; ++j)
            printf("%10.5g ", fabsf(A[j][i]) < EPS_FLOAT ? 0.0 : (double) A[j][i]);
        printf("\n");
    }
}

/** For debugging purposes - to be called from GDB.
 */
void print_vector_double(int n, double* a, char offset[])
{
    int i;

    printf("%s", offset);
    for (i = 0; i < n; ++i)
        printf("%10.5g ", fabs(a[i]) < EPS_DOUBLE ? 0.0 : a[i]);
    printf("\n");
}

/** For debugging purposes - to be called from GDB.
 */
void print_vector_float(int n, float* a, char offset[])
{
    int i;

    printf("%s", offset);
    for (i = 0; i < n; ++i)
        printf("%10.5g ", fabsf(a[i]) < EPS_FLOAT ? 0.0 : (double) a[i]);
    printf("\n");
}

/**
 */
ENSOBSTYPE interpolate2d(double fi, double fj, int ni, int nj, float** v, int** mask)
{
    int i1 = (int) floor(fi);
    double wi1 = ceil(fi) - fi;
    int i2 = (int) ceil(fi);
    double wi2 = fi - floor(fi);
    int j1 = (int) floor(fj);
    double wj1 = ceil(fj) - fj;
    int j2 = (int) ceil(fj);
    double wj2 = fj - floor(fj);
    double sum = 0.0;
    double w = 0.0;
    double ww;

    if (i1 == i2)
        wi1 = 1.0;
    if (j1 == j2)
        wj1 = 1.0;

    if (i1 == -1)
        i1 = ni - 1;
    if (i2 == ni)
        i2 = 0;

    assert(i1 >= 0 && i2 < ni && j1 >= 0 && j2 < nj);

    if (mask[j1][i1]) {
        ww = wj1 * wi1;
        sum += v[j1][i1] * ww;
        w += ww;
    }
    if (mask[j1][i2]) {
        ww = wj1 * wi2;
        sum += v[j1][i2] * ww;
        w += ww;
    }
    if (mask[j2][i1]) {
        ww = wj2 * wi1;
        sum += v[j2][i1] * ww;
        w += ww;
    }
    if (mask[j2][i2]) {
        ww = wj2 * wi2;
        sum += v[j2][i2] * ww;
        w += ww;
    }
    sum = sum / w;

    return (ENSOBSTYPE) sum;
}

/** Linearly interpolates a 3D field to fractional coordinates in index space.
 *  Assumes that integer k indices correspond to layer centres. E.g. for 
 *  fk = 1.2 the vertical weights are 0.8 of layer 1 and 0.2 of layer 2.
 */
ENSOBSTYPE interpolate3d(double fi, double fj, double fk, int ni, int nj, int nk, float*** v, int** nlevels)
{
    int i1 = (int) floor(fi);
    double wi1 = ceil(fi) - fi;
    int i2 = (int) ceil(fi);
    double wi2 = fi - floor(fi);
    int j1 = (int) floor(fj);
    double wj1 = ceil(fj) - fj;
    int j2 = (int) ceil(fj);
    double wj2 = fj - floor(fj);
    int k1, k2;
    double wk1, wk2;
    double sum = 0.0;
    double w = 0.0;
    double ww;

    if (fk < 0.0)
        fk = 0.0;
    if (fk > (double) (nk - 1))
        fk = (double) (nk - 1);
    k1 = (int) floor(fk);
    wk1 = ceil(fk) - fk;
    k2 = (int) ceil(fk);
    wk2 = fk - floor(fk);

    if (i1 == i2)
        wi1 = 1.0;
    if (j1 == j2)
        wj1 = 1.0;
    if (k1 == k2)
        wk1 = 1.0;

    if (i1 == -1)
        i1 = ni - 1;
    if (i2 == ni)
        i2 = 0;

    assert(i1 >= 0 && i2 <= ni - 1 && j1 >= 0 && j2 <= nj - 1 && k1 >= 0 && k2 < nk);

    if (nlevels[j1][i1] > k1) {
        ww = wj1 * wi1 * wk1;
        sum += v[k1][j1][i1] * ww;
        w += ww;
    }
    if (nlevels[j1][i2] > k1) {
        ww = wj1 * wi2 * wk1;
        sum += v[k1][j1][i2] * ww;
        w += ww;
    }
    if (nlevels[j2][i1] > k1) {
        ww = wj2 * wi1 * wk1;
        sum += v[k1][j2][i1] * ww;
        w += ww;
    }
    if (nlevels[j2][i2] > k1) {
        ww = wj2 * wi2 * wk1;
        sum += v[k1][j2][i2] * ww;
        w += ww;
    }
    if (nlevels[j1][i1] > k2) {
        ww = wj1 * wi1 * wk2;
        sum += v[k2][j1][i1] * ww;
        w += ww;
    }
    if (nlevels[j1][i2] > k2) {
        ww = wj1 * wi2 * wk2;
        sum += v[k2][j1][i2] * ww;
        w += ww;
    }
    if (nlevels[j2][i1] > k2) {
        ww = wj2 * wi1 * wk2;
        sum += v[k2][j2][i1] * ww;
        w += ww;
    }
    if (nlevels[j2][i2] > k2) {
        ww = wj2 * wi2 * wk2;
        sum += v[k2][j2][i2] * ww;
        w += ww;
    }
    sum = sum / w;

    return (ENSOBSTYPE) sum;
}

/** Gaspary & Cohn's taper function.
 * @param x Support radius
 * @return Taper coefficient
 */
double taper_gc(double x)
{
    double x2, x3;

    assert(x >= 0 && x <= 1.0 + 1.0e-8);

    if (x >= 1.0)               /* handle possible round-up error */
        return 0.0;

    x *= 2.0;
    x2 = x * x;
    x3 = x2 * x;
    if (x < 1.0)
        return 1.0 + x2 * (-x3 / 4.0 + x2 / 2.0) + x3 * (5.0 / 8.0) - x2 * (5.0 / 3.0);
    return x2 * (x3 / 12.0 - x2 / 2.0) + x3 * (5.0 / 8.0) + x2 * (5.0 / 3.0) - x * 5.0 + 4.0 - (2.0 / 3.0) / x;
}

/** Converts from geographic to cartesian coordinates.
 * @param in Input: {lon, lat}
 * @param out Output: {x, y, z}
 */
void ll2xyz(double in[2], double out[3])
{
    double lon = in[0] * DEG2RAD;
    double lat = in[1] * DEG2RAD;
    double coslat = cos(lat);

    out[0] = REARTH * sin(lon) * coslat;
    out[1] = REARTH * cos(lon) * coslat;
    out[2] = REARTH * sin(lat);
}
