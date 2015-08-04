/******************************************************************************
 *
 * File:        transforms.c        
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
#include <assert.h>
#include <string.h>
#include <math.h>
#include "definitions.h"
#include "nan.h"
#include "utils.h"
#include "distribute.h"
#include "calcs.h"
#include "dasystem.h"
#include "pointlog.h"

/**
 */
static void nc_createX5(char fname[], int nj, int ni, int stride, int nmem, int* ncid, int* varid_X5)
{
    int dimids[3];

    assert(rank == 0);

    enkf_printf("      creating empty file \"%s\":\n", fname);
    ncw_create(fname, NC_CLOBBER | NC_64BIT_OFFSET, ncid);
    ncw_def_dim(fname, *ncid, "nj", nj, &dimids[0]);
    ncw_def_dim(fname, *ncid, "ni", ni, &dimids[1]);
    ncw_def_dim(fname, *ncid, "msq", nmem * nmem, &dimids[2]);
    ncw_put_att_int(fname, *ncid, NC_GLOBAL, "stride", 1, &stride);
    ncw_def_var(fname, *ncid, "X5", NC_FLOAT, 3, dimids, varid_X5);
    ncw_enddef(fname, *ncid);
}

/**
 */
static void nc_writeX5(char fname[], int ncid, int j, int ni, int nmem, int varid_X5, float* X5j)
{
    size_t start[3], count[3];

    assert(rank == 0);

    start[0] = j;
    start[1] = 0;
    start[2] = 0;

    count[0] = 1;
    count[1] = ni;
    count[2] = nmem * nmem;

    ncw_put_vara_float(fname, ncid, varid_X5, start, count, X5j);
}

/**
 */
static void nc_createw(char fname[], int nj, int ni, int stride, int nmem, int* ncid, int* varid_w)
{
    int dimids[3];

    assert(rank == 0);

    enkf_printf("    creating empty file \"%s\":\n", fname);
    ncw_create(fname, NC_CLOBBER | NC_64BIT_OFFSET, ncid);
    ncw_def_dim(fname, *ncid, "nj", nj, &dimids[0]);
    ncw_def_dim(fname, *ncid, "ni", ni, &dimids[1]);
    ncw_def_dim(fname, *ncid, "m", nmem, &dimids[2]);
    ncw_put_att_int(fname, *ncid, NC_GLOBAL, "stride", 1, &stride);
    ncw_def_var(fname, *ncid, "w", NC_FLOAT, 3, dimids, varid_w);
    ncw_enddef(fname, *ncid);
}

/**
 */
static void nc_writew(char fname[], int ncid, int j, int ni, int nmem, int varid_w, float* wj)
{
    size_t start[3], count[3];

    assert(rank == 0);

    start[0] = j;
    start[1] = 0;
    start[2] = 0;

    count[0] = 1;
    count[1] = ni;
    count[2] = nmem;

    ncw_put_vara_float(fname, ncid, varid_w, start, count, wj);
}

typedef struct {
    int nlobs_sum;
    int nlobs_max;
    int n_inv_obs;
    int n_inv_ens;
    int ncell;
} calcstats;

/**
 */
