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

# set content "hello world"

set content_length [string length $content]
set content_type text/html

puts "STARTING INTERP"

proc ngx_handler {r} {
    $r status 100
    $r content_type $::content_type
    $r content_length $::content_length
    $r send_header

    switch -- [$r method] {
        HEAD {
        }

        GET {
            puts "sending..."
            $r send_content $::content
        }

        default {
            return DECLINED
        }
    }

    return OK
}

