# cgi.tcl
# CGI support
# Stephen Uhler / Brent Welch (c) 1997 Sun Microsystems
# Brent Welch (c) 1998-2000 Ajuba Solutions
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: cgi.tcl 5904 2010-10-18 07:28:06Z dino $

package provide httpd::cgi 0.9

# Set the environment for the cgi scripts.  This is passed implicitly to
# the cgi script during the "open" call.

proc Cgi_SetEnv {sock path {var env}} {
    upvar 1 $var env
    upvar #0 Httpd$sock data
    Cgi_SetEnvAll $sock $path {} $data(url) env
}

proc Cgi_SetEnvAll {sock path extra url var} {
    upvar #0 Httpd$sock data
    upvar 1 $var env
    global Httpd Httpd_EnvMap Cgi

    foreach name [array names Httpd_EnvMap] {
	set env($name) ""
	catch {
	    set env($name) $data($Httpd_EnvMap($name))
	}
    }
    set env(REQUEST_URI) [Httpd_SelfUrl $data(uri) $sock]
    set env(GATEWAY_INTERFACE) "CGI/1.1"
    set env(SERVER_PORT) [Httpd_Port $sock]
    if {[info exist Httpd(https_port)]} {
	set env(SERVER_HTTPS_PORT) $Httpd(https_port)
    }
    set env(SERVER_NAME) $Httpd(name)
    set env(SERVER_SOFTWARE) $Httpd(server)
    set env(SERVER_PROTOCOL) HTTP/1.0
    set env(REMOTE_ADDR) $data(ipaddr)
    set env(SCRIPT_NAME) $url
    set env(PATH_INFO) $extra
    set env(PATH_TRANSLATED) [string trimright [Doc_Root] /]/[string trimleft $data(url) /]
    set env(DOCUMENT_ROOT) [Doc_Root]
    # set env(HOME) [Doc_Root] ;# macht Probleme

    if {$data(proto) == "POST"} {
	set env(QUERY_STRING) ""
    }
}
