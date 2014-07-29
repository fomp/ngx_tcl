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

puts "TCL: STARTING INTERP"

proc ngx_handler {r} {
    set URL [$r url]
    set URI [$r uri]
    set METHOD [$r method]
    set PROT [$r protocol]
    set QRY [$r query]

    puts TCL:ipaddr:[$r ipaddr]:
    puts TCL:url:$URL:
    puts TCL:uri:$URI:
    puts TCL:query:$QRY:
    puts TCL:method:$METHOD:
    puts TCL:in.content-length:[$r in.content-length]:
    puts TCL:protocol:$PROT:

    $r out.header.add foo bar blim blam

    switch -- [$r method] {
        HEAD {
            $r out.status 200
            $r out.content-type $::content_type
            $r out.content-length $::content_length
            $r send_header
        }

        GET {
            if {[string match *passwd* [$r url]]} {
                $r out.status 200
                $r out.content-type text/plain
                $r out.content-length [file size /etc/passwd]
                $r send_header
                $r send_file /etc/passwd
            } else {
                $r out.status 200
                $r out.content-type $::content_type
                $r out.content-length $::content_length
                $r send_header
                $r send_content $::content
            }
        }

        default {
            return DECLINED
        }
    }

    return OK
}
