/******************************************************************************
 *
 * File:        ensobs.c        
 *
 * Created:     11/2013
 *
 * Author:      Pavel Sakov
 *              Bureau of Meteorology
 *
 * Description: The code in this file structurally belongs to dasystem.c,
 *              and is put in a separate file just to break dasystem.c in
 *              smaller parts.
 *
 * Revisions:
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <values.h>
#include <math.h>
#include "nan.h"
#include "distribute.h"
#include "lapack.h"
#include "definitions.h"
#include "utils.h"
#include "grid.h"
#include "model2obs.h"
#include "allhs.h"
#include "dasystem.h"

#define EPSF 1.0e-6f

/** Reads observations from "observations.nc".
 */
void das_readobs(dasystem* das, char fname[])
{
    observations* obs = das->obs;
    int ncid;
    int dimid_nobs[1];
    size_t nobs;
    int varid_type, varid_product, varid_instrument, varid_id, varid_idorig, varid_value, varid_std, varid_lon, varid_lat, varid_depth, varid_fi, varid_fj, varid_fk, varid_date, varid_status, varid_aux;
    int* type;
    int* product;
    int* instrument;
    int* id;
    int* id_orig;
    double* value;
    double* std;
    double* lon;
    double* lat;
    double* depth;
    double* fi;
    double* fj;
    double* fk;
    double* date;
    int* status;
    int* aux;
    int nobstypes, nproducts, ninstruments;
    int i;

    ncw_open(fname, NC_NOWRITE, &ncid);
    ncw_inq_dimid(fname, ncid, "nobs", dimid_nobs);
    ncw_inq_dimlen(fname, ncid, dimid_nobs[0], &nobs);

    obs->nobs = nobs;
    obs->data = malloc(nobs * sizeof(measurement));
    enkf_printf("    %u observations\n", (unsigned int) nobs);

    ncw_inq_varid(fname, ncid, "type", &varid_type);
    ncw_inq_varid(fname, ncid, "product", &varid_product);
    ncw_inq_varid(fname, ncid, "instrument", &varid_instrument);
    ncw_inq_varid(fname, ncid, "id", &varid_id);
    ncw_inq_varid(fname, ncid, "id_orig", &varid_idorig);
    ncw_inq_varid(fname, ncid, "value", &varid_value);
    ncw_inq_varid(fname, ncid, "std", &varid_std);
    ncw_inq_varid(fname, ncid, "lon", &varid_lon);
    ncw_inq_varid(fname, ncid, "lat", &varid_lat);
    ncw_inq_varid(fname, ncid, "depth", &varid_depth);
    ncw_inq_varid(fname, ncid, "fi", &varid_fi);
    ncw_inq_varid(fname, ncid, "fj", &varid_fj);
    ncw_inq_varid(fname, ncid, "fk", &varid_fk);
    ncw_inq_varid(fname, ncid, "date", &varid_date);
    ncw_inq_varid(fname, ncid, "status", &varid_status);
    ncw_inq_varid(fname, ncid, "aux", &varid_aux);

    type = malloc(nobs * sizeof(int));
    product = malloc(nobs * sizeof(int));
    instrument = malloc(nobs * sizeof(int));
    id = malloc(nobs * sizeof(int));
    id_orig = malloc(nobs * sizeof(int));
    value = malloc(nobs * sizeof(double));
    std = malloc(nobs * sizeof(double));
    lon = malloc(nobs * sizeof(double));
    lat = malloc(nobs * sizeof(double));
    depth = malloc(nobs * sizeof(double));
    fi = malloc(nobs * sizeof(double));
    fj = malloc(nobs * sizeof(double));
    fk = malloc(nobs * sizeof(double));
    date = malloc(nobs * sizeof(double));
    status = malloc(nobs * sizeof(int));
    aux = malloc(nobs * sizeof(int));

    /*
     * type 
     */
    ncw_inq_varnatts(fname, ncid, varid_type, &nobstypes);
    for (i = 0; i < nobstypes; ++i) {
        char attname[NC_MAX_NAME];

        ncw_inq_attname(fname, ncid, varid_type, i, attname);
        assert(strcmp(attname, st_findstringbyindex(obs->types, i)) == 0);
    }

    /*
     * product 
     */
    ncw_inq_varnatts(fname, ncid, varid_product, &nproducts);
    for (i = 0; i < nproducts; ++i) {
        char attname[NC_MAX_NAME];

        ncw_inq_attname(fname, ncid, varid_product, i, attname);
        st_add(obs->products, attname, i);
    }

    /*
     * instrument 
     */
    ncw_inq_varnatts(fname, ncid, varid_instrument, &ninstruments);
    for (i = 0; i < ninstruments; ++i) {
        char attname[NC_MAX_NAME];

        ncw_inq_attname(fname, ncid, varid_instrument, i, attname);
        st_add(obs->instruments, attname, i);
    }

    ncw_get_var_int(fname, ncid, varid_type, type);
    ncw_get_var_int(fname, ncid, varid_product, product);
    ncw_get_var_int(fname, ncid, varid_instrument, instrument);
    ncw_get_var_int(fname, ncid, varid_id, id);
    ncw_get_var_int(fname, ncid, varid_idorig, id_orig);
    ncw_get_var_double(fname, ncid, varid_value, value);
    ncw_get_var_double(fname, ncid, varid_std, std);
    ncw_get_var_double(fname, ncid, varid_lon, lon);
    ncw_get_var_double(fname, ncid, varid_lat, lat);
    ncw_get_var_double(fname, ncid, varid_depth, depth);
    ncw_get_var_double(fname, ncid, varid_fi, fi);
    ncw_get_var_double(fname, ncid, varid_fj, fj);
    ncw_get_var_double(fname, ncid, varid_fk, fk);
    ncw_get_var_double(fname, ncid, varid_date, date);
    ncw_get_var_int(fname, ncid, varid_status, status);
    ncw_get_var_int(fname, ncid, varid_aux, aux);

    ncw_close(fname, ncid);

    for (i = 0; i < (int) nobs; ++i) {
        measurement* m = &obs->data[i];

        m->type = type[i];
        m->product = product[i];
        m->instrument = instrument[i];
        m->id = id[i];
        m->id_orig = id_orig[i];
        m->value = value[i];
        m->std = std[i];
        m->lon = lon[i];
        m->lat = lat[i];
        m->depth = depth[i];
        m->fi = fi[i];
        m->fj = fj[i];
        m->fk = fk[i];
        m->date = date[i];
        m->status = status[i];
        m->aux = aux[i];
    }

    free(type);
    free(product);
    free(instrument);
    free(id);
    free(id_orig);
    free(value);
    free(std);
    free(lon);
    free(lat);
    free(depth);
    free(fi);
    free(fj);
    free(fk);
    free(date);
    free(status);
    free(aux);

    obs_calcstats(obs);
}

