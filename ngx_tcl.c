/**
 * vim:set sts=4 sw=4 et sta:
 */
 
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <tcl.h>

#include "ngx_tcl.h"
#include "ngx_tcl_var.h"
#include "ngx_tcl_header.h"

static ngx_tcl_interp_conf_t *interp_confs = NULL;
static Tcl_ObjType *tclByteArrayType;

struct ngx_tcl_loc_conf_s {
    struct ngx_tcl_interp_conf_s *interp_conf;
    Tcl_Obj *script;
};

typedef struct ngx_tcl_loc_conf_s ngx_tcl_loc_conf_t;


static char         *ngx_tcl_tcl_interp_cmd(ngx_conf_t *cf,
                        ngx_command_t * cmd, void * conf);
static char         *ngx_tcl_tcl_handler_cmd(ngx_conf_t *cf,
                        ngx_command_t * cmd, void * conf);
static void         *ngx_tcl_create_loc_conf(ngx_conf_t *cf);
static char         *ngx_tcl_merge_loc_conf(ngx_conf_t *cf, void *parent,
                        void *child);

static ngx_int_t     ngx_http_tcl_handler(ngx_http_request_t *r);
static ngx_int_t     ngx_tcl_init_module(ngx_cycle_t *cycle);
static ngx_int_t     ngx_tcl_init_process(ngx_cycle_t *cycle);
static ngx_int_t     ngx_tcl_init(ngx_conf_t *cf);

static int           response_set_status(ClientData clientData,
                        Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

static int           command_sendheader(ClientData clientData,
                        Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int           command_sendcontent(ClientData clientData,
                        Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);
static int           command_sendfile(ClientData clientData,
                        Tcl_Interp *interp, int objc, Tcl_Obj *const objv[]);

#define getrequest(C) ((ngx_tcl_interp_conf_t*)(C))->request

static ngx_http_module_t ngx_tcl_ctx = {
    NULL,	// preconfiguration
    ngx_tcl_init, // postconfiguration

    NULL,	// create main configuration
    NULL,	// init main configuration

    NULL,	// create server configuration
    NULL,	// merge server configuration

    ngx_tcl_create_loc_conf,	// create location configuration
    ngx_tcl_merge_loc_conf	// merge location configuration
};

static ngx_command_t ngx_tcl_commands[] = {

    {   ngx_string("tcl_interp"),
        NGX_HTTP_MAIN_CONF|NGX_CONF_TAKE2,
        ngx_tcl_tcl_interp_cmd,
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
    ngx_tcl_init_module,
    ngx_tcl_init_process,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};

static struct {
    char *name;
    Tcl_ObjCmdProc *cmd;
} commands[] = {
    {"::ngx::sendheader", command_sendheader},
    {"::ngx::sendcontent", command_sendcontent},
    {"::ngx::sendfile", command_sendfile},
    {"::ngx::getv", ngx_http_tcl_getv_cmd},
    {"::ngx::setv", ngx_http_tcl_setv_cmd},
    {"::ngx::outheader", ngx_http_tcl_outheader_cmd},

    {"::ngx::status", response_set_status },

    {NULL, NULL}
};

static void
ngx_tcl_set_error_code(Tcl_Interp *interp, int rc)
{
    char rcstr[10];
    ngx_snprintf((u_char*)rcstr, sizeof(rcstr), "%i", rc);
    Tcl_SetErrorCode(interp, "NGX", rcstr, NULL);
}

static void
ngx_tcl_cleanup_Tcl_Obj(void *data)
{
    Tcl_Obj *obj = *(Tcl_Obj**)data;
    printf("%s(%p)\n", __FUNCTION__, obj); fflush(stdout);
    Tcl_DecrRefCount(obj);
}

int
ngx_tcl_cleanup_add_Tcl_Obj(ngx_pool_t *p, Tcl_Obj *obj)
{
    ngx_pool_cleanup_t *cln;

printf("%s(%p)\n", __FUNCTION__, obj); fflush(stdout);
    cln = ngx_pool_cleanup_add(p, sizeof(Tcl_Obj*));

    if (cln == NULL) {
        return TCL_ERROR;
    }

    cln->handler = ngx_tcl_cleanup_Tcl_Obj;
    *(Tcl_Obj**)cln->data = obj;
    Tcl_IncrRefCount(obj);

    return TCL_OK;
}

typedef struct {
    Tcl_Interp *interp;
    Tcl_Command token;
} ngx_tcl_cleanup_Tcl_Command_t;

static Tcl_Obj *UNKNOWNMethodObj;
static Tcl_Obj *GETMethodObj;
static Tcl_Obj *HEADMethodObj;
static Tcl_Obj *POSTMethodObj;
static Tcl_Obj *PUTMethodObj;
static Tcl_Obj *DELETEMethodObj;

static ngx_int_t
ngx_tcl_init_module(ngx_cycle_t *cycle)
{
printf("%s\n", __FUNCTION__); fflush(stdout);
#define OBJ(X) X ## MethodObj = Tcl_NewStringObj(#X, -1); \
        Tcl_IncrRefCount(X ## MethodObj);
    OBJ(UNKNOWN);
    OBJ(GET);
    OBJ(HEAD);
    OBJ(POST);
    OBJ(PUT);
    OBJ(DELETE);
#undef OBJ

    tclByteArrayType = Tcl_GetObjType("bytearray");

    return NGX_OK;
}

static ngx_int_t
ngx_tcl_init_process(ngx_cycle_t *cycle)
{
printf("%s\n", __FUNCTION__); fflush(stdout);

    return NGX_OK;
}

static ngx_int_t
ngx_tcl_init(ngx_conf_t *cf)
{
    ngx_tcl_interp_conf_t *iconf;
    int rc;
    int i;

printf("%s\n", __FUNCTION__); fflush(stdout);

    if (interp_confs != NULL) {
        Tcl_FindExecutable(NULL);
    }

    for (iconf = interp_confs; iconf != NULL; iconf = iconf->next) {
        iconf->interp = Tcl_CreateInterp();
        rc = Tcl_Init(iconf->interp);
        if (rc != TCL_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "tcl error: %s", Tcl_GetStringResult(iconf->interp));
            return NGX_ERROR;
        }

        for (i = 0; commands[i].name; ++i) {
            Tcl_CreateObjCommand(iconf->interp, commands[i].name,
                commands[i].cmd, iconf, NULL);
        }

        rc = Tcl_Eval(iconf->interp, (char*)iconf->initscript.data);
        if (rc != TCL_OK) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "tcl error: %s\n%s\n",
                Tcl_GetStringResult(iconf->interp),
                Tcl_GetVar(iconf->interp, "errorInfo", TCL_GLOBAL_ONLY)
            );

            return NGX_ERROR;
        }
    }

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

    conf->interp_conf = NGX_CONF_UNSET_PTR;

    return conf;
}

static char *
ngx_tcl_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_tcl_loc_conf_t *prev = parent;
    ngx_tcl_loc_conf_t *conf = child;

printf("%s\n", __FUNCTION__); fflush(stdout);

    ngx_conf_merge_ptr_value(conf->interp_conf, prev->interp_conf, NULL);

    return NGX_CONF_OK;
}

