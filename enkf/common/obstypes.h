/******************************************************************************
 *
 * File:        obstypes.h       
 *
 * Created:     18/08/2014
 *
 * Author:      Pavel Sakov
 *              Bureau of Meteorology
 *
 * Description:
 *
 * Revisions:
 *
 *****************************************************************************/

#if !defined(_OBSTYPES_H)

typedef struct {
    int id;
    char* name;
    char* varname;
    char* varname2;
    char* offset_fname;
    char* offset_varname;
    char* hfunction;
    int issurface;
    double allowed_min;
    double allowed_max;
    int isasync;
    double async_tstep;
    double locrad;
    double rfactor;

    int vid;
    int gridid;

    double xmin;
    double xmax;
    double ymin;
    double ymax;
    double zmin;
    double zmax;

    int nobs;
    int ngood;
    int noutside;
    int nland;
    int nshallow;
    int nbadbatch;
    int nroundup;
    int nrange;
    int nmodified;

    double date_min;
    double date_max;
} obstype;

void obstypes_read(char fname[], int* n, obstype** types, double locrad_base, double rfactor_base);
void obstypes_destroy(int n, obstype* types);
int obstype_getid(int n, obstype types[], char* name);
void obstypes_describeprm(void);

#define _OBSTYPES_H
#endif
