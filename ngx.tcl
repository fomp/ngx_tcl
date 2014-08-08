# vim:set sts=4 sw=4 et sta:

set content {
<html>
    <head>
        <title>moep page</title>
    </head>
    <body>
        Hey, if you can see this, you can see this!!!
    </body>
</html>
}

set content_type text/html

puts "TCL: STARTING INTERP"

proc ngx_handler {} {
    set atts {unparsed_uri uri method protocol ipaddr}

    set unparsed_uri [ngx::req::unparsed_uri]
    set uri [ngx::req::uri]
    set method [ngx::req::method]
    set scheme [ngx::req::scheme]
    set ipaddr [ngx::req::ipaddr]

    ngx::res::status 200
    ngx::res::Content-Type $::content_type

    switch -- $method {
        HEAD {
            ngx::res::Content-Length [string length $::content]
            ngx::send_header
        }

        GET {
            if {[string match *passwd* $uri]} {
                ngx::res::Content-Type text/plain
                ngx::send_file /etc/passwd
            } else {
                ngx::res::Content-Type $::content_type
                ngx::send_content $::content
            }
        }

        default {
            return DECLINED
        }
    }

    return OK
}