/**
 */
void das_getHE(dasystem* das, int fstatsonly)
{
    observations* obs = das->obs;
    model* m = das->m;
    int ni, nj, nk;
    int i, e;

    model_getdims(m, &ni, &nj, &nk);

    if (das->nmem <= 0)
        das_getnmem(das);
    enkf_printf("    ensemble size = %d\n", das->nmem);
    assert(das->nmem > 0);

    distribute_iterations(0, das->nmem - 1, nprocesses, rank);

    /*
     * ensemble observation array to be filled 
     */
    assert(das->S == NULL);
    das->S = alloc2d(das->nmem, obs->nobs, sizeof(ENSOBSTYPE));
    if (das->mode == MODE_ENOI)
        das->Hx = calloc(obs->nobs, sizeof(ENSOBSTYPE));

    for (i = 0; i < obs->nobstypes; ++i) {
        obstype* ot = &obs->obstypes[i];
        float*** vvv = NULL;
        float** vv = NULL;
        H_fn H = NULL;
        int nobs;
        int* obsids;
        char fname[MAXSTRLEN];

        enkf_printf("    %s ", ot->name);
        fflush(stdout);

        if (ot->issurface)
            vv = alloc2d(nj, ni, sizeof(float));
        else
            vvv = alloc3d(nk, nj, ni, sizeof(float));

        /*
         * set H 
         */
        H = getH(ot->name, ot->hfunction);

        if (ot->isasync) {
            int t1 = get_tshift(ot->date_min, ot->async_tstep);
            int t2 = get_tshift(ot->date_max, ot->async_tstep);
            int t;

            for (t = t1; t <= t2; ++t) {
                enkf_printf("|");
                obs_find_bytypeandtime(obs, i, t, &nobs, &obsids, fstatsonly);
                if (nobs == 0)
                    continue;

                if (das->mode == MODE_ENKF || !fstatsonly) {
                    for (e = my_first_iteration; e <= my_last_iteration; ++e) {
                        int success = model_getmemberfname_async(m, das->ensdir, ot->varname, ot->name, e + 1, t, fname);

                        H(das, nobs, obsids, fname, e + 1, t, ot->varname, (ot->issurface) ? (void*) vv : (void*) vvv, das->S[e]);
                        enkf_printf((success) ? "a" : "s");
                        fflush(stdout);
                    }
                }

                if (das->mode == MODE_ENOI) {
                    if (enkf_obstype == OBSTYPE_VALUE) {
                        int success = model_getbgfname_async(m, das->bgdir, ot->varname, ot->name, t, fname);

                        H(das, nobs, obsids, fname, -1, t, ot->varname, (ot->issurface) ? (void*) vv : (void*) vvv, das->Hx);
                        enkf_printf((success) ? "A" : "S");
                        fflush(stdout);
                    } else if (enkf_obstype == OBSTYPE_INNOVATION) {
                        das->Hx[0] = 0;
                        enkf_printf("-");
                        fflush(stdout);
                    }
                }

                free(obsids);
            }
        } else {
            obs_find_bytype(obs, i, &nobs, &obsids, fstatsonly);
            if (nobs == 0)
                goto next;

            if (das->mode == MODE_ENKF || !fstatsonly) {
                for (e = my_first_iteration; e <= my_last_iteration; ++e) {
                    model_getmemberfname(m, das->ensdir, ot->varname, e + 1, fname);
                    H(das, nobs, obsids, fname, e + 1, MAXINT, ot->varname, (ot->issurface) ? (void*) vv : (void*) vvv, das->S[e]);
                    enkf_printf(".");
                    fflush(stdout);
                }
            }

            if (das->mode == MODE_ENOI) {
                if (enkf_obstype == OBSTYPE_VALUE) {
                    model_getbgfname(m, das->bgdir, ot->varname, fname);
                    H(das, nobs, obsids, fname, -1, MAXINT, ot->varname, (ot->issurface) ? (void*) vv : (void*) vvv, das->Hx);
                    enkf_printf("+");
                    fflush(stdout);
                } else if (enkf_obstype == OBSTYPE_INNOVATION) {
                    das->Hx[0] = 0;
                    enkf_printf("-");
                    fflush(stdout);
                }
            }

            free(obsids);
        }

      next:

        if (ot->issurface)
            free2d(vv);
        else
            free3d(vvv);
        enkf_printf("\n");
    }                           /* for i (over obstypes) */

#if defined(MPI)
    if (das->mode == MODE_ENKF || !fstatsonly) {
#if !defined(HE_VIAFILE)
        /*
         * communicate HE via MPI
         */
        int ierror, count;

        /*
         * Blocking communications can create a bottleneck in instances with
         * large number of observations (e.g., 2e6 obs., 144 members, 48
         * processes), but asynchronous send/receive seem to work well
         */
        if (rank > 0) {
            MPI_Request request;

            /*
             * send ensemble observations to master
             */
            count = (my_last_iteration - my_first_iteration + 1) * obs->nobs;
            ierror = MPI_Isend(das->S[my_first_iteration], count, MPIENSOBSTYPE, 0, rank, MPI_COMM_WORLD, &request);
            assert(ierror == MPI_SUCCESS);
        } else {
            int r;
            MPI_Request* requests = malloc((nprocesses - 1) * sizeof(MPI_Request));

            /*
             * collect ensemble observations from slaves
             */
            for (r = 1; r < nprocesses; ++r) {
                count = (last_iteration[r] - first_iteration[r] + 1) * obs->nobs;
                ierror = MPI_Irecv(das->S[first_iteration[r]], count, MPIENSOBSTYPE, r, r, MPI_COMM_WORLD, &requests[r - 1]);
                assert(ierror == MPI_SUCCESS);
            }
            ierror = MPI_Waitall(nprocesses - 1, requests, MPI_STATUS_IGNORE);
            assert(ierror == MPI_SUCCESS);
            free(requests);
        }
        /*
         * now send the full set of ensemble observations to slaves
         */
        count = das->nmem * obs->nobs;
        ierror = MPI_Bcast(das->S[0], count, MPIENSOBSTYPE, 0, MPI_COMM_WORLD);
        assert(ierror == MPI_SUCCESS);
#else
        /*
         * communicate HE via file
         */
        {
            int ncid;
            int varid;
            size_t start[2], count[2];

            if (rank == 0) {
                int dimids[2];

                ncw_create(FNAME_HE, NC_CLOBBER, &ncid);
                ncw_def_dim(FNAME_HE, ncid, "m", das->nmem, &dimids[0]);
                ncw_def_dim(FNAME_HE, ncid, "p", obs->nobs, &dimids[1]);
                ncw_def_var(FNAME_HE, ncid, "HE", NC_FLOAT, 2, dimids, &varid);
                ncw_close(FNAME_HE, ncid);
            }
            MPI_Barrier(MPI_COMM_WORLD);

            ncw_open(FNAME_HE, NC_WRITE, &ncid);
            ncw_inq_varid(FNAME_HE, ncid, "HE", &varid);
            start[0] = my_first_iteration;
            start[1] = 0;
            count[0] = my_last_iteration - my_first_iteration + 1;
            count[1] = obs->nobs;
            ncw_put_vara_float(FNAME_HE, ncid, varid, start, count, das->S[my_first_iteration]);
            ncw_close(FNAME_HE, ncid);
            MPI_Barrier(MPI_COMM_WORLD);

            ncw_open(FNAME_HE, NC_NOWRITE, &ncid);
            ncw_inq_varid(FNAME_HE, ncid, "HE", &varid);
            ncw_get_var_float(FNAME_HE, ncid, varid, das->S[0]);
            ncw_close(FNAME_HE, ncid);
        }
#endif
    }
#endif

    if (das->mode == MODE_ENOI) {
        /*
         * subtract ensemble mean; add background
         */
        if (!fstatsonly) {
            double* ensmean = calloc(obs->nobs, sizeof(double));

            for (e = 0; e < das->nmem; ++e) {
                ENSOBSTYPE* Se = das->S[e];

                for (i = 0; i < obs->nobs; ++i)
                    ensmean[i] += Se[i];
            }
            for (i = 0; i < obs->nobs; ++i)
                ensmean[i] /= (double) das->nmem;

            for (e = 0; e < das->nmem; ++e) {
                ENSOBSTYPE* Se = das->S[e];

                for (i = 0; i < obs->nobs; ++i)
                    Se[i] += das->Hx[i] - ensmean[i];
            }

            free(ensmean);
        } else {
            for (e = 0; e < das->nmem; ++e) {
                ENSOBSTYPE* Se = das->S[e];

                for (i = 0; i < obs->nobs; ++i)
                    Se[i] = das->Hx[i];
            }
        }
    }

    das->s_mode = S_MODE_HE_f;
}

