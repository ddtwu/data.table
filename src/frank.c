#include "data.table.h"
#include <Rdefines.h>
// #include <signal.h> // the debugging machinery + breakpoint aidee
// raise(SIGINT);

extern SEXP char_integer64;

static union {
    double d;
    unsigned long long ull;
} u;

SEXP dt_na(SEXP x, SEXP cols) {
    int i, j, n=0, this;
    double *dv;
    SEXP v, ans, class;
    
    if (!isNewList(x)) error("Internal error. Argument 'x' to Cdt_na is type '%s' not 'list'", type2char(TYPEOF(x)));
    if (!isInteger(cols)) error("Internal error. Argument 'cols' to Cdt_na is type '%s' not 'integer'", type2char(TYPEOF(cols)));
    for (i=0; i<LENGTH(cols); i++) {
        this = INTEGER(cols)[i];
        if (this<1 || this>LENGTH(x)) 
            error("Item %d of 'cols' is %d which is outside 1-based range [1,ncol(x)=%d]", i+1, this, LENGTH(x));
        if (!n) n = length(VECTOR_ELT(x, this-1));
    }
    ans = PROTECT(allocVector(LGLSXP, n));
    for (i=0; i<n; i++) LOGICAL(ans)[i]=0;
    for (i=0; i<LENGTH(cols); i++) {
        v = VECTOR_ELT(x, INTEGER(cols)[i]-1);
        if (!length(v) || isNewList(v) || isList(v)) continue; // like stats:::na.omit.data.frame, skip list/pairlist columns
        if (n != length(v))
            error("Column %d of input list x is length %d, inconsistent with first column of that item which is length %d.", i+1,length(v),n);
        switch (TYPEOF(v)) {
        case LGLSXP:
            for (j=0; j<n; j++) LOGICAL(ans)[j] |= (LOGICAL(v)[j] == NA_LOGICAL);
            break;
        case INTSXP:
            for (j=0; j<n; j++) LOGICAL(ans)[j] |= (INTEGER(v)[j] == NA_INTEGER);
            break;
        case STRSXP:
            for (j=0; j<n; j++) LOGICAL(ans)[j] |= (STRING_ELT(v, j) == NA_STRING);
            break;
        case REALSXP:
            class = getAttrib(v, R_ClassSymbol);        
            if (isString(class) && STRING_ELT(class, 0) == char_integer64) {
                dv = (double *)REAL(v);
                for (j=0; j<n; j++) {
                    u.d = dv[j];
                    LOGICAL(ans)[j] |= ((u.ull ^ 0x8000000000000000) == 0);
                }
            } else {
                for (j=0; j<n; j++) LOGICAL(ans)[j] |= ISNAN(REAL(v)[j]);
            }
            break;
        case RAWSXP: 
            // no such thing as a raw NA
            // vector already initialised to all 0's
            break;
        case CPLXSXP:
            // taken from https://github.com/wch/r-source/blob/d75f39d532819ccc8251f93b8ab10d5b83aac89a/src/main/coerce.c
            for (j=0; j<n; j++) LOGICAL(ans)[j] |= (ISNAN(COMPLEX(v)[j].r) || ISNAN(COMPLEX(v)[j].i));
            break;
        default:
            error("Unknown column type '%s'", type2char(TYPEOF(v)));
        }
    }
    UNPROTECT(1);
    return(ans);
}

SEXP frank(SEXP xorderArg, SEXP xstartArg, SEXP xlenArg, SEXP ties_method) {
    int i=0, j=0, k=0, n;
    int *xstart = INTEGER(xstartArg), *xlen = INTEGER(xlenArg), *xorder = INTEGER(xorderArg);
    enum {MEAN, MAX, MIN, DENSE, RUNLENGTH} ties = MEAN;
    SEXP ans;

    if (!strcmp(CHAR(STRING_ELT(ties_method, 0)), "average"))  ties = MEAN;
    else if (!strcmp(CHAR(STRING_ELT(ties_method, 0)), "max")) ties = MAX;
    else if (!strcmp(CHAR(STRING_ELT(ties_method, 0)), "min")) ties = MIN;
    else if (!strcmp(CHAR(STRING_ELT(ties_method, 0)), "dense")) ties = DENSE;
    else if (!strcmp(CHAR(STRING_ELT(ties_method, 0)), "runlength")) ties = RUNLENGTH;
    else error("Internal error: invalid ties.method for frankv(), should have been caught before. Please report to datatable-help");
    n = length(xorderArg);
    ans = (ties == MEAN) ? PROTECT(allocVector(REALSXP, n)) : PROTECT(allocVector(INTSXP, n));
    if (n > 0) {
        switch (ties) {
            case MEAN : 
            for (i = 0; i < length(xstartArg); i++) {
                for (j = xstart[i]-1; j < xstart[i]+xlen[i]-1; j++)
                    REAL(ans)[xorder[j]-1] = (2*xstart[i]+xlen[i]-1)/2.0;
            }
            break;
            case MAX :
            for (i = 0; i < length(xstartArg); i++) {
                for (j = xstart[i]-1; j < xstart[i]+xlen[i]-1; j++)
                    INTEGER(ans)[xorder[j]-1] = xstart[i]+xlen[i]-1;
            }
            break;
            case MIN :
            for (i = 0; i < length(xstartArg); i++) {
                for (j = xstart[i]-1; j < xstart[i]+xlen[i]-1; j++)
                    INTEGER(ans)[xorder[j]-1] = xstart[i];
            }
            break;
            case DENSE :
            k=1;
            for (i = 0; i < length(xstartArg); i++) {
                for (j = xstart[i]-1; j < xstart[i]+xlen[i]-1; j++)
                    INTEGER(ans)[xorder[j]-1] = k;
                k++;
            }
            break;
            case RUNLENGTH :
            for (i = 0; i < length(xstartArg); i++) {
                k=1;
                for (j = xstart[i]-1; j < xstart[i]+xlen[i]-1; j++)
                    INTEGER(ans)[xorder[j]-1] = k++;
            }
            break;
        }
    }
    UNPROTECT(1);
    return(ans);
}