static char*
ngx_tcl_tcl_interp_cmd(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    ngx_tcl_interp_conf_t *I;
    ngx_str_t *args = cf->args->elts;

printf("tcl_interp %p\n", conf); fflush(stdout);

    I = malloc(sizeof(ngx_tcl_interp_conf_t));
    I->interp = NULL;
    I->name = args[1];
    I->initscript = args[2];
    I->next = interp_confs;
    interp_confs = I;

    return NGX_CONF_OK;
}

static char*
ngx_tcl_tcl_handler_cmd(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    ngx_str_t *args = cf->args->elts;
    ngx_http_core_loc_conf_t * clcf;
    ngx_tcl_loc_conf_t * lcf = (ngx_tcl_loc_conf_t*)conf;
    ngx_tcl_interp_conf_t *iconf;

printf("%s %p\n", __FUNCTION__, conf); fflush(stdout);

    for (iconf = interp_confs; iconf; iconf = iconf->next) {
        if (ngx_strcmp(args[1].data, iconf->name.data) == 0) break;
    }

    if (!iconf) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
            "interpreter not found %s", args[1].data);
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_tcl_handler;

    lcf->interp_conf = iconf;
    lcf->script = Tcl_NewStringObj((const char*)args[2].data, args[2].len);
    Tcl_IncrRefCount(lcf->script);

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_tcl_handler(ngx_http_request_t * r)
{
    const char *result;
    int rc;
    ngx_tcl_loc_conf_t *tlc;
    Tcl_Interp *interp;

printf("%s\n", __FUNCTION__); fflush(stdout);

    tlc = ngx_http_get_module_loc_conf(r, ngx_tcl_module);
    tlc->interp_conf->request = r;
    interp = tlc->interp_conf->interp;

    /* call command */
    rc = Tcl_EvalObjEx(interp, tlc->script, TCL_EVAL_GLOBAL);
printf("%s returned %i\n", __FUNCTION__, rc); fflush(stdout);

    if (rc != TCL_OK) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "interpreter returned: %s\n%s",
            Tcl_GetStringResult(interp),
            Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY)
        );
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    result = Tcl_GetString(Tcl_GetObjResult(interp));
    if (strcmp(result, "") == 0 || strcasecmp(result, "OK") == 0) {
printf("returning NGX_HTTP_OK\n"); fflush(stdout);
        return NGX_HTTP_OK;
    } else if (strcasecmp(result, "DECLINED") == 0) {
        return NGX_HTTP_NOT_ALLOWED;
    } else if (strcasecmp(result, "AGAIN") == 0) {
        return NGX_AGAIN;
    }

    return NGX_HTTP_OK;
}

