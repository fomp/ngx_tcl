/**
	vim:set sts=4 sw=4 et sta:
*/
 
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <tcl.h>

struct ngx_tcl_interp {
    Tcl_Interp *interp;
    ngx_str_t name;
    ngx_str_t file;
    struct ngx_tcl_interp *next;
};

struct ngx_tcl_loc_conf_s {
    Tcl_Interp *interp;
};

typedef struct ngx_tcl_loc_conf_s ngx_tcl_loc_conf_t;

static struct ngx_tcl_interp *interps = NULL;

static char * ngx_tcl_create_tcl(ngx_conf_t * cf, ngx_command_t * cmd,
        void * conf);

static char * ngx_tcl_to_tcl(ngx_conf_t * cf, ngx_command_t * cmd,
        void * conf);

static void * ngx_tcl_create_loc_conf(ngx_conf_t *cf);
static char * ngx_tcl_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t ngx_http_tcl_handler(ngx_http_request_t * r);

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
        ngx_tcl_create_tcl,
        0, 0,
        NULL },

    {   ngx_string("to_tcl"),
        NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
        ngx_tcl_to_tcl,
        0, 0,
        NULL }

    ,ngx_null_command
};

ngx_module_t ngx_tcl = {
    NGX_MODULE_V1,
    &ngx_tcl_ctx,
    ngx_tcl_commands,
    NGX_HTTP_MODULE,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NGX_MODULE_V1_PADDING
};

static void *
ngx_tcl_create_loc_conf(ngx_conf_t *cf)
{
    ngx_tcl_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_tcl_loc_conf_t));

    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    return conf;
}

static char *
ngx_tcl_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    return NGX_CONF_OK;
}

static char*
ngx_tcl_create_tcl(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    struct ngx_tcl_interp *I;
    ngx_str_t *args = cf->args->elts;

    I = malloc(sizeof(struct ngx_tcl_interp));
    I->interp = Tcl_CreateInterp();
    I->name = args[1];
    I->file = args[2];
    I->next = interps;
    interps = I;

    Tcl_EvalFile(I->interp, (char*)I->file.data);

    return NGX_CONF_OK;
}

static char*
ngx_tcl_to_tcl(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
    ngx_str_t *args = cf->args->elts;
    ngx_http_core_loc_conf_t * clcf;
    ngx_tcl_loc_conf_t * tcf = (ngx_tcl_loc_conf_t*)conf;
    struct ngx_tcl_interp *i;

    for (i = interps; i; i = i->next) {
        if (ngx_strcmp(args[1].data, i->name.data) == 0) break;
    }

    if (!i) {
        return NGX_CONF_ERROR;
    }

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_tcl_handler;

    tcf->interp = i->interp;

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_tcl_handler(ngx_http_request_t * r)
{
    ngx_chain_t out;
    ngx_buf_t *b;
    ngx_int_t rc;

    char body[] = "foobarbaz";

    if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    rc = ngx_http_discard_request_body(r);

    if (rc != NGX_OK) {
        return rc;
    }

    r->headers_out.content_type_len = sizeof("text/html") - 1;
    r->headers_out.content_type.len = sizeof("text/html") - 1;
    r->headers_out.content_type.data = (u_char*) "text/html";

    if (r->method == NGX_HTTP_HEAD) {
        r->headers_out.status = NGX_HTTP_OK;
        r->headers_out.content_length_n = sizeof(body) - 1;
        return ngx_http_send_header(r);
    }

    b = ngx_palloc(r->pool, sizeof(ngx_buf_t));
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    b->pos = (u_char*)body;
    b->last = (u_char*)body + sizeof(body) - 1;
    b->memory = 1;	 // This buffer is in read-only memory
    // This means that filters should copy it, and not
    // try to rewrite in place
    b->last_buf = 1; // this is the last buffer in the buffer chain

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = sizeof(body) - 1;

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    return ngx_http_output_filter(r, &out);
}