/**
 */
void das_calcinnandspread(dasystem* das)
{
    observations* obs = das->obs;
    int e, o;

    if (das->s_mode == S_MODE_HE_f) {
        if (das->s_f == NULL) {
            das->s_f = calloc(obs->nobs, sizeof(double));
            assert(das->std_f == NULL);
            das->std_f = calloc(obs->nobs, sizeof(double));
        } else {
            memset(das->s_f, 0, obs->nobs * sizeof(double));
            memset(das->std_f, 0, obs->nobs * sizeof(double));
        }

        /*
         * calculate ensemble mean observations 
         */
        for (e = 0; e < das->nmem; ++e) {
            ENSOBSTYPE* Se = das->S[e];

            for (o = 0; o < obs->nobs; ++o)
                das->s_f[o] += (double) Se[o];
        }
        for (o = 0; o < obs->nobs; ++o)
            das->s_f[o] /= (double) das->nmem;

        /*
         * calculate ensemble spread and innovation 
         */
        for (e = 0; e < das->nmem; ++e) {
            ENSOBSTYPE* Se = das->S[e];

            for (o = 0; o < obs->nobs; ++o) {
                Se[o] -= (ENSOBSTYPE) das->s_f[o];
                das->std_f[o] += (double) (Se[o] * Se[o]);
            }
        }
        for (o = 0; o < obs->nobs; ++o) {
            das->std_f[o] = sqrt(das->std_f[o] / (double) (das->nmem - 1));
            das->s_f[o] = obs->data[o].value - das->s_f[o];
            if (!isfinite(das->s_f[o]) || fabs(das->s_f[o]) > STATE_BIGNUM)
                enkf_quit("obs # %d: Hx = %d, no point to continue", o);
        }

        das->s_mode = S_MODE_HA_f;
    } else if (das->s_mode == S_MODE_HE_a) {
        if (das->s_a == NULL) {
            das->s_a = calloc(obs->nobs, sizeof(double));
            assert(das->std_a == NULL);
            das->std_a = calloc(obs->nobs, sizeof(double));
        } else {
            memset(das->s_a, 0, obs->nobs * sizeof(double));
            memset(das->std_a, 0, obs->nobs * sizeof(double));
        }

        /*
         * calculate ensemble mean observations 
         */
        for (e = 0; e < das->nmem; ++e) {
            ENSOBSTYPE* Se = das->S[e];

            for (o = 0; o < obs->nobs; ++o)
                das->s_a[o] += (double) Se[o];
        }
        for (o = 0; o < obs->nobs; ++o)
            das->s_a[o] /= (double) das->nmem;

        /*
         * calculate ensemble spread and innovation 
         */
        for (e = 0; e < das->nmem; ++e) {
            ENSOBSTYPE* Se = das->S[e];

            for (o = 0; o < obs->nobs; ++o) {
                Se[o] -= (ENSOBSTYPE) das->s_a[o];
                das->std_a[o] += (double) (Se[o] * Se[o]);
            }
        }
        for (o = 0; o < obs->nobs; ++o) {
            das->std_a[o] = sqrt(das->std_a[o] / (double) (das->nmem - 1));
            das->s_a[o] = obs->data[o].value - das->s_a[o];
            if (!isfinite(das->s_a[o]) || fabs(das->s_a[o]) > STATE_BIGNUM)
                enkf_quit("obs # %d: Hx = %d, no point to continue", o);
        }

        das->s_mode = S_MODE_HA_a;
    } else
        enkf_quit("programming error");
}

