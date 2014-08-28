/*
 * vim:set sts=4 sw=4 et sta:
 */

#include <tcl.h>
#include <ngx_http.h>

#include "ngx_tcl.h"
#include "ngx_tcl_var.h"

extern ngx_module_t ngx_tcl_module;

#define getrequest(C) ((ngx_tcl_interp_conf_t*)(C))->request
#define vartype_get_hash(O) ((O)->internalRep.ptrAndLongRep.value)
#define vartype_get_name(O) ((O)->internalRep.ptrAndLongRep.ptr)
#define vartype_get_namelen(O) Tcl_GetCharLength(O)


static int		 SetVarFromAny(Tcl_Interp *interp, Tcl_Obj *obj);
static void              FreeVar(Tcl_Obj*);

static Tcl_ObjType ngxVarType = {
    "ngxvar",
    FreeVar,		/* freeIntRepProc */
    NULL,		/* dupIntRepProc */
    NULL,		/* updateStringProc */
    SetVarFromAny       /* setFromAnyProc */
};

static void
FreeVar(Tcl_Obj *obj)
{
    ckfree(obj->internalRep.ptrAndLongRep.ptr);
}


static int
SetVarFromAny(Tcl_Interp *interp, Tcl_Obj *obj)
{
    int varlen;
    ngx_uint_t hash;
    char *lowcase;
    char *varname;

    if (obj->typePtr != &ngxVarType) {
        varname = Tcl_GetStringFromObj(obj, &varlen);
        lowcase = ckalloc(varlen);

        if (lowcase == NULL) {
            return TCL_ERROR;
        }

        hash = ngx_hash_strlow((u_char*)lowcase, (u_char*)varname, varlen);

        if (obj->typePtr != NULL && obj->typePtr->freeIntRepProc != NULL) {
            obj->typePtr->freeIntRepProc(obj);
        }

        obj->internalRep.ptrAndLongRep.ptr = lowcase;
        obj->internalRep.ptrAndLongRep.value = hash;

        obj->typePtr = &ngxVarType;
    }

    return TCL_OK;
}

int
ngx_http_tcl_getv_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const objv[])
{
    ngx_http_request_t *r = getrequest(clientData);
    ngx_http_variable_value_t *vv;
    Tcl_Obj *varObj;
    ngx_str_t varname;
    int rc;
    int len;

    if (objc < 2 || objc > 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "varname ?default?");
        return TCL_ERROR;
    }

    varObj = objv[1];

    /* TODO: check return */
    rc = SetVarFromAny(interp, varObj);
    if (rc != TCL_OK) {
        return rc;
    }

    Tcl_GetStringFromObj(varObj, &len);

    varname.len = len;
    varname.data = (u_char*)varObj->internalRep.ptrAndLongRep.ptr;
    
    vv = ngx_http_get_variable(r, &varname, vartype_get_hash(varObj));

    if (vv->not_found) {
        if (objc == 3) {
            Tcl_SetObjResult(interp, objv[2]);
            return TCL_OK;
        }

        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, "variable \"", Tcl_GetString(varObj),
                "\" doesn't exist", NULL);
        return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, Tcl_NewStringObj((char*)vv->data, vv->len));

    return TCL_OK;
}

int
ngx_http_tcl_setv_cmd(ClientData clientData, Tcl_Interp *interp, int objc,
        Tcl_Obj *const objv[])
{
    Tcl_Obj *varObj;
    Tcl_Obj *valObj;
    ngx_http_request_t *r = getrequest(clientData);
    ngx_http_core_main_conf_t *cmcf;
    ngx_http_variable_t *v;
    ngx_http_variable_value_t *vv;
    int len;
    int rc;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "key value");
        return TCL_ERROR;
    }

    varObj = objv[1];
    valObj = objv[2];

    rc = SetVarFromAny(interp, varObj);
    if (rc != TCL_OK) {
        return rc;
    }

    cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

    v = ngx_hash_find(&cmcf->variables_hash, vartype_get_hash(varObj),
            vartype_get_name(varObj), vartype_get_namelen(varObj));

    if (v == NULL) {
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, "variable \"", Tcl_GetString(varObj),
                "\" not found", NULL);
        return TCL_ERROR;
    }

    if (v->flags & NGX_HTTP_VAR_CHANGEABLE) {
        if (v->set_handler != NULL) {
            vv = ngx_palloc(r->pool, sizeof(ngx_http_variable_value_t));

            if (vv == NULL) {
                return TCL_ERROR;
            }

            vv->valid = 1;
            vv->not_found = 0;
            vv->no_cacheable = 0;
            vv->data = (u_char*)Tcl_GetStringFromObj(valObj, &len);
            vv->len = len;

            v->set_handler(r, vv, v->data);

            return TCL_OK;
        }

        if (v->flags & NGX_HTTP_VAR_INDEXED) {
            vv = &r->variables[v->index];

            vv->valid = 1;
            vv->not_found = 0;
            vv->no_cacheable = 0;
            vv->data = (u_char*)Tcl_GetStringFromObj(valObj, &len);
            vv->len = len;

            return TCL_OK;
        }
    }

    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "variable \"", Tcl_GetString(varObj),
            "\" is not changeable", NULL);

    return TCL_ERROR;
}