/*******************************************************************************
 * RESPONSE commands
 */

static int
response_set_status(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_http_request_t *r = getrequest(clientData);
    int status;
    int rc;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "status");
        return TCL_ERROR;
    }

    rc = Tcl_GetIntFromObj(interp, objv[1], &status);

    if (rc != TCL_OK) {
        return rc;
    }

    r->headers_out.status = status;

    return TCL_OK;
}

/*******************************************************************************
 * send header command
 */

static int
command_sendheader(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_http_request_t *r = getrequest(clientData);
    int rc;

    if (objc != 1) {
        Tcl_WrongNumArgs(interp, 1, objv, NULL);
        return TCL_ERROR;
    }

    rc = ngx_http_send_header(r);
    if (rc != NGX_OK) {
        ngx_tcl_set_error_code(interp, rc);
        return TCL_ERROR;
    }

    return TCL_OK;
}

/*******************************************************************************
 * sendcontent command
 */

static int
command_sendcontent(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_buf_t *b;
    ngx_chain_t out;
    Tcl_Obj *content;
    int rc;
    int len;
    ngx_http_request_t *r = getrequest(clientData);

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "content");
        return TCL_ERROR;
    }

    content = objv[1];

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return TCL_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    b->start = (u_char*)Tcl_GetByteArrayFromObj(content, &len);

    b->pos = b->start;
    b->end = b->last = b->start + len;
    b->memory = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;

    rc = ngx_tcl_cleanup_add_Tcl_Obj(r->pool, content);

    if (rc != TCL_OK) {
        return rc;
    }

    if (!r->header_sent) {
        r->headers_out.content_length_n = len;
        if (r->headers_out.status == 0) {
            r->headers_out.status = 200;
        }
        ngx_http_send_header(r);
        /* TODO: CHECK RETURN */
    }

    rc = ngx_http_output_filter(r, &out);

    if (rc != NGX_OK) {
        ngx_tcl_set_error_code(interp, rc);
        return TCL_ERROR;
    }

    printf("ngx_http_output_filter returns %i\n", rc); fflush(stdout);

    return TCL_OK;
}

/*******************************************************************************
 * send file command
 */

static int
command_sendfile(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_buf_t *b;
    ngx_chain_t out;
    ngx_http_request_t *r = getrequest(clientData);
    ngx_file_info_t *fi;
    ngx_fd_t fd;
    const char *filename;
    int filename_len;
    int rc;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "file");
        return TCL_ERROR;
    }

    filename = Tcl_GetStringFromObj(objv[1], &filename_len);

    fd = ngx_open_file(filename, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);
    if (fd == NGX_INVALID_FILE) {
        Tcl_SetResult(interp, (char *) Tcl_PosixError(interp),
            TCL_VOLATILE);
        return TCL_ERROR;
    }

    b = ngx_calloc_buf(r->pool);
   if (b == NULL) {
        return TCL_ERROR;
    }

    b->file = ngx_pcalloc(r->pool, sizeof(ngx_file_t));
    if (b->file == NULL) {
        return TCL_ERROR;
    }

    fi = &b->file->info;
    ngx_fd_info(fd, fi);

    if (!r->header_sent) {
        r->headers_out.content_length_n = ngx_file_size(fi);
        r->headers_out.last_modified_time = ngx_file_mtime(fi);

        if (ngx_http_set_etag(r) != NGX_OK) {
            return TCL_ERROR;
        }

        ngx_http_send_header(r);
        /* TODO: CHECK RETURN */
    }

    r->allow_ranges = 1;

    b->file_pos = 0;
    b->file_last = ngx_file_size(fi);

    b->in_file = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;

    b->file->fd = fd;
    b->file->name.data = (u_char*)filename;
    b->file->name.len = filename_len;
    b->file->log = r->connection->log;

    out.buf = b;
    out.next = NULL;

    rc = ngx_http_output_filter(r, &out);

    if (rc != NGX_OK) {
        ngx_tcl_set_error_code(interp, rc);
        return TCL_ERROR;
    }

    printf("ngx_http_output_filter returns %i\n", rc); fflush(stdout);

    return TCL_OK;
}
