/**
 * vim:set sts=4 sw=4 et sta:
 */

#include <tcl.h>
#include <ngx_http.h>
#include <ngx_core.h>

#include "ngx_tcl.h"
#include "ngx_tcl_header.h"

#define getrequest(C) ((ngx_tcl_interp_conf_t*)(C))->request

typedef int(ngx_http_tcl_header_setter)(ngx_http_request_t *r,
            Tcl_Interp *interp, Tcl_Obj *key, Tcl_Obj *val);

static int               SetHeaderFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static int               set_unknown_header(ngx_http_request_t *r,
                                Tcl_Interp *interp, Tcl_Obj *key, Tcl_Obj *val);
static int               set_content_type(ngx_http_request_t *r,
                                Tcl_Interp *interp, Tcl_Obj *key, Tcl_Obj *val);
static int               set_content_length(ngx_http_request_t *r,
                                Tcl_Interp *interp, Tcl_Obj *key, Tcl_Obj *val);

static Tcl_ObjType headerObjType = {
    "ngxheader",
    NULL,               /* freeIntRepProc */
    NULL,               /* dupIntRepProc */
    NULL,               /* updateStringProc */
    SetHeaderFromAny    /* setFromAnyProc */
};

typedef struct {
    ngx_str_t name;
    ngx_http_tcl_header_setter *setter;
} ngx_http_tcl_header_t;

static ngx_http_tcl_header_t headers[] = {
    { ngx_string("content-type"), set_content_type },
    { ngx_string("content-length"), set_content_length },
    { ngx_null_string, NULL }
};

static ngx_http_tcl_header_t unknown_header = {
    ngx_string("unknown-header"), set_unknown_header
};

static int
SetHeaderFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    u_char *s;
    int len;
    ngx_http_tcl_header_t *hdr;

    if (objPtr->typePtr == &headerObjType) {
        return TCL_OK;
    }

    s = (u_char*)Tcl_GetStringFromObj(objPtr, &len);
    hdr = headers;

    for (hdr = headers; /* void */; ++hdr) {
        if (hdr->name.len == 0) {
            hdr = &unknown_header;
            break;
        }

        if (hdr->name.len == (u_int)len
                && ngx_strcasecmp(s, hdr->name.data) == 0) {
            break;
        }
    }

    if (objPtr->typePtr != NULL && objPtr->typePtr->freeIntRepProc != NULL) {
        objPtr->typePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.otherValuePtr = hdr;

    objPtr->typePtr = &headerObjType;

    return TCL_OK;
}

int
ngx_http_tcl_outheader_cmd(ClientData clientData, Tcl_Interp *interp, 
	int objc, Tcl_Obj *const objv[])
{
    int i, rc;
    ngx_http_request_t *r = getrequest(clientData);
    ngx_http_tcl_header_t *hdr;
    
    if (objc % 2 == 0) {
        Tcl_WrongNumArgs(interp, 1, objv, "[key value ...]");
        return TCL_ERROR;
    }

    for (i = 1; i < objc; i += 2) {
        rc = SetHeaderFromAny(interp, objv[i]);
        if (rc != TCL_OK) {
            return rc;
        }

        hdr = (ngx_http_tcl_header_t*)objv[i]->internalRep.otherValuePtr;

        rc = hdr->setter(r, interp, objv[i], objv[i+1]);
        if (rc != TCL_OK) {
            return rc;
        }
    }

    return TCL_OK;
}

static int
set_unknown_header(ngx_http_request_t *r, Tcl_Interp *interp,
        Tcl_Obj *key, Tcl_Obj *val)
{
    ngx_list_t *headers;
    ngx_table_elt_t *elt;
    int len;

    headers = &r->headers_out.headers;

    elt = ngx_list_push(headers);

    if (elt == NULL) {
        return TCL_ERROR;
    }

    elt->hash = 1;
    elt->key.data = (u_char*)Tcl_GetStringFromObj(key, &len);
    elt->key.len = len;
    ngx_tcl_cleanup_add_Tcl_Obj(headers->pool, key);

    elt->value.data = (u_char*)Tcl_GetStringFromObj(val, &len);
    elt->value.len = len;
    ngx_tcl_cleanup_add_Tcl_Obj(headers->pool, val);

    return TCL_OK;
}

static int
set_content_type(ngx_http_request_t *r, Tcl_Interp *interp,
        Tcl_Obj *key, Tcl_Obj *val)
{
    int len;

    r->headers_out.content_type.data = 
            (u_char*)Tcl_GetStringFromObj(val, &len);
    r->headers_out.content_type_len = len;
    r->headers_out.content_type.len = len;

    ngx_tcl_cleanup_add_Tcl_Obj(r->pool, val);

    return TCL_OK;
}

static int
set_content_length(ngx_http_request_t *r, Tcl_Interp *interp,
        Tcl_Obj *key, Tcl_Obj *val)
{
    long contlen;
    int rc;

    rc  = Tcl_GetLongFromObj(interp, val, &contlen);
    if (rc != TCL_OK) {
        return rc;
    }

    r->headers_out.content_length_n = (off_t)contlen;

    return TCL_OK;
}