static void nc_writediag(char fname[], int nobstypes, int nj, int ni, int stride, int** nlobs, float** dfs, float** srf, int*** pnlobs, float*** pdfs, float*** psrf)
{
    int ncid;
    int dimids[3];
    int varid_nlobs, varid_dfs, varid_srf, varid_pnlobs, varid_pdfs, varid_psrf;

    assert(rank == 0);

    enkf_printf("    writing stats to \"%s\":\n", fname);
    ncw_create(fname, NC_CLOBBER | NC_64BIT_OFFSET, &ncid);
    ncw_def_dim(fname, ncid, "nobstypes", nobstypes, &dimids[0]);
    ncw_def_dim(fname, ncid, "nj", nj, &dimids[1]);
    ncw_def_dim(fname, ncid, "ni", ni, &dimids[2]);
    ncw_put_att_int(fname, ncid, NC_GLOBAL, "stride", 1, &stride);
    ncw_def_var(fname, ncid, "nlobs", NC_INT, 2, &dimids[1], &varid_nlobs);
    ncw_def_var(fname, ncid, "dfs", NC_FLOAT, 2, &dimids[1], &varid_dfs);
    ncw_def_var(fname, ncid, "srf", NC_FLOAT, 2, &dimids[1], &varid_srf);
    ncw_def_var(fname, ncid, "pnlobs", NC_INT, 3, dimids, &varid_pnlobs);
    ncw_def_var(fname, ncid, "pdfs", NC_FLOAT, 3, dimids, &varid_pdfs);
    ncw_def_var(fname, ncid, "psrf", NC_FLOAT, 3, dimids, &varid_psrf);
    ncw_enddef(fname, ncid);

    ncw_put_var_int(fname, ncid, varid_nlobs, nlobs[0]);
    ncw_put_var_float(fname, ncid, varid_dfs, dfs[0]);
    ncw_put_var_float(fname, ncid, varid_srf, srf[0]);
    ncw_put_var_int(fname, ncid, varid_pnlobs, pnlobs[0][0]);
    ncw_put_var_float(fname, ncid, varid_pdfs, pdfs[0][0]);
    ncw_put_var_float(fname, ncid, varid_psrf, psrf[0][0]);

    ncw_close(fname, ncid);
}

#if !defined(SHUFFLE_ROWS)
/** Distributes iterations in such a way that for any given i iterations # i 
 * for each process are grouped together.
 */
static void group_iterations(int n, int ids[])
{
    int iter, r, i;

    assert(first_iteration[0] == 0);
    assert(last_iteration[nprocesses - 1] == n - 1);
    for (iter = 0, i = 0; iter <= last_iteration[0]; ++iter)
        for (r = 0; r < nprocesses && i < n; ++r, ++i)
            ids[first_iteration[r] + iter] = i;
}
#endif

/** The central DA procedure, where the actual transforms are calculated.
 */
