/**
#	vim:set sts=4 sw=4 et sta:
*/
 
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <tcl.h>

struct ngx_tcl_interp_s {
    Tcl_Interp *interp;
    ngx_str_t name;
    ngx_str_t file;
    struct ngx_tcl_interp_s *next;
};

typedef struct ngx_tcl_interp_s ngx_tcl_interp_t;

static ngx_tcl_interp_t *interps = NULL;

struct ngx_tcl_loc_conf_s {
    Tcl_Interp *interp;
    Tcl_Obj *handler;
};

typedef struct ngx_tcl_loc_conf_s ngx_tcl_loc_conf_t;


static char	*ngx_tcl_create_tcl_cmd(ngx_conf_t *cf, ngx_command_t * cmd,
		    void * conf);
static char	*ngx_tcl_tcl_handler_cmd(ngx_conf_t *cf, ngx_command_t * cmd,
		    void * conf);
static void	*ngx_tcl_create_loc_conf(ngx_conf_t *cf);
static char	*ngx_tcl_merge_loc_conf(ngx_conf_t *cf, void *parent,
		    void *child);
static ngx_int_t ngx_tcl_handler(ngx_http_request_t *r);
static int	 tclRequestCmd(ClientData clientData, Tcl_Interp *interp,
		    int objc, Tcl_Obj *const objv[]);
static ngx_int_t ngx_tcl_init(ngx_cycle_t *cycle);

static ngx_http_module_t ngx_tcl_ctx = {
    NULL,	// preconfiguration
    NULL,	// postconfiguration

    NULL,	// create main configuration
    NULL,	// init main configuration

    NULL,	// create server configuration
    NULL,	// merge server configuration

    ngx_tcl_create_loc_conf,	// create location configuration
    ngx_tcl_merge_loc_conf	// merge location configuration
};

static ngx_command_t ngx_tcl_commands[] = {

    {   ngx_string("create_tcl"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE2,
        ngx_tcl_create_tcl_cmd,
        0,
        0,
        NULL },

    {   ngx_string("tcl_handler"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE2,
        ngx_tcl_tcl_handler_cmd,
        NGX_HTTP_LOC_CONF_OFFSET,
        0,
        NULL }

    ,ngx_null_command
};

ngx_module_t ngx_tcl_module = {
    NGX_MODULE_V1,
    &ngx_tcl_ctx,
    ngx_tcl_commands,
    NGX_HTTP_MODULE,
    NULL,
    ngx_tcl_init,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};

static Tcl_Obj *UNKNOWNMethodObj;
static Tcl_Obj *GETMethodObj;
static Tcl_Obj *HEADMethodObj;
static Tcl_Obj *POSTMethodObj;
static Tcl_Obj *PUTMethodObj;
static Tcl_Obj *DELETEMethodObj;

static ngx_int_t ngx_tcl_init(ngx_cycle_t *cycle)
{
printf("%s\n", __FUNCTION__); fflush(stdout);
#define METH(X) X ## MethodObj = Tcl_NewStringObj(#X, -1); \
        Tcl_IncrRefCount(X ## MethodObj);
    METH(UNKNOWN);
    METH(GET);
    METH(HEAD);
    METH(POST);
    METH(PUT);
    METH(DELETE);
#undef METH

    return NGX_OK;
}

static void *
ngx_tcl_create_loc_conf(ngx_conf_t *cf)
{
    ngx_tcl_loc_conf_t *conf;

printf("%s\n", __FUNCTION__); fflush(stdout);

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_tcl_loc_conf_t));

    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->interp = NGX_CONF_UNSET_PTR;

    return conf;
}

static char *
ngx_tcl_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_tcl_loc_conf_t *prev = parent;
    ngx_tcl_loc_conf_t *conf = child;

printf("%s\n", __FUNCTION__); fflush(stdout);

    ngx_conf_merge_ptr_value(conf->interp, prev->interp, NULL);

    return NGX_CONF_OK;
}

static char*
ngx_tcl_create_tcl_cmd(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    ngx_tcl_interp_t *I;
    ngx_str_t *args = cf->args->elts;

printf("create_tcl %p\n", conf); fflush(stdout);

    I = malloc(sizeof(ngx_tcl_interp_t));
    I->interp = Tcl_CreateInterp();
    I->name = args[1];
    I->file = args[2];
    I->next = interps;
    interps = I;

    Tcl_EvalFile(I->interp, (char*)I->file.data);

    return NGX_CONF_OK;
}

