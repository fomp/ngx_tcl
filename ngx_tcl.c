/**
 * vim:set sts=4 sw=4 et sta:
 */
 
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <tcl.h>

struct ngx_tcl_interp_conf_s {
    Tcl_Interp *interp;
    ngx_str_t name;
    ngx_str_t file;
    struct ngx_tcl_interp_conf_s *next;
};

typedef struct ngx_tcl_interp_conf_s ngx_tcl_interp_conf_t;

static ngx_tcl_interp_conf_t *interp_confs = NULL;

struct ngx_tcl_loc_conf_s {
    struct ngx_tcl_interp_conf_s *interp_conf;
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
static ngx_int_t ngx_http_tcl_handler(ngx_http_request_t *r);
static int	 tclRequestCmd(ClientData clientData, Tcl_Interp *interp,
		    int objc, Tcl_Obj *const objv[]);
static ngx_int_t ngx_tcl_init_module(ngx_cycle_t *cycle);
static ngx_int_t ngx_tcl_init_process(ngx_cycle_t *cycle);
static ngx_int_t ngx_tcl_init(ngx_conf_t *cf);

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
    ngx_tcl_init_module,
    ngx_tcl_init_process,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
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

static int
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

static void
ngx_tcl_cleanup_Tcl_Command(void *data)
{
    ngx_tcl_cleanup_Tcl_Command_t *clnd =
        (ngx_tcl_cleanup_Tcl_Command_t*)data;

printf("%s\n", __FUNCTION__); fflush(stdout);

    Tcl_DeleteCommandFromToken(clnd->interp, clnd->token);
}


static int
ngx_tcl_cleanup_add_Tcl_Command(ngx_pool_t *pool, Tcl_Interp *interp,
    Tcl_Command token)
{
    ngx_pool_cleanup_t *cln;
    ngx_tcl_cleanup_Tcl_Command_t *clnd;

printf("%s\n", __FUNCTION__); fflush(stdout);

    cln = ngx_pool_cleanup_add(pool, sizeof(ngx_tcl_cleanup_Tcl_Command_t));

    if (cln == NULL) {
        return TCL_ERROR;
    }

    cln->handler = ngx_tcl_cleanup_Tcl_Command;
    clnd = (ngx_tcl_cleanup_Tcl_Command_t*)cln->data;
    clnd->interp = interp;
    clnd->token = token;

    return TCL_OK;
}


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

        rc = Tcl_EvalFile(iconf->interp, (char*)iconf->file.data);
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
ngx_tcl_create_tcl_cmd(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    ngx_tcl_interp_conf_t *I;
    ngx_str_t *args = cf->args->elts;

printf("create_tcl %p\n", conf); fflush(stdout);

    I = malloc(sizeof(ngx_tcl_interp_conf_t));
    I->interp = NULL;
    I->name = args[1];
    I->file = args[2];
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
    lcf->handler = Tcl_NewStringObj((const char*)args[2].data, args[2].len);
    Tcl_IncrRefCount(lcf->handler);

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_tcl_handler(ngx_http_request_t * r)
{
    const char *result;
    char cmdName[80];
    Tcl_Obj *objv[2];
    int rc;
    Tcl_Command token;
    ngx_tcl_loc_conf_t *conf;
    Tcl_Interp *interp;

printf("%s\n", __FUNCTION__); fflush(stdout);

    conf = ngx_http_get_module_loc_conf(r, ngx_tcl_module);
    interp = conf->interp_conf->interp;

    /* create request command */
    snprintf(cmdName, sizeof(cmdName), "ngx%p", r);

    token = Tcl_CreateObjCommand(interp, cmdName, tclRequestCmd, r, NULL);

    ngx_tcl_cleanup_add_Tcl_Command(r->pool, interp, token);

    /* prepare arguments */
    objv[0] = conf->handler;
    objv[1] = Tcl_NewStringObj(cmdName, -1);

    Tcl_IncrRefCount(objv[0]);
    Tcl_IncrRefCount(objv[1]);

    /* call command */
    rc = Tcl_EvalObjv(interp, 2, objv, TCL_EVAL_GLOBAL);
printf("%s returned %i\n", __FUNCTION__, rc); fflush(stdout);

    Tcl_DecrRefCount(objv[0]);
    Tcl_DecrRefCount(objv[1]);

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
 * request-command
 */


static int
request_method(ClientData clientData, Tcl_Interp *interp, int objc,
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
request_uri(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *result;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;

    result = Tcl_NewStringObj((char*)r->uri.data, r->uri.len);
    // TODO: should result be checked???

    Tcl_SetObjResult(interp, result);

    return TCL_OK;
}

static int
request_url(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *result;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;

    result = Tcl_NewStringObj((char*)r->unparsed_uri.data, r->unparsed_uri.len);
    // TODO: should result be checked???

    Tcl_SetObjResult(interp, result);

    return TCL_OK;
}

static int
request_query(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *result;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;

    result = Tcl_NewStringObj((char*)r->args.data, r->args.len);
    // TODO: should result be checked???

    Tcl_SetObjResult(interp, result);

    return TCL_OK;
}

static int
request_ipaddr(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *result;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;
    u_char text[NGX_SOCKADDR_STRLEN];
    size_t textlen;

    textlen = ngx_sock_ntop(r->connection->sockaddr, r->connection->socklen,
        text, NGX_SOCKADDR_STRLEN, 0);

    result = Tcl_NewStringObj((char*)text, textlen);

    Tcl_SetObjResult(interp, result);

    return TCL_OK;
}

static int
request_protocol(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *result;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;

#if (NGX_HTTP_SSL)
    result = r->connection->ssl
        ? Tcl_NewStringObj("https", 5)
        : Tcl_NewStringObj("http", 4);
#else

    result = Tcl_NewStringObj("http", 4);

#endif

    Tcl_SetObjResult(interp, result);

    return TCL_OK;
}

static int
request_in_content_length(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    Tcl_Obj *result;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;

    result = Tcl_NewLongObj(r->headers_in.content_length_n);

    Tcl_SetObjResult(interp, result);

    return TCL_OK;
}

static int
request_out_status(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;
    int status;
    int rc;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "count");
        return TCL_ERROR;
    }

    rc = Tcl_GetIntFromObj(interp, objv[2], &status);

    if (rc != TCL_OK) {
        return rc;
    }

    r->headers_out.status = status;

    return TCL_OK;
}

static int
request_out_content_length(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;
    long contlen;
    int rc;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "Content-Length");
        return TCL_ERROR;
    }

