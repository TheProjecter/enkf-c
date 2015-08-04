/******************************************************************************
 *
 * File:        utils.h        
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

#if !defined(_UTILS_H)

#include <unistd.h>

void enkf_init(int* argc, char*** argv);
void enkf_finish(void);
void enkf_quit(char* format, ...);
int enkf_printf(const char* format, ...);
void enkf_flush(void);
FILE* enkf_fopen(const char fname[], const char* mode);
void enkf_printtime(const char offset[]);
void enkf_printcompileflags(const char offset[]);
void enkf_printflags(const char offset[]);
void enkf_printversion(void);
void find_files(char template[], int* nfiles, char*** fnames);
void tunits_convert(char tunits[], double* tunits_multiple, double* tunits_offset);
void file_delete(char* fname);
int file_exists(char fname[]);
int str2double(char token[], double* value);
int str2int(char* token, int* value);
int read_bool(char* token);
void* alloc2d(int n2, int n1, size_t unitsize);
void free2d(void* pp);
void* alloc3d(int n3, int n2, int n1, size_t unitsize);
void free3d(void* ppp);
int nc_isunlimdimid(int ncid, int dimid);
int getnlevels(char fname[], char varname[]);
void readfield(char fname[], char varname[], int k, float* v);
void writefield(char fname[], char varname[], int k, float* v);
void writerow(char fname[], char varname[], int k, int j, float* v);
void read3dfield(char fname[], char varname[], float* v);
int getnumlevels(char fname[], char varname[]);
double date_str2dbl(char strdate[]);
int get_tshift(double date, double tstep);
void print_matrix_double(int n, int m, double** A, char offset[]);
void print_matrix_float(int n, int m, float** A, char offset[]);
void print_vector_double(int n, double* a, char offset[]);
void print_vector_float(int n, float* a, char offset[]);
ENSOBSTYPE interpolate2d(double fi, double fj, int ni, int nj, float** v, int** mask, int periodic_x, int periodic_y);
ENSOBSTYPE interpolate3d(double fi, double fj, double fk, int ni, int nj, int nk, float*** v, int** nlevels, int periodic_x, int periodic_y);
double taper_gc(double x);
void ll2xyz(double in[2], double out[3]);
void print_commandinfo(int argc, char* argv[]);

#if defined(INTERNAL_QSORT_R)
typedef int (*__compar_d_fn_t) (const void*, const void*, void*);
static inline void qsort_r(void* base, size_t n, size_t width, __compar_d_fn_t cmp, void* p)
{
    int nested_cmp(const void* a, const void* b) {
        return cmp(a, b, p);
    }
    qsort(base, n, width, nested_cmp);
}
#endif

#define _UTILS_H
#endif
