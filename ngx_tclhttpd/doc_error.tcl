# doc_error.tcl
#@c handlers for server errors and doc-not-found cases.
#
#
# Derived from doc.tcl
# Stephen Uhler / Brent Welch (c) 1997-1998 Sun Microsystems
# Brent Welch (c) 1998-2000 Ajuba Solutions
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: doc_error.tcl 5904 2010-10-18 07:28:06Z dino $

package provide httpd::doc_error 0.9

package require httpd::utils

# Doc_NotFound --
#
#@c	Called when a page is missing.  This looks for a handler page
#@c	and sets up a small amount of context for it.
#
# Arguments:
#@a	sock	The socket connection.
#
# Results:
#@r	None
#
# Side Effects:
#@e	Returns a page.

proc Doc_NotFound { sock } {
    global Doc Referer
    upvar #0 Httpd$sock data
    set Doc(url,notfound) $data(url)	;# For subst
    if {[info exists data(mime,referer)]} {

	# Record the referring URL so we can track down
	# bad links

	lappendOnce Referer($data(url)) $data(mime,referer)
    }
    Httpd_Error $sock 404 [protect_text $Doc(url,notfound)]
}