void das_calctransforms(dasystem* das)
{
    model* m = das->m;
    observations* obs = das->obs;
    int ngrid = model_getngrid(m);
    int gid;

    assert(das->s_mode == S_MODE_HA_f);
    das_standardise(das);

    for (gid = 0; gid < ngrid; ++gid) {
        void* grid = model_getgridbyid(m, gid);
        char fname_XW[MAXSTRLEN];

        int mni, mnj;
        int nj, ni;
        int* jiter = NULL;
        int* iiter = NULL;

        int ncid_X5 = -1;
        int varid_X5 = -1;
        int ncid_w = -1;
        int varid_w = -1;

        /*
         * transforms 
         */
        float** X5j = NULL;     /* X5 for one grid row [ni][m x m] */
        double** X5 = NULL;     /* X5 for one grid cell [m][m] */

        /*
         * coeffs 
         */
        float** wj = NULL;
        double* w = NULL;

        /*
         * stats 
         */
        calcstats stats = { 0, 0, 0, 0, 0 };
        int** nlobs = NULL;
        int*** pnlobs = NULL;
        float** dfs = NULL;
        float*** pdfs = NULL;
        float** srf = NULL;
        float*** psrf = NULL;

        int* jpool;
        int i, j, ii, jj, ot, jjj;

        /*
         * skip this grid if there are no model variables associated with it
         */
        for (i = 0; i < model_getnvar(m); ++i)
            if (model_getvargridid(m, i) == gid)
                break;
        if (i == model_getnvar(m))
            continue;

        enkf_printf("    calculating transforms for %s:\n", grid_getname(grid));

        obs_createkdtrees(obs, m);
        grid_getdims(grid, &mni, &mnj, NULL);

        /*
         * work out how to cycle j 
         */
        nj = (mnj - 1) / das->stride + 1;
        jiter = malloc(nj * sizeof(int));
        for (j = 0, i = 0; j < nj; ++j, i += das->stride)
            jiter[j] = i;
        ni = (mni - 1) / das->stride + 1;
        iiter = malloc(ni * sizeof(int));
        for (i = 0, j = 0; i < ni; ++i, j += das->stride)
            iiter[i] = j;

        if (das->mode == MODE_ENKF) {
            das_getfname_X5(das, grid, fname_XW);

            if (rank == 0)
                nc_createX5(fname_XW, nj, ni, das->stride, das->nmem, &ncid_X5, &varid_X5);
            X5j = alloc2d(ni, das->nmem * das->nmem, sizeof(float));
            X5 = alloc2d(das->nmem, das->nmem, sizeof(double));
        } else if (das->mode == MODE_ENOI) {
            das_getfname_w(das, grid, fname_XW);

            if (rank == 0)
                nc_createw(fname_XW, nj, ni, das->stride, das->nmem, &ncid_w, &varid_w);
            wj = alloc2d(ni, das->nmem, sizeof(float));
            w = malloc(das->nmem * sizeof(double));
        } else
            enkf_quit("programming error");

        distribute_iterations(0, nj - 1, nprocesses, rank, "      ");
        assert(my_number_of_iterations <= number_of_iterations[0]);

        if (rank == 0) {
            nlobs = alloc2d(nj, ni, sizeof(int));
            dfs = alloc2d(nj, ni, sizeof(float));
            srf = alloc2d(nj, ni, sizeof(float));
            pnlobs = alloc3d(obs->nobstypes, nj, ni, sizeof(int));
            pdfs = alloc3d(obs->nobstypes, nj, ni, sizeof(float));
            psrf = alloc3d(obs->nobstypes, nj, ni, sizeof(float));
        } else {
            nlobs = alloc2d(my_last_iteration - my_first_iteration + 1, ni, sizeof(int));
            dfs = alloc2d(my_last_iteration - my_first_iteration + 1, ni, sizeof(float));
            srf = alloc2d(my_last_iteration - my_first_iteration + 1, ni, sizeof(float));
            pnlobs = alloc3d(obs->nobstypes, my_last_iteration - my_first_iteration + 1, ni, sizeof(int));
            pdfs = alloc3d(obs->nobstypes, my_last_iteration - my_first_iteration + 1, ni, sizeof(float));
            psrf = alloc3d(obs->nobstypes, my_last_iteration - my_first_iteration + 1, ni, sizeof(float));
        }

        /*
         * main cycle 
         */
        enkf_printf("      main cycle for %s (%d x %d local analyses):\n", grid_getname(grid), nj, ni);

        enkf_flush();
#if defined(MPI)
        MPI_Barrier(MPI_COMM_WORLD);
#endif
        jpool = malloc(nj * sizeof(int));
        if (rank == 0) {
            for (j = 0; j < nj; ++j)
                jpool[j] = j;
            if (nprocesses > 1) {
                /*
                 * If writing of X5 is organised so that each process writes
                 * independently, then it may have sense to distribute the
                 * iterations randomly, so that the total time taken by each
                 * process is approximately equal.
                 */
#if defined(SHUFFLE_ROWS)
                shuffle(nj, jpool);
#else
                group_iterations(nj, jpool);
#endif
            }
        }
#if defined(MPI)
        {
            int ierror = MPI_Bcast(jpool, nj, MPI_INTEGER, 0, MPI_COMM_WORLD);

            assert(ierror == MPI_SUCCESS);
        }
#endif
        for (jj = my_first_iteration; jj <= my_last_iteration; ++jj) {

            j = jiter[jpool[jj]];
            printf("        j = %d (%d: %d: %.1f%%)\n", j, rank, jj, 100.0 * (double) (jj - my_first_iteration + 1) / (double) (my_last_iteration - my_first_iteration + 1));
            fflush(stdout);

            if (rank == 0)
                jjj = jpool[jj];
            else
                jjj = jj - my_first_iteration;

            for (ii = 0; ii < ni; ++ii) {
                int ploc = 0;   /* `nlobs' already engaged, using another
                                 * name */
                int* lobs = NULL;
                int* plobs = NULL;
                double* lcoeffs = NULL;
                double** Sloc = NULL;
                double* sloc = NULL;
                double** G = NULL;
                int e, o;

                i = iiter[ii];

                obs_findlocal(obs, m, grid, i, j, &ploc, &lobs, &lcoeffs);

                assert(ploc >= 0 && ploc <= obs->nobs);

                if (ploc > stats.nlobs_max)
                    stats.nlobs_max = ploc;
                stats.nlobs_sum += ploc;
                stats.ncell++;

                /*
                 * set X5 = 0 
                 */
                if (das->mode == MODE_ENKF)
                    memset(X5[0], 0, das->nmem * das->nmem * sizeof(double));
                else if (das->mode == MODE_ENOI)
                    memset(w, 0, das->nmem * sizeof(double));

                if (ploc == 0) {
                    if (das->mode == MODE_ENKF) {
                        /*
                         * set X5 = I 
                         */
                        memset(X5j[ii], 0, das->nmem * das->nmem * sizeof(float));
                        for (e = 0; e < das->nmem; ++e)
                            X5j[ii][e * das->nmem + e] = (float) 1.0;
                    } else if (das->mode == MODE_ENOI)
                        memset(wj[ii], 0, das->nmem * sizeof(float));

                    nlobs[jjj][ii] = 0;
                    dfs[jjj][ii] = 0.0;
                    srf[jjj][ii] = 0.0;
                    for (ot = 0; ot < obs->nobstypes; ot++) {
                        pnlobs[ot][jjj][ii] = 0;
                        pdfs[ot][jjj][ii] = 0.0;
                        psrf[ot][jjj][ii] = 0.0;
                    }
                    continue;
                }

                Sloc = alloc2d(das->nmem, ploc, sizeof(double));
                sloc = malloc(ploc * sizeof(double));
                G = alloc2d(ploc, das->nmem, sizeof(double));
                plobs = malloc(ploc * sizeof(int));

                for (e = 0; e < das->nmem; ++e) {
                    ENSOBSTYPE* Se = das->S[e];
                    double* Sloce = Sloc[e];

                    for (o = 0; o < ploc; ++o)
                        Sloce[o] = (double) Se[lobs[o]] * lcoeffs[o];
                }
                for (o = 0; o < ploc; ++o)
                    sloc[o] = das->s_f[lobs[o]] * lcoeffs[o];

                if (das->mode == MODE_ENOI || das->scheme == SCHEME_DENKF)
                    calc_G_denkf(das->nmem, ploc, Sloc, i, j, G);
                else if (das->scheme == SCHEME_ETKF)
                    /*
                     * (X5 is used for storing T)
                     */
                    calc_G_etkf(das->nmem, ploc, Sloc, i, j, G, X5);

                if (das->mode == MODE_ENKF) {
                    if (das->scheme == SCHEME_DENKF)
                        calc_X5_denkf(das->nmem, ploc, G, Sloc, sloc, i, j, X5);
                    else if (das->scheme == SCHEME_ETKF)
                        calc_X5_etkf(das->nmem, ploc, G, sloc, i, j, X5);
                    else
                        enkf_quit("programming error");
                    /*
                     * convert X5 to float and store in X5j 
                     */
                    for (e = 0; e < das->nmem * das->nmem; ++e)
                        X5j[ii][e] = (float) X5[0][e];
                } else if (das->mode == MODE_ENOI) {
                    calc_w(das->nmem, ploc, G, sloc, w);

                    for (e = 0; e < das->nmem; ++e)
                        wj[ii][e] = (float) w[e];
                }

                nlobs[jjj][ii] = ploc;  /* (ploc > 0 here) */
                dfs[jjj][ii] = traceprod(0, 0, ploc, das->nmem, G, Sloc);
                srf[jjj][ii] = sqrt(traceprod(0, 1, das->nmem, ploc, Sloc, Sloc) / dfs[jjj][ii]) - 1.0;
                for (ot = 0; ot < obs->nobstypes; ++ot) {
                    int p = 0;

                    for (o = 0; o < ploc; ++o) {
                        if (obs->data[lobs[o]].type == ot) {
                            plobs[p] = o;
                            p++;
                        }
                    }
                    pnlobs[ot][jjj][ii] = p;
                    if (p == 0) {
                        pdfs[ot][jjj][ii] = 0.0;
                        psrf[ot][jjj][ii] = 0.0;
                    } else {
                        double** pS;
                        double** pG;

                        pG = malloc(p * sizeof(double*));
                        for (o = 0; o < p; ++o)
                            pG[o] = G[plobs[o]];
                        pS = alloc2d(das->nmem, p, sizeof(double));
                        for (e = 0; e < das->nmem; ++e)
                            for (o = 0; o < p; ++o)
                                pS[e][o] = Sloc[e][plobs[o]];
                        pdfs[ot][jjj][ii] = traceprod(0, 0, p, das->nmem, pG, pS);
                        if (pdfs[ot][jjj][ii] > 0.0)
                            psrf[ot][jjj][ii] = sqrt(traceprod(0, 1, das->nmem, p, pS, pS) / pdfs[ot][jjj][ii]) - 1.0;
                        else
                            psrf[ot][jjj][ii] = 0.0;

                        free(pG);
                        free2d(pS);
                    }
                }

                if (ploc > das->nmem)
                    stats.n_inv_ens++;
                else
                    stats.n_inv_obs++;

                free2d(G);
                free(sloc);
                free2d(Sloc);
                free(lobs);
                free(plobs);
                free(lcoeffs);
            }                   /* for i */

#if defined(MPI)
            if (das->mode == MODE_ENKF) {
                if (rank > 0) {
                    int ierror = MPI_Send(X5j[0], ni * das->nmem * das->nmem, MPI_FLOAT, 0, jj, MPI_COMM_WORLD);

                    assert(ierror == MPI_SUCCESS);
                } else {
                    int r, ierror;

                    /*
                     * write own results 
                     */
                    nc_writeX5(fname_XW, ncid_X5, jpool[jj], ni, das->nmem, varid_X5, X5j[0]);
                    /*
                     * collect and write results from slaves 
                     */
                    for (r = 1; r < nprocesses; ++r) {
                        if (jj > last_iteration[r] - first_iteration[r])
                            continue;
                        /*
                         * (recall that my_number_of_iterations <=
                         * number_of_iterations[0]) 
                         */
                        ierror = MPI_Recv(X5j[0], ni * das->nmem * das->nmem, MPI_FLOAT, r, first_iteration[r] + jj, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        assert(ierror == MPI_SUCCESS);
                        nc_writeX5(fname_XW, ncid_X5, jpool[first_iteration[r] + jj], ni, das->nmem, varid_X5, X5j[0]);
                    }
                }
            } else if (das->mode == MODE_ENOI) {
                if (rank > 0) {
                    int ierror = MPI_Send(wj[0], ni * das->nmem, MPI_FLOAT, 0, jj, MPI_COMM_WORLD);

                    assert(ierror == MPI_SUCCESS);
                } else {
                    int r, ierror;

                    /*
                     * write own results 
                     */
                    nc_writew(fname_XW, ncid_w, jpool[jj], ni, das->nmem, varid_w, wj[0]);
                    /*
                     * collect and write results from slaves 
                     */
                    for (r = 1; r < nprocesses; ++r) {
                        if (jj > last_iteration[r] - first_iteration[r])
                            continue;
                        /*
                         * (recall that my_number_of_iterations <=
                         * number_of_iterations[0]) 
                         */
                        ierror = MPI_Recv(wj[0], ni * das->nmem, MPI_FLOAT, r, first_iteration[r] + jj, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                        assert(ierror == MPI_SUCCESS);
                        nc_writew(fname_XW, ncid_w, jpool[first_iteration[r] + jj], ni, das->nmem, varid_w, wj[0]);
                    }
                }
            }
#else
            if (das->mode == MODE_ENKF)
                nc_writeX5(fname_XW, ncid_X5, jpool[jj], ni, das->nmem, varid_X5, X5j[0]);
            else if (das->mode == MODE_ENKF)
                nc_writew(fname_XW, ncid_w, jpool[jj], ni, das->nmem, varid_w, wj[0]);
#endif                          /* MPI */
        }                       /* for jj */

        if (rank == 0) {
            if (das->mode == MODE_ENKF)
                ncw_close(fname_XW, ncid_X5);
            else if (das->mode == MODE_ENOI)
                ncw_close(fname_XW, ncid_w);
        }
#if defined(MPI)
        MPI_Barrier(MPI_COMM_WORLD);
#endif
        enkf_printf("    finished calculating transforms for %s\n", grid_getname(grid));

        enkf_flush();

#if defined(MPI)
        /*
         * collect stats on master 
         */
        if (rank > 0) {
            int ierror;

            ierror = MPI_Send(nlobs[0], (my_last_iteration - my_first_iteration + 1) * ni, MPI_INT, 0, 1, MPI_COMM_WORLD);
            assert(ierror == MPI_SUCCESS);
            ierror = MPI_Send(dfs[0], (my_last_iteration - my_first_iteration + 1) * ni, MPI_FLOAT, 0, 2, MPI_COMM_WORLD);
            assert(ierror == MPI_SUCCESS);
            ierror = MPI_Send(srf[0], (my_last_iteration - my_first_iteration + 1) * ni, MPI_FLOAT, 0, 3, MPI_COMM_WORLD);
            assert(ierror == MPI_SUCCESS);
            ierror = MPI_Send(pnlobs[0][0], obs->nobstypes * (my_last_iteration - my_first_iteration + 1) * ni, MPI_INT, 0, 4, MPI_COMM_WORLD);
            assert(ierror == MPI_SUCCESS);
            ierror = MPI_Send(pdfs[0][0], obs->nobstypes * (my_last_iteration - my_first_iteration + 1) * ni, MPI_FLOAT, 0, 5, MPI_COMM_WORLD);
            assert(ierror == MPI_SUCCESS);
            ierror = MPI_Send(psrf[0][0], obs->nobstypes * (my_last_iteration - my_first_iteration + 1) * ni, MPI_FLOAT, 0, 6, MPI_COMM_WORLD);
            assert(ierror == MPI_SUCCESS);
        } else {
            int ierror;
            int** buffer_nlobs;
            float** buffer_dfs; /* also used for srf */
            int*** buffer_pnlobs;
            float*** buffer_pdfs;       /* also used for psrf */
            int r;

            for (r = 1; r < nprocesses; ++r) {
                buffer_nlobs = alloc2d(last_iteration[r] - first_iteration[r] + 1, ni, sizeof(int));
                ierror = MPI_Recv(buffer_nlobs[0], (last_iteration[r] - first_iteration[r] + 1) * ni, MPI_INT, r, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                assert(ierror == MPI_SUCCESS);
                for (jj = first_iteration[r], jjj = 0; jj <= last_iteration[r]; ++jj, ++jjj) {
                    j = jpool[jj];
                    memcpy(nlobs[j], buffer_nlobs[jjj], ni * sizeof(int));
                }
                free2d(buffer_nlobs);

                buffer_dfs = alloc2d(last_iteration[r] - first_iteration[r] + 1, ni, sizeof(int));
                ierror = MPI_Recv(buffer_dfs[0], (last_iteration[r] - first_iteration[r] + 1) * ni, MPI_FLOAT, r, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                assert(ierror == MPI_SUCCESS);
                for (jj = first_iteration[r], jjj = 0; jj <= last_iteration[r]; ++jj, ++jjj) {
                    j = jpool[jj];
                    memcpy(dfs[j], buffer_dfs[jjj], ni * sizeof(float));
                }

                ierror = MPI_Recv(buffer_dfs[0], (last_iteration[r] - first_iteration[r] + 1) * ni, MPI_FLOAT, r, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                assert(ierror == MPI_SUCCESS);
                for (jj = first_iteration[r], jjj = 0; jj <= last_iteration[r]; ++jj, ++jjj) {
                    j = jpool[jj];
                    memcpy(srf[j], buffer_dfs[jjj], ni * sizeof(float));
                }
                free2d(buffer_dfs);

                buffer_pnlobs = alloc3d(obs->nobstypes, last_iteration[r] - first_iteration[r] + 1, ni, sizeof(int));
                ierror = MPI_Recv(buffer_pnlobs[0][0], obs->nobstypes * (last_iteration[r] - first_iteration[r] + 1) * ni, MPI_INT, r, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                assert(ierror == MPI_SUCCESS);
                for (ot = 0; ot < obs->nobstypes; ot++) {
                    for (jj = first_iteration[r], jjj = 0; jj <= last_iteration[r]; ++jj, ++jjj) {
                        j = jpool[jj];
                        memcpy(pnlobs[ot][j], buffer_pnlobs[ot][jjj], ni * sizeof(int));
                    }
                }
                free3d(buffer_pnlobs);

                buffer_pdfs = alloc3d(obs->nobstypes, last_iteration[r] - first_iteration[r] + 1, ni, sizeof(float));
                ierror = MPI_Recv(buffer_pdfs[0][0], obs->nobstypes * (last_iteration[r] - first_iteration[r] + 1) * ni, MPI_FLOAT, r, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                assert(ierror == MPI_SUCCESS);
                for (ot = 0; ot < obs->nobstypes; ot++) {
                    for (jj = first_iteration[r], jjj = 0; jj <= last_iteration[r]; ++jj, ++jjj) {
                        j = jpool[jj];
                        memcpy(pdfs[ot][j], buffer_pdfs[ot][jjj], ni * sizeof(float));
                    }
                }

                ierror = MPI_Recv(buffer_pdfs[0][0], obs->nobstypes * (last_iteration[r] - first_iteration[r] + 1) * ni, MPI_FLOAT, r, 6, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                assert(ierror == MPI_SUCCESS);
                for (ot = 0; ot < obs->nobstypes; ot++) {
                    for (jj = first_iteration[r], jjj = 0; jj <= last_iteration[r]; ++jj, ++jjj) {
                        j = jpool[jj];
                        memcpy(psrf[ot][j], buffer_pdfs[ot][jjj], ni * sizeof(float));
                    }
                }
                free3d(buffer_pdfs);
            }
        }                       /* rank == 0 */
#endif                          /* MPI */
        if (rank == 0) {
            char fname_stats[MAXSTRLEN];

            das_getfname_stats(das, grid, fname_stats);

            nc_writediag(fname_stats, obs->nobstypes, nj, ni, das->stride, nlobs, dfs, srf, pnlobs, pdfs, psrf);
        }

        if (das->mode == MODE_ENKF) {
            free2d(X5j);
            free2d(X5);
        } else if (das->mode == MODE_ENOI) {
            free2d(wj);
            free(w);
        }
#if defined(MPI)
        /*
         * merge the stats for the log report
         */
        if (rank != 0) {
            int ierror = MPI_Send(&stats, sizeof(stats) / sizeof(int), MPI_INT, 0, 99, MPI_COMM_WORLD);

            assert(ierror == MPI_SUCCESS);
        } else {
            int r;

            for (r = 1; r < nprocesses; ++r) {
                calcstats morestats;
                int ierror = MPI_Recv(&morestats, sizeof(stats) / sizeof(int), MPI_INT, r, 99, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                assert(ierror == MPI_SUCCESS);

                stats.nlobs_sum += morestats.nlobs_sum;
                if (morestats.nlobs_max > stats.nlobs_max)
                    stats.nlobs_max = morestats.nlobs_max;
                stats.n_inv_obs += morestats.n_inv_obs;
                stats.n_inv_ens += morestats.n_inv_ens;
                stats.ncell += morestats.ncell;
            }
        }
#endif

        enkf_printf("    summary stats on %s:\n", grid_getname(grid));

        enkf_printf("      # of local analyses = %d\n", stats.ncell);
        enkf_printf("      average # of local obs = %.1f\n", (double) stats.nlobs_sum / (double) stats.ncell);
        enkf_printf("      # of inversions in obs space = %d\n", stats.n_inv_obs);
        enkf_printf("      # of inversions in ens space = %d\n", stats.n_inv_ens);
        enkf_printf("");

        free2d(nlobs);
        free2d(dfs);
        free2d(srf);
        free3d(pnlobs);
        free3d(pdfs);
        free3d(psrf);
        free(jiter);
        free(iiter);
        free(jpool);
    }                           /* for gid */
}

/** Calculates transform for logpoints. This is done separately from
 * das_calctransfroms() to avoid interpolation related problems.
 */
void das_dopointlogs(dasystem* das)
{
    model* m = das->m;
    observations* obs = das->obs;
    void* grid = model_getgridbyid(m, 0);
    double** X5 = NULL;
    double* w = NULL;
    int p, e, o;

    assert(das->s_mode == S_MODE_S_f);

    if (das->mode == MODE_ENKF) {
        X5 = alloc2d(das->nmem, das->nmem, sizeof(double));
    } else if (das->mode == MODE_ENOI)
        w = malloc(das->nmem * sizeof(double));

    for (p = 0; p < das->nplogs; ++p) {
        int i = das->plogs[p].i;
        int j = das->plogs[p].j;
        double lon = NaN;
        double lat = NaN;
        int* lobs = NULL;
        double* lcoeffs = NULL;
        int ploc = 0;
        double** Sloc = NULL;
        double* sloc = NULL;
        double** G = NULL;
        double** T = NULL;

        printf("    calculating transform for log point (%d, %d):", i, j);
        obs_findlocal(obs, m, grid, i, j, &ploc, &lobs, &lcoeffs);

        assert(ploc >= 0 && ploc <= obs->nobs);

        if (X5 != NULL)
            memset(X5[0], 0, das->nmem * das->nmem * sizeof(double));
        if (w != NULL)
            memset(w, 0, das->nmem * sizeof(double));

        if (ploc == 0) {
            if (das->mode == MODE_ENKF)
                for (e = 0; e < das->nmem; ++e)
                    X5[e][e] = (float) 1.0;
        } else {
            Sloc = alloc2d(das->nmem, ploc, sizeof(double));
            sloc = malloc(ploc * sizeof(double));
            G = alloc2d(ploc, das->nmem, sizeof(double));

            if (das->mode == MODE_ENKF && das->scheme == SCHEME_ETKF)
                T = alloc2d(das->nmem, das->nmem, sizeof(double));

            for (e = 0; e < das->nmem; ++e) {
                ENSOBSTYPE* Se = das->S[e];
                double* Sloce = Sloc[e];

                for (o = 0; o < ploc; ++o)
                    Sloce[o] = (double) Se[lobs[o]] * lcoeffs[o];
            }
            for (o = 0; o < ploc; ++o)
                sloc[o] = das->s_f[lobs[o]] * lcoeffs[o];

            if (das->mode == MODE_ENOI || das->scheme == SCHEME_DENKF)
                calc_G_denkf(das->nmem, ploc, Sloc, i, j, G);
            else if (das->scheme == SCHEME_ETKF)
                calc_G_etkf(das->nmem, ploc, Sloc, i, j, G, T);
            else
                enkf_quit("programming error");
            if (das->mode == MODE_ENKF) {
                if (das->scheme == SCHEME_DENKF)
                    calc_X5_denkf(das->nmem, ploc, G, Sloc, sloc, i, j, X5);
                else if (das->scheme == SCHEME_ETKF)
                    calc_X5_etkf(das->nmem, ploc, G, sloc, i, j, X5);
                else
                    enkf_quit("programming error");
            } else if (das->mode == MODE_ENOI)
                calc_w(das->nmem, ploc, G, sloc, w);
        }
        printf(" %d obs\n", ploc);

        printf("    writing the log for point (%d, %d):", i, j);
        grid_fij2xy(grid, (double) i, (double) j, &lon, &lat);
        plog_write(das, i, j, lon, lat, (grid_getdepth(grid))[j][i], ploc, lobs, lcoeffs, sloc, (ploc == 0) ? NULL : Sloc[0], (das->mode == MODE_ENKF) ? X5[0] : w);

        printf("\n");

        if (ploc > 0) {
            free2d(G);
            free(sloc);
            free2d(Sloc);
            free(lobs);
            free(lcoeffs);
            if (T != NULL)
                free2d(T);
        }
    }

    if (das->mode == MODE_ENKF)
        free2d(X5);
    else if (das->mode == MODE_ENOI)
        free(w);
}
