#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static char * ngx_tcl(ngx_conf_t * cf, ngx_command_t * cmd, void * conf);

static u_char ngx_tcl_string[] = "This is fun!";