/** Adds forecast observations and forecast ensemble spread to the observation
 * file.
 */
void das_addforecast(dasystem* das, char fname[])
{
    int ncid;
    int dimid_nobs[1];
    size_t nobs;
    int varid_Hx, varid_spread;
    double* Hx;
    int o;

    if (rank != 0)
        return;

    assert(das->s_mode == S_MODE_HA_f);

    ncw_open(fname, NC_WRITE, &ncid);
    if (ncw_var_exists(ncid, "Hx_f")) {
        enkf_printf("  Hx already added to \"%s\" (skipping)\n", fname);
        goto finish;
    }

    ncw_inq_dimid(fname, ncid, "nobs", dimid_nobs);
    ncw_inq_dimlen(fname, ncid, dimid_nobs[0], &nobs);
    ncw_redef(fname, ncid);
    ncw_def_var(fname, ncid, "Hx_f", NC_FLOAT, 1, dimid_nobs, &varid_Hx);
    ncw_def_var(fname, ncid, "std_f", NC_FLOAT, 1, dimid_nobs, &varid_spread);
    ncw_enddef(fname, ncid);

    Hx = calloc(nobs, sizeof(double));
    for (o = 0; o < (int) nobs; ++o)
        Hx[o] = das->obs->data[o].value - das->s_f[o];

    ncw_put_var_double(fname, ncid, varid_Hx, Hx);
    ncw_put_var_double(fname, ncid, varid_spread, das->std_f);

    free(Hx);

  finish:
    ncw_close(fname, ncid);
}

/** Modifies observation error so that the increment for this observation would
 * not exceed KFACTOR * <ensemble spread> (all in observation space) after
 * assimilating this observation only. This can be viewed as an adaptive QC.
 */
void das_moderateobs(dasystem* das)
{
    observations* obs = das->obs;
    double kfactor = das->kfactor;
    double* std_new;
    int i;

    if (!isfinite(kfactor))
        return;

    assert(das->s_mode == S_MODE_HA_f);

    std_new = malloc(obs->nobs * sizeof(double));

    for (i = 0; i < obs->nobs; ++i) {
        measurement* o = &obs->data[i];
        double svar = das->std_f[i] * das->std_f[i];
        double ovar = o->std * o->std;
        double inn = das->s_f[i];

        std_new[i] = sqrt(sqrt((svar + ovar) * (svar + ovar) + svar * inn * inn / kfactor / kfactor) - svar);
        if (svar > 0.0 && std_new[i] * std_new[i] / ovar > 2.0) {
            obs->nmodified++;
            obs->obstypes[o->type].nmodified++;
        }
    }

    for (i = 0; i < obs->nobs; ++i)
        obs->data[i].std = std_new[i];

    enkf_printf("    observations substantially modified:\n");
    for (i = 0; i < obs->nobstypes; ++i)
        enkf_printf("      %s    %7d (%.1f%%)\n", obs->obstypes[i].name, obs->obstypes[i].nmodified, 100.0 * (double) obs->obstypes[i].nmodified / (double) obs->obstypes[i].nobs);
    enkf_printf("      total  %7d (%.1f%%)\n", obs->nmodified, 100.0 * (double) obs->nmodified / (double) obs->nobs);

    free(std_new);
}

/** Replaces observation errors in the observation file with the modified
 * values. The original values are stored as "std_orig".
 */
