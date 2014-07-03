#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <tcl.h>

static char * ngx_tcl_tcl(ngx_conf_t * cf, ngx_command_t * cmd, void * conf);

static ngx_http_module_t ngx_tcl_ctx = {
	NULL,	// preconfiguration
	NULL,	// postconfiguration

	NULL,	// create main configuration
	NULL,	// init main configuration

	NULL,	// create server configuration
	NULL,	// merge server configuration

	NULL,	// create location configuration
	NULL	// merge location configuration
};

static ngx_command_t ngx_tcl_commands[] = {
	{ ngx_string("tcl"),
	NGX_MAIN_CONF|NGX_DIRECT_CONF|NGX_CONF_TAKE1,
	ngx_tcl_tcl,
	0, 0,
	NULL },
	ngx_null_command
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

static char*
ngx_tcl_tcl(ngx_conf_t * cf, ngx_command_t * cmd, void * conf)
{
	ngx_http_core_loc_conf_t * clcf;
	Tcl_Interp * I;

printf("moep\n");

	clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
	clcf->handler = ngx_http_tcl_handler;

	I = Tcl_CreateInterp();
	Tcl_EvalFile(I, "ngx.tcl");

	return NGX_CONF_OK;
}
