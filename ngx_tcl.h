#ifndef NGX_TCL_h
#define NGX_TCL_h

#include <tcl.h>
#include <ngx_core.h>

struct ngx_tcl_interp_conf_s {
    Tcl_Interp *interp;
    ngx_str_t name;
    ngx_str_t initscript;
    ngx_http_request_t *request;
    struct ngx_tcl_interp_conf_s *next;
};

typedef struct ngx_tcl_interp_conf_s ngx_tcl_interp_conf_t;

int      ngx_tcl_cleanup_add_Tcl_Obj(ngx_pool_t *p, Tcl_Obj *obj);

#endif
