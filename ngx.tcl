# vim:set sts=4 sw=4 et sta:

set content "hello world"
set content_length [string length $content]
set content_type text/html

puts "STARTING INTERP"

proc ngx_handler {r} {
    global content_length content_type content
    set method [$r method]

puts "method $method"

    switch -- $method {
        HEAD {
            puts "in head"
            $r status 100

            puts sending_header
            $r send_header content_length $content_length \
                content_type $content_type
        }

        GET {
            $r status 100
            $r content_length $content_length
            $r send_header content_type $content_type
            $r send_content $content
        }

        default {
            return DECLINED
        }
    }

    return OK
}

