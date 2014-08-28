# vim: set sw=4 sts=4 et sta:

package provide httpd 0.9

package require httpd::utils

array set Httpd_Errors {
    200 {Data follows}
    204 {No Content}
    302 {Found}
    304 {Not Modified}
    400 {Bad Request}
    401 {Authorization Required}
    403 {Permission denied}
    404 {Not Found}
    408 {Request Timeout}
    411 {Length Required}
    419 {Expectation Failed}
    500 {Server Internal Error}
    501 {Server Busy}
    503 {Service Unavailable}
    504 {Service Temporarily Unavailable}
}

proc HttpdErrorString { code } {
    global Httpd_Errors
    if {[info exist Httpd_Errors($code)]} {
	return $Httpd_Errors($code)
    } else {
	return "Error $code"
    }
}

set Httpd_ErrorFormat {
    <title>Httpd_Error: %1$s</title>
    Got the error <b>%2$s</b><br>
    while trying to obtain <b>%3$s</b>.
}

proc Httpd_Error {sock code {detail ""}} {

    upvar #0 Httpd$sock data
    global Httpd Httpd_ErrorFormat

    append data(url) ""
    set message [format $Httpd_ErrorFormat $code [HttpdErrorString $code] \
	$data(url)]

    append message <br>[protect_text $detail]
    Log $sock Error $code $data(url) $detail

    # TODO: completion-callbacks???
    # HttpdDoCallback $sock $message

    if {[catch {
        ::ngx::outheader Content-Type text/html
        ::ngx::status $code
        ::ngx::sendcontent $message
    } err]} {
	Log $sock LostSocket $data(url) $err
        return -code error $err
    }
}

# Httpd_SelfUrl --
#
# Create an absolute URL for this server
#
# Arguments:
#	url	A server-relative URL on this server.
#	sock	The current connection so we can tell if it
#		is the regular port or the secure port.
#
# Results:
#	An absolute URL.
#
# Side Effects:
#	None

proc Httpd_SelfUrl {url {sock ""}} {
    global Httpd
    if {$sock == ""} {
	set sock $Httpd(currentSocket)
    }
    upvar #0 Httpd$sock data

    set type [Httpd_Protocol $sock]
    set port [Httpd_Port $sock]
    if {[info exists data(mime,host)]} {

	# Use in preference to our "true" name because
	# the client might not have a DNS entry for use.

	set name $data(mime,host)
    } else {
	set name [Httpd_Name $sock]
    }
    set newurl $type://$name
    if {[string first : $name] == -1} {
	# Add in the port number, which may or may not be present in
	# the name already.  IE5 sticks the port into the Host: header,
	# while Tcl's own http package does not...

	if {$type == "http" && $port != 80} {
	    append newurl :$port
	}
	if {$type == "https" && $port != 443} {
	    append newurl :$port
	}
    }
    append newurl $url
}

proc Httpd_Protocol {sock} {
    return [::ngx::getv scheme]
}

proc Httpd_Port {sock} {
    # TODO: build it
    return 8080
}

proc Httpd_Name {sock} {
    # TODO: bulid it
    return localhost
}

proc Httpd_SetCookie {sock cookie {modify 0}} {
    ::ngx::outheader set-cookie $cookie
}

proc Httpd_CurrentSocket {{sock {}}} {
    global Httpd
    if {[string length $sock]} {
        set Httpd(currentSocket) $sock
    }
    return $Httpd(currentSocket)
}

proc Httpd_ReturnFile {sock type path {offset 0}} {
    ::ngx::status 200
    ::ngx::outheader content-type $type
    ::ngx::sendfile $path
}

proc Httpd_ReturnData {sock type data {code 200} {close 0}} {
    ::ngx::outheader content-type $type
    ::ngx::status $code
    ::ngx::sendcontent $data
}

set HttpdRedirectFormat {
    <html><head>
    <title>Found</title>
    </head><body>
    This document has moved to a new <a href="%s">location</a>.
    Please update your documents and hotlists accordingly.
    </body></html>
}

proc Httpd_Redirect {newurl sock} {
    global Httpd HttpdRedirectFormat

    set message [format $HttpdRedirectFormat $newurl]
    ::ngx::outheader \
        Location $newurl \
        content-type text/html
    ::ngx::status 302
    ::ngx::sendcontent $message
}

proc ::ngx::socket {} {
    return sock42
}

proc ::ngx::var? {varname} {
    if {[catch {var $varname} result]} {
        set result {}
    }

    return $result
}

proc emu_tclhttpd {} {
    # TODO: name must be set...

    set sock [ngx::socket]

    array set ::Httpd$sock [list                \
        proto   [::ngx::getv scheme]            \
        uri     [::ngx::getv request_uri]       \
        url     [::ngx::getv uri]               \
        query   [::ngx::getv args {}]           \
        ipaddr  [::ngx::getv remote_addr]       \
	cert    ""                              \
        count   [::ngx::getv content_length 0]  \
        name    localhost                       \
    ]


    array set ::Httpd [list             \
        name            localhost       \
        server          ngx_tclhttp/0.1 \
        currentSocket   $sock           \
    ]

    ::Url_Dispatch $sock
}