static char*
ngx_tcl_tcl_handler_cmd(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    ngx_str_t *args = cf->args->elts;
    ngx_http_core_loc_conf_t * clcf;
    ngx_tcl_loc_conf_t * lcf = (ngx_tcl_loc_conf_t*)conf;
    ngx_tcl_interp_t *i;

printf("%s %p\n", __FUNCTION__, conf); fflush(stdout);

    for (i = interps; i; i = i->next) {
        if (ngx_strcmp(args[1].data, i->name.data) == 0) break;
    }

    if (!i) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
            "interpreter not found %s", args[1].data); 
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_tcl_handler;

    lcf->interp = i->interp;
    lcf->handler = Tcl_NewStringObj((const char*)args[2].data, args[2].len);
    Tcl_IncrRefCount(lcf->handler);

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_tcl_handler(ngx_http_request_t * r)
{
    ngx_chain_t out;
    ngx_buf_t *b;
    const char *result;
    char cmdName[80];
    Tcl_Obj *objv[2];

    ngx_tcl_loc_conf_t *conf = ngx_http_get_module_loc_conf(r, ngx_tcl_module);

    ngx_snprintf((u_char*)cmdName, sizeof(cmdName), "ngx%p", r);
    Tcl_CreateObjCommand(conf->interp, cmdName, tclRequestCmd, r, NULL);

    objv[0] = conf->handler;
    objv[1] = Tcl_NewStringObj(cmdName, -1);

    if (Tcl_EvalObjv(conf->interp, 2, objv, TCL_EVAL_GLOBAL) != TCL_OK) {
        // TODO: result in log...
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    result = Tcl_GetString(Tcl_GetObjResult(conf->interp));
    if (strcmp(result, "") == 0 || strcmp(result, "HTTP_OK") == 0) {
        r->headers_out.status = NGX_HTTP_OK;
        return NGX_HTTP_OK;
    } else if (strcmp(result, "DECLINED") == 0) {
        return NGX_HTTP_NOT_ALLOWED;
    } 

    b = ngx_palloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    // b->pos = (u_char*)body;
    // b->last = (u_char*)body + sizeof(body) - 1;
    // b->memory = 1;	 // This buffer is in read-only memory
    // This means that filters should copy it, and not
    // try to rewrite in place
    // b->last_buf = 1; // this is the last buffer in the buffer chain

    // r->headers_out.status = NGX_HTTP_OK;
    // r->headers_out.content_length_n = sizeof(body) - 1;

    // rc = ngx_http_send_header(r);

    // if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
    //    return rc;
    //}

    return ngx_http_output_filter(r, &out);
}

static int
requestMethodSubcmd(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *result = UNKNOWNMethodObj;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;

    switch (r->method & 0x3f) {
        case NGX_HTTP_GET:    result = GETMethodObj;    break;
        case NGX_HTTP_HEAD:   result = HEADMethodObj;   break;
        case NGX_HTTP_POST:   result = POSTMethodObj;   break;
        case NGX_HTTP_PUT:    result = PUTMethodObj;    break;
        case NGX_HTTP_DELETE: result = DELETEMethodObj; break;
    }

    Tcl_SetObjResult(interp, result);

    return TCL_OK;
}

static int
requestStatusSubcmd(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;
    int status;
    int rc;

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 2, objv, NULL);
        return TCL_ERROR;
    }

    rc = Tcl_GetIntFromObj(interp, objv[3], &status);

    if (rc != TCL_OK) {
        return rc;
    }

    r->headers_out.status = status;

    return TCL_OK;
}

static int
requestSendHeaderSubcmd(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;
    int i;

    if (objc < 4) {
        Tcl_WrongNumArgs(interp, 2, objv, NULL);
        return TCL_ERROR;
    }

    for (i = 2; i < objc; i+=2 ) {
        if (strcasecmp(Tcl_GetString(objv[i]), "content_type")) {
            int len = Tcl_GetCharLength(objv[i+1]);
            r->headers_out.content_type_len = len;
            r->headers_out.content_type.len = len;
            r->headers_out.content_type.data = (u_char*)Tcl_GetString(objv[i+1]);
            ngx_http_send_header(r);
            // TODO: check return value
        } else if (strcasecmp(Tcl_GetString(objv[i]), "content_length")) {
            long contlen;
            // TODO: check return value
            Tcl_GetLongFromObj(interp, objv[3], &contlen);
            r->headers_out.content_length_n = (off_t)contlen;
        }

    }

    return TCL_OK;
}

static char *RequestSubCmdNames[] = {
    "method",
    "status",
    "send_header",
    NULL
};

static Tcl_ObjCmdProc *RequestSubCmds[] = {
    requestMethodSubcmd,
    requestStatusSubcmd,
    requestSendHeaderSubcmd
};

static int
tclRequestCmd(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    int index;
    int rc;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 2, objv, NULL);
        return TCL_ERROR;
    }

    rc = Tcl_GetIndexFromObj(interp, objv[1], RequestSubCmdNames, "", 0,
        &index);

    if (rc != TCL_OK) {
        return rc;
    }

    return RequestSubCmds[index](clientData, interp, objc, objv);
}
