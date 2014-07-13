# vim:set sts=4 sw=4 et sta:

proc ngx_handler {req} {
    if {[$req method] eq "HEAD"} {
        $req status 100
        $req send_header content_type text/html
        return HTTP_OK
    } 

    return DECLINED
}