void das_addmodifiederrors(dasystem* das, char fname[])
{
    int ncid;
    int dimid_nobs[1];
    size_t nobs;
    int varid_std;
    double* std;
    int i;

    if (rank != 0)
        return;

    ncw_open(fname, NC_WRITE, &ncid);
    ncw_inq_dimid(fname, ncid, "nobs", dimid_nobs);
    ncw_inq_dimlen(fname, ncid, dimid_nobs[0], &nobs);

    ncw_redef(fname, ncid);
    if (ncw_var_exists(ncid, "std_orig"))
        enkf_quit("\"observations.nc\" has already been modified by `enkf_calc\'. To proceed please remove observations*.nc and rerun `enkf_prep\'.");
    ncw_rename_var(fname, ncid, "std", "std_orig");
    ncw_def_var(fname, ncid, "std", NC_FLOAT, 1, dimid_nobs, &varid_std);
    ncw_enddef(fname, ncid);

    std = malloc(nobs * sizeof(double));

    for (i = 0; i < (int) nobs; ++i)
        std[i] = das->obs->data[i].std;
    ncw_put_var_double(fname, ncid, varid_std, std);

    free(std);

    ncw_close(fname, ncid);
}

/**
 */
void das_standardise(dasystem* das)
{
    observations* obs = das->obs;
    double mult = sqrt((double) das->nmem - 1);
    int e, i;

    if (das->s_mode == S_MODE_HA_f) {
        for (e = 0; e < das->nmem; ++e) {
            ENSOBSTYPE* Se = das->S[e];

            for (i = 0; i < obs->nobs; ++i) {
                measurement* o = &obs->data[i];

                Se[i] /= o->std * sqrt(obs->obstypes[o->type].rfactor) * mult;
            }
        }
        for (i = 0; i < obs->nobs; ++i) {
            measurement* o = &obs->data[i];

            das->s_f[i] /= o->std * sqrt(obs->obstypes[o->type].rfactor) * mult;
        }
        das->s_mode = S_MODE_S_f;
    } else if (das->s_mode == S_MODE_HA_a) {
        for (e = 0; e < das->nmem; ++e) {
            ENSOBSTYPE* Se = das->S[e];

            for (i = 0; i < obs->nobs; ++i) {
                measurement* o = &obs->data[i];

                Se[i] /= o->std * sqrt(obs->obstypes[o->type].rfactor) * mult;
            }
        }
        for (i = 0; i < obs->nobs; ++i) {
            measurement* o = &obs->data[i];

            das->s_a[i] /= o->std * sqrt(obs->obstypes[o->type].rfactor) * mult;
        }
        das->s_mode = S_MODE_S_a;
    } else
        enkf_quit("programming error");
}

/**
 */
static int cmp_obs_byij(const void* p1, const void* p2)
{
    measurement* m1 = (measurement*) p1;
    measurement* m2 = (measurement*) p2;
    int i1, i2;

    i1 = (int) floor(m1->fj);
    i2 = (int) floor(m2->fj);
    if (i1 > i2)
        return 1;
    if (i1 < i2)
        return -1;

    i1 = (int) floor(m1->fi);
    i2 = (int) floor(m2->fi);
    if (i1 > i2)
        return 1;
    if (i1 < i2)
        return -1;

    return 0;
}

/**
 */
static void das_destandardise(dasystem* das)
{
    observations* obs = das->obs;
    double mult = sqrt((double) das->nmem - 1);
    int e, i;

    if (das->s_mode == S_MODE_HA_f || das->s_mode == S_MODE_HA_a)
        return;
    else if (das->s_mode == S_MODE_S_f) {
        for (e = 0; e < das->nmem; ++e) {
            ENSOBSTYPE* Se = das->S[e];

            for (i = 0; i < obs->nobs; ++i) {
                measurement* o = &obs->data[i];

                Se[i] *= o->std * sqrt(obs->obstypes[o->type].rfactor) * mult;
            }
        }
        for (i = 0; i < obs->nobs; ++i) {
            measurement* o = &obs->data[i];

            das->s_f[i] *= o->std * sqrt(obs->obstypes[o->type].rfactor) * mult;
        }
        das->s_mode = S_MODE_HA_f;
    } else if (das->s_mode == S_MODE_S_a) {
        for (e = 0; e < das->nmem; ++e) {
            ENSOBSTYPE* Se = das->S[e];

            for (i = 0; i < obs->nobs; ++i) {
                measurement* o = &obs->data[i];

                Se[i] *= o->std * sqrt(obs->obstypes[o->type].rfactor) * mult;
            }
        }
        for (i = 0; i < obs->nobs; ++i) {
            measurement* o = &obs->data[i];

            das->s_a[i] *= o->std * sqrt(obs->obstypes[o->type].rfactor) * mult;
        }
        das->s_mode = S_MODE_HA_a;
    } else
        enkf_quit("programming error");
}

/** Sorts all observations by j,i so that they can be processed in a single
 * cycle over horizontal grid cells.
 */
static void sortobs_byij(dasystem* das)
{
    observations* obs = das->obs;
    ENSOBSTYPE** Snew;
    int o, e;

    assert(das->s_mode == S_MODE_HA_f);

    enkf_printf("    sorting obs by ij:\n");
    qsort(obs->data, obs->nobs, sizeof(measurement), cmp_obs_byij);

    Snew = alloc2d(das->nmem, obs->nobs, sizeof(ENSOBSTYPE));

    for (e = 0; e < das->nmem; ++e) {
        ENSOBSTYPE* Se = das->S[e];
        ENSOBSTYPE* Snewe = Snew[e];

        for (o = 0; o < obs->nobs; ++o)
            /*
             * das->s_f is innovation = obs - forecast; hence forecast = obs
             * - innovation 
             */
            Snewe[o] = Se[obs->data[o].id] + obs->data[o].value - das->s_f[obs->data[o].id];
    }
    free2d(das->S);
    das->S = Snew;

    das->s_mode = S_MODE_HE_f;
}

/** Rolls back sorting of ensemble observations S by i,j, so that S is again
 * consistent with other variables.
 */
