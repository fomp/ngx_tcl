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
    foreach var {
        uri request_uri request_method scheme remote_addr args content_length
    } {
        set val [ngx::getv $var ""]
        set $var $val
        puts [string toupper $var]:$val:
    }

    ngx::status 200

    ngx::outheader Content-Type $::content_type

    switch -- $request_method {
        HEAD {
            ngx::outheader Content-Length [string length $::content]
            ngx::sendheader
        }

        GET {
            if {[string match *passwd* $uri]} {
                ngx::outheader Content-Type text/plain
                ngx::sendfile /etc/passwd
            } else {
                ngx::outheader Content-Type $::content_type
                ngx::sendcontent $::content
            }
        }

        default {
            return DECLINED
        }
    }

    return OK
}
