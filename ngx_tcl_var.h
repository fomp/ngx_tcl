#ifndef NGX_TCL_VARIABLE_H
#define NGX_TCL_VARIABLE_H

int              ngx_http_tcl_getv_cmd(ClientData clientData,
                        Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
int              ngx_http_tcl_setv_cmd(ClientData clientData,
                        Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

#endif