static void unsortobs_byij(dasystem* das)
{
    observations* obs = das->obs;
    ENSOBSTYPE** Snew;          /* for consistency only */
    double* snew;
    double* stdnew;
    int o, e;

    assert(das->s_mode == S_MODE_HA_a);

    snew = calloc(obs->nobs, sizeof(double));
    stdnew = calloc(obs->nobs, sizeof(double));

    for (o = 0; o < obs->nobs; ++o) {
        snew[obs->data[o].id] = das->s_a[o];
        stdnew[obs->data[o].id] = das->std_a[o];
    }
    free(das->s_a);
    das->s_a = snew;
    free(das->std_a);
    das->std_a = stdnew;

    Snew = alloc2d(das->nmem, obs->nobs, sizeof(ENSOBSTYPE));
    for (e = 0; e < das->nmem; ++e) {
        ENSOBSTYPE* Se = das->S[e];
        ENSOBSTYPE* Snewe = Snew[e];

        for (o = 0; o < obs->nobs; ++o)
            Snewe[obs->data[o].id] = Se[o];
    }
    free2d(das->S);
    das->S = Snew;

    /*
     * Ensemble can collapse to zero spread on the boundary, therefore
     * commenting out.
     *
     * for (o = 0; o < obs->nobs; ++o)
     *     assert(das->std_a[o] != 0.0);
     */
}

/** Updates ensemble observations by applying X5
 */
static void update_HE(dasystem* das)
{
    model* m = das->m;
    int nvar = model_getnvar(m);
    observations* obs = das->obs;
    int periodic_i = grid_isperiodic_x(model_getgrid(m));
    int periodic_j = grid_isperiodic_y(model_getgrid(m));
    int ncid;
    int varid;
    int dimids[3];
    size_t dimlens[3];
    float** X5jj;
    float** X5jj1 = NULL;
    float** X5jj2 = NULL;
    float** X5j;
    int i, j, ni, nj, mni, mnj, mnk;
    int* iiter;
    int* jiter;
    int jj, stepj, ii, stepi;
    size_t start[3], count[3];
    int e, o;
    float* HEi;
    float* tmp;
    char do_T = 'T';
    float alpha = 1.0f;
    float beta = 0.0f;
    int inc = 1;
    int* varids = NULL;

    enkf_printf("    updating HE:\n");
    assert(das->s_mode == S_MODE_HE_f);

    /*
     * for each observation type precompute id of the associated variable for
     * quicker access to inflation
     */
    varids = malloc(obs->nobstypes * sizeof(int));
    for (i = 0; i < obs->nobstypes; ++i) {
        for (j = 0; j < nvar; ++j) {
            if (strcmp(obs->obstypes[i].varname, model_getvarname(m, j)) == 0) {
                varids[i] = j;
                break;
            }
        }
        assert(j < nvar);
    }

    /*
     * the following code for interpolation of X5 essentially coincides with
     * that in das_updatefields() 
     */

    model_getdims(m, &mni, &mnj, &mnk);

    ncw_open(FNAME_X5, NC_NOWRITE, &ncid);
    ncw_inq_varid(FNAME_X5, ncid, "X5", &varid);
    ncw_inq_vardimid(FNAME_X5, ncid, varid, dimids);
    for (i = 0; i < 3; ++i)
        ncw_inq_dimlen(FNAME_X5, ncid, dimids[i], &dimlens[i]);
    ni = dimlens[1];
    nj = dimlens[0];

    assert((int) dimlens[2] == das->nmem * das->nmem);

    jiter = malloc((nj + 1) * sizeof(int));     /* "+ 1" to handle periodic
                                                 * grids */
    iiter = malloc((ni + 1) * sizeof(int));
    for (j = 0, i = 0; j < nj; ++j, i += das->stride)
        jiter[j] = i;
    if (periodic_j)
        jiter[nj] = jiter[nj - 1] + das->stride;
    for (i = 0, j = 0; i < ni; ++i, j += das->stride)
        iiter[i] = j;
    if (periodic_i)
        iiter[ni] = iiter[ni - 1] + das->stride;

    start[0] = 0;
    start[1] = 0;
    start[2] = 0;
    count[0] = 1;
    count[1] = ni;
    count[2] = das->nmem * das->nmem;
    X5j = alloc2d(mni, das->nmem * das->nmem, sizeof(float));
    if (das->stride > 1) {
        X5jj = alloc2d(ni, das->nmem * das->nmem, sizeof(float));
        X5jj1 = alloc2d(ni, das->nmem * das->nmem, sizeof(float));
        X5jj2 = alloc2d(ni, das->nmem * das->nmem, sizeof(float));
        ncw_get_vara_float(FNAME_X5, ncid, varid, start, count, X5jj2[0]);
    }
    HEi = malloc(das->nmem * sizeof(ENSOBSTYPE));
    tmp = malloc(das->nmem * sizeof(ENSOBSTYPE));
    /*
     * jj, ii are the indices of the subsampled grid; i, j are the indices of
     * the actual model grid 
     */
    for (jj = 0, j = 0, o = 0; jj < nj; ++jj) {
        for (stepj = 0; stepj < das->stride && j < mnj; ++stepj, ++j) {
            if (das->stride == 1) {
                /*
                 * no interpolation necessary; simply read the ETMs for the
                 * j-th row from disk 
                 */
                start[0] = j;
                ncw_get_vara_float(FNAME_X5, ncid, varid, start, count, X5j[0]);
            } else {
                /*
                 * the following code interpolates the ETM back to the
                 * original grid, first by j, and then by i 
                 */
                if (stepj == 0) {
                    memcpy(X5jj[0], X5jj2[0], ni * das->nmem * das->nmem * sizeof(float));
                    memcpy(X5jj1[0], X5jj2[0], ni * das->nmem * das->nmem * sizeof(float));
                    if (jj < nj - 1 || periodic_j) {
                        start[0] = (jj + 1) % nj;
                        ncw_get_vara_float(FNAME_X5, ncid, varid, start, count, X5jj2[0]);
                    }
                } else {
                    float weight2 = (float) stepj / das->stride;
                    float weight1 = (float) 1.0 - weight2;

                    for (ii = 0; ii < ni; ++ii) {
                        float* X5jjii = X5jj[ii];
                        float* X5jj1ii = X5jj1[ii];
                        float* X5jj2ii = X5jj2[ii];

                        for (e = 0; e < das->nmem * das->nmem; ++e)
                            X5jjii[e] = X5jj1ii[e] * weight1 + X5jj2ii[e] * weight2;
                    }
                }

                for (ii = 0, i = 0; ii < ni; ++ii) {
                    for (stepi = 0; stepi < das->stride && i < mni; ++stepi, ++i) {
                        if (stepi == 0)
                            memcpy(X5j[i], X5jj[ii], das->nmem * das->nmem * sizeof(float));
                        else {
                            float weight2 = (float) stepi / das->stride;
                            float weight1 = (float) 1.0 - weight2;
                            float* X5jjii1 = X5jj[ii];
                            float* X5ji = X5j[i];
                            float* X5jjii2;

                            if (ii < ni - 1)
                                X5jjii2 = X5jj[ii + 1];
                            else
                                X5jjii2 = X5jj[(periodic_i) ? (ii + 1) % ni : ii];

                            for (e = 0; e < das->nmem * das->nmem; ++e)
                                X5ji[e] = X5jjii1[e] * weight1 + X5jjii2[e] * weight2;
                        }
                    }
                }
            }                   /* stride != 1 */

            /*
             * (at this stage X5j should contain the array of X5 matrices
             * for the j-th row of the grid) 
             */

            if ((int) (obs->data[o].fj) > j)
                continue;

            for (; o < obs->nobs && (int) (obs->data[o].fj) == j; ++o) {
                float inflation = model_getvarinflation(m, varids[obs->data[o].type]);

                /*
                 * HE(i, :) = HE(i, :) * X5 
                 */
                i = (int) (obs->data[o].fi);
                for (e = 0; e < das->nmem; ++e)
                    HEi[e] = das->S[e][o];
                sgemv_(&do_T, &das->nmem, &das->nmem, &alpha, X5j[i], &das->nmem, HEi, &inc, &beta, tmp, &inc);
                /*
                 * applying inflation:
                 */
                if (fabsf(inflation - 1.0f) > EPSF) {
                    float v_av = 0.0f;

                    for (e = 0; e < das->nmem; ++e)
                        v_av += tmp[e];
                    v_av /= (float) das->nmem;
                    for (e = 0; e < das->nmem; ++e)
                        tmp[e] = (tmp[e] - v_av) * inflation + v_av;
                }

                for (e = 0; e < das->nmem; ++e)
                    das->S[e][o] = tmp[e];
            }

        }                       /* for stepj */
    }                           /* for jj */

    ncw_close(FNAME_X5, ncid);

    free(tmp);
    free(varids);
    free(HEi);
    free(iiter);
    free(jiter);
    free2d(X5j);
    if (das->stride > 1) {
        free2d(X5jj);
        free2d(X5jj1);
        free2d(X5jj2);
    }

    das->s_mode = S_MODE_HE_a;
}                               /* update_HE() */

