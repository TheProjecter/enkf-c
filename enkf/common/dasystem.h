/******************************************************************************
 *
 * File:        dasystem.h        
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

#if !defined(_DASYSTEM_H)

#include "ncw.h"
#include "observations.h"
#include "model.h"

#define S_MODE_NONE 0
#define S_MODE_HE_f 1
#define S_MODE_HA_f 2
#define S_MODE_S_f  3
#define S_MODE_HE_a 4
#define S_MODE_HA_a 5
#define S_MODE_S_a  6

struct field;
typedef struct field field;

struct field {
    int id;
    int varid;
    char varname[NC_MAX_NAME];
    int level;
};

struct dasystem;
typedef struct dasystem dasystem;

struct dasystem {
    char* prmfname;
    int mode;
    int scheme;
    int target;
    char* ensdir;
    char* bgdir;

    int nmem;

    model* m;

    observations* obs;
    /*
     * Currently there are two observation sorting modes used by the code: by ID
     * and by IJ. Correspondingly, the `sort_mode' flag is set to either
     * OBS_SORTMODE_ID or OBS_SORTMODE_IJ. Note that at any time the
     * correspondence between array of observations obs->data and arrays of
     * ensemble observations/innovations below is assumed and should be
     * maintained: obs->data[i] corresponds to S[:][i], s_f[i], std_f[i],
     * s_a[i] and std_a[i].
     */
    int sort_mode;

    /*
     * The ensemble observations `S' and innovations `s' can be either in
     * standardised or not standardised mode. Further, `S' is too big to be
     * copied, instead it is modified when transferred between states. The
     * states of `s' and 'S' are described by `s_mode':
     *
     *   s_mode      s_f, s_a and S      S contains    S relates to
     *               are standardised    anomalies     forecast
     * S_MODE_HE_f         no                no           yes
     * S_MODE_HA_f         no                yes          yes
     * S_MODE_S_f          yes               yes          yes
     * S_MODE_HE_a         no                no           no
     * S_MODE_HA_a         no                yes          no
     * S_MODE_S_a          yes               yes          no
     */
    ENSOBSTYPE** S;             /* HE or HA or S [mem][obs] */
    double* s_f;                /* innovation */
    double* std_f;              /* ensemble spread */
    double* s_a;                /* innovation */
    double* std_a;              /* ensemble spread */
    int s_mode;

    double kfactor;
    double locrad;
    int stride;

    int nfields;
    field* fields;

    int fieldbufsize;

    int nregions;
    region* regions;

    int nplogs;
    pointlog* plogs;

    int nbadbatchspecs;
    badbatchspec* badbatchspecs;
};

dasystem* das_create(enkfprm* prm);
void das_destroy(dasystem* das);

void das_getHE(dasystem* das);
void das_addanalysis(dasystem* das, char fname[]);
void das_addforecast(dasystem* das, char fname[]);
void das_addmodifiederrors(dasystem* das, char fname[]);
void das_calcinnandspread(dasystem* das);
void das_calctransforms(dasystem* das);
void das_dopointlogs(dasystem* das);
void das_getfields(dasystem* das);
void das_getnmem(dasystem* das);
void das_interpolate2d(dasystem* das, char fname[], float** v, int nobs, int obsids[], double out[]);
void das_interpolate3d(dasystem* das, char fname[], float*** v, int nobs, int obsids[], double out[]);
void das_moderateobs(dasystem* das);
void das_calcbatchstats(dasystem* das, int doprint);
void das_printobsstats(dasystem* das);
void das_printfobsstats(dasystem* das);
void das_readobs(dasystem* das, char fname[]);
void das_setobstypes(dasystem* das);
void das_standardise(dasystem* das);
void das_destandardise(dasystem* das);
void das_update(dasystem* das, int calcspread, int leavetiles);
void das_updateHE(dasystem* das);

#define _DASYSTEM_H
#endif