    rc  = Tcl_GetLongFromObj(interp, objv[2], &contlen);
    if (rc != TCL_OK) {
        return rc;
    }

    r->headers_out.content_length_n = (off_t)contlen;

    return TCL_OK;
}

static int
request_out_content_type(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;
    int len;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "Content-Type");
        return TCL_ERROR;
    }

    len = Tcl_GetCharLength(objv[2]);
    r->headers_out.content_type_len = len;
    r->headers_out.content_type.len = len;
    r->headers_out.content_type.data = (u_char*)Tcl_GetString(objv[2]);

    return ngx_tcl_cleanup_add_Tcl_Obj(r->pool, objv[2]);
}

static int
request_out_headers_add(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    int i, len;
    ngx_table_elt_t *elt;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;

    if (objc % 2) {
        Tcl_WrongNumArgs(interp, 2, objv, "[key value ...]");
        return TCL_ERROR;
    }

    for (i = 2; i < objc; i += 2) {
        elt = ngx_list_push(&r->headers_out.headers);

        if (elt == NULL) {
            return TCL_ERROR;
        }

        elt->hash = 1;
        elt->key.data = (u_char*)Tcl_GetStringFromObj(objv[i], &len);
        elt->key.len = len;
        ngx_tcl_cleanup_add_Tcl_Obj(r->pool, objv[i]);

        elt->value.data = (u_char*)Tcl_GetStringFromObj(objv[i+1], &len);
        elt->value.len = len;
        ngx_tcl_cleanup_add_Tcl_Obj(r->pool, objv[i+1]);
    }

    return TCL_OK;
}

static int
request_send_header(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;
    int rc;

    if (objc != 2) {
        Tcl_WrongNumArgs(interp, 2, objv, NULL);
        return TCL_ERROR;
    }

    rc = ngx_http_send_header(r);
    if (rc != NGX_OK) {
        ngx_tcl_set_error_code(interp, rc);
        return TCL_ERROR;
    }

    return TCL_OK;
}

static int
request_send_content(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_buf_t *b;
    ngx_chain_t out;
    int rc;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "content");
        return TCL_ERROR;
    }

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return TCL_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    b->start = b->pos = (u_char*)Tcl_GetString(objv[2]);
    b->end = b->last = b->pos + Tcl_GetCharLength(objv[2]);
    b->memory = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;

    rc = ngx_tcl_cleanup_add_Tcl_Obj(r->pool, objv[2]);

    if (rc != TCL_OK) {
        return rc;
    }

    rc = ngx_http_output_filter(r, &out);

    if (rc != NGX_OK) {
        ngx_tcl_set_error_code(interp, rc);
        return TCL_ERROR;
    }

    printf("ngx_http_output_filter returns %i\n", rc); fflush(stdout);

    return TCL_OK;
}

static int
request_send_file(ClientData clientData, Tcl_Interp *interp, int objc,
    Tcl_Obj *const objv[])
{
    ngx_buf_t *b;
    ngx_chain_t out;
    ngx_http_request_t *r = (ngx_http_request_t*)clientData;
    ngx_file_info_t *fi;
    ngx_fd_t fd;
    const char *filename;
    int filename_len;
    int rc;

    if (objc != 3) {
        Tcl_WrongNumArgs(interp, 2, objv, "file");
        return TCL_ERROR;
    }

    filename = Tcl_GetStringFromObj(objv[2], &filename_len);
    rc = ngx_tcl_cleanup_add_Tcl_Obj(r->pool, objv[2]);
    if (rc != TCL_OK) {
        return rc;
    }

    fd = ngx_open_file(filename, NGX_FILE_RDONLY, NGX_FILE_OPEN, 0);

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

    if (ngx_http_set_etag(r) != NGX_OK) {
        return TCL_ERROR;
    }

    if (!r->header_sent) {
        r->headers_out.content_length_n = ngx_file_size(fi);
        r->headers_out.last_modified_time = ngx_file_mtime(fi);
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

static char *RequestSubNames[] = {
    "method",
    "uri",
    "url",
    "query",
    "ipaddr",
    "protocol",
    "in.content-length",
    "out.status",
    "out.content-length",
    "out.content-type",
    "out.headers.add",
    "send_header",
    "send_content",
    "send_file",
    NULL
};

static Tcl_ObjCmdProc *RequestSubs[] = {
    request_method,
    request_uri,
    request_url,
    request_query,
    request_ipaddr,
    request_protocol,
    request_in_content_length,
    request_out_status,
    request_out_content_length,
    request_out_content_type,
    request_out_headers_add,
    request_send_header,
    request_send_content,
    request_send_file
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

    rc = Tcl_GetIndexFromObj(interp, objv[1], RequestSubNames, "subcmd", 0,
        &index);

    if (rc != TCL_OK) {
        return rc;
    }

    rc = RequestSubs[index](clientData, interp, objc, objv);

    return rc;
}