/**
 */
static void update_Hx(dasystem* das)
{
    model* m = das->m;
    observations* obs = das->obs;
    int periodic_i = grid_isperiodic_x(model_getgrid(m));
    int periodic_j = grid_isperiodic_y(model_getgrid(m));
    int ncid;
    int varid;
    int dimids[3];
    size_t dimlens[3];
    float** wjj;
    float** wjj1 = NULL;
    float** wjj2 = NULL;
    float** wj;
    int i, j, ni, nj, mni, mnj, mnk;
    int* iiter;
    int* jiter;
    int jj, stepj, ii, stepi;
    size_t start[3], count[3];
    int e, o;
    float* tmp;

    enkf_printf("    updating Hx:\n");
    assert(das->s_mode == S_MODE_HE_f);

    /*
     * the following code for interpolation of X5 essentially coincides with
     * that in das_updatefields() 
     */

    model_getdims(m, &mni, &mnj, &mnk);

    ncw_open(FNAME_W, NC_NOWRITE, &ncid);
    ncw_inq_varid(FNAME_W, ncid, "w", &varid);
    ncw_inq_vardimid(FNAME_W, ncid, varid, dimids);
    for (i = 0; i < 3; ++i)
        ncw_inq_dimlen(FNAME_W, ncid, dimids[i], &dimlens[i]);
    ni = dimlens[1];
    nj = dimlens[0];

    assert((int) dimlens[2] == das->nmem);

    jiter = malloc((nj + 1) * sizeof(int));     /* "+ 1" to handle periodic
                                                 * grids */
    iiter = malloc((ni + 1) * sizeof(int));
    for (j = 0, i = 0; j < nj; ++j, i += das->stride)
        jiter[j] = i;
    if (periodic_j)
        jiter[nj] = jiter[nj - 1] + das->stride;
    for (i = 0, j = 0; i < ni; ++i, j += das->stride)
        iiter[i] = j;
    if (periodic_i)
        iiter[ni] = iiter[ni - 1] + das->stride;

    start[0] = 0;
    start[1] = 0;
    start[2] = 0;
    count[0] = 1;
    count[1] = ni;
    count[2] = das->nmem;
    wj = alloc2d(mni, das->nmem, sizeof(float));
    if (das->stride > 1) {
        wjj = alloc2d(ni, das->nmem, sizeof(float));
        wjj1 = alloc2d(ni, das->nmem, sizeof(float));
        wjj2 = alloc2d(ni, das->nmem, sizeof(float));
        ncw_get_vara_float(FNAME_W, ncid, varid, start, count, wjj2[0]);
    }
    tmp = malloc(das->nmem * sizeof(float));
    /*
     * jj, ii are the indices of the subsampled grid; i, j are the indices of
     * the actual model grid 
     */
    for (jj = 0, j = 0, o = 0; jj < nj; ++jj) {
        for (stepj = 0; stepj < das->stride && j < mnj; ++stepj, ++j) {
            if (das->stride == 1) {
                /*
                 * no interpolation necessary; simply read the ETMs for the
                 * j-th row from disk 
                 */
                start[0] = j;
                ncw_get_vara_float(FNAME_W, ncid, varid, start, count, wj[0]);
            } else {
                /*
                 * the following code interpolates the ETM back to the
                 * original grid, first by j, and then by i 
                 */
                if (stepj == 0) {
                    memcpy(wjj[0], wjj2[0], ni * das->nmem * sizeof(float));
                    memcpy(wjj1[0], wjj2[0], ni * das->nmem * sizeof(float));
                    if (jj < nj - 1 || periodic_j) {
                        start[0] = (jj + 1) % nj;
                        ncw_get_vara_float(FNAME_W, ncid, varid, start, count, wjj2[0]);
                    }
                } else {
                    float weight2 = (float) stepj / das->stride;
                    float weight1 = (float) 1.0 - weight2;

                    for (ii = 0; ii < ni; ++ii) {
                        float* wjjii = wjj[ii];
                        float* wjj1ii = wjj1[ii];
                        float* wjj2ii = wjj2[ii];

                        for (e = 0; e < das->nmem; ++e)
                            wjjii[e] = wjj1ii[e] * weight1 + wjj2ii[e] * weight2;
                    }
                }

                for (ii = 0, i = 0; ii < ni; ++ii) {
                    for (stepi = 0; stepi < das->stride && i < mni; ++stepi, ++i) {
                        if (stepi == 0)
                            memcpy(wj[i], wjj[ii], das->nmem * sizeof(float));
                        else {
                            float weight2 = (float) stepi / das->stride;
                            float weight1 = (float) 1.0 - weight2;
                            float* wjjii1 = wjj[ii];
                            float* wji = wj[i];
                            float* wjjii2;

                            if (ii < ni - 1)
                                wjjii2 = wjj[ii + 1];
                            else
                                wjjii2 = wjj[(periodic_i) ? (ii + 1) % ni : ii];

                            for (e = 0; e < das->nmem; ++e)
                                wji[e] = wjjii1[e] * weight1 + wjjii2[e] * weight2;
                        }
                    }
                }
            }                   /* stride != 1 */

            /*
             * (at this stage wj should contain the array of b vectors for
             * the j-th row of the grid) 
             */

            if ((int) (obs->data[o].fj) > j)
                continue;

            for (; o < obs->nobs && (int) (obs->data[o].fj) == j; ++o) {
                double dHx = 0.0;
                double Hx = 0.0;

                for (e = 0; e < das->nmem; ++e)
                    Hx += das->S[e][o];
                Hx /= (double) das->nmem;

                i = (int) (obs->data[o].fi);

                /*
                 * HE(i, :) += HA(i, :) * b * 1' 
                 */
                for (e = 0; e < das->nmem; ++e)
                    dHx += (das->S[e][o] - Hx) * wj[i][e];
                for (e = 0; e < das->nmem; ++e)
                    das->S[e][o] += dHx;
            }

        }                       /* for stepj */
    }                           /* for jj */

    ncw_close(FNAME_W, ncid);

    free(tmp);
    free(iiter);
    free(jiter);
    free2d(wj);
    if (das->stride > 1) {
        free2d(wjj);
        free2d(wjj1);
        free2d(wjj2);
    }

    das->s_mode = S_MODE_HE_a;
}                               /* update_Hx() */

