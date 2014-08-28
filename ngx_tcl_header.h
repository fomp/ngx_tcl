#ifndef NGX_TCL_HEADER_H
#define NGX_TCL_HEADER_H

int              ngx_http_tcl_outheader_cmd(ClientData clientData,
                        Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

#endif