/**
 */
void das_updateHE(dasystem* das)
{
    if (rank != 0)
        return;

    das_destandardise(das);
    sortobs_byij(das);
    if (das->mode == MODE_ENKF)
        update_HE(das);
    else
        update_Hx(das);
    das_calcinnandspread(das);
    unsortobs_byij(das);
}

/** Modify the observation error in file FNAME_SOBS.
 */
void das_addanalysis(dasystem* das, char fname[])
{
    int ncid;
    int dimid_nobs[1];
    size_t nobs;
    int varid_Hx, varid_spread;
    measurement* data = das->obs->data;
    double* s;
    int i;

    if (rank != 0)
        return;

    assert(das->s_mode == S_MODE_HA_a);

    ncw_open(fname, NC_WRITE, &ncid);
    ncw_inq_dimid(fname, ncid, "nobs", dimid_nobs);
    ncw_inq_dimlen(fname, ncid, dimid_nobs[0], &nobs);

    ncw_redef(fname, ncid);
    ncw_def_var(fname, ncid, "Hx_a", NC_FLOAT, 1, dimid_nobs, &varid_Hx);
    ncw_def_var(fname, ncid, "std_a", NC_FLOAT, 1, dimid_nobs, &varid_spread);
    ncw_enddef(fname, ncid);

    ncw_put_var_double(fname, ncid, varid_spread, das->std_a);
    s = malloc(nobs * sizeof(double));
    /*
     * the obs are still sorted by ij 
     */
    for (i = 0; i < (int) nobs; ++i)
        s[data[i].id] = data[i].value;
    for (i = 0; i < (int) nobs; ++i)
        s[i] -= das->s_a[i];
    ncw_put_var_double(fname, ncid, varid_Hx, s);
    free(s);

    ncw_close(fname, ncid);
}