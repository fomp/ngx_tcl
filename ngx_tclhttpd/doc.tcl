# doc.tcl
#
# File system based URL support.
# This calls out to the Auth module to check for access files.
# Once a file is found, it checks for content-type handlers defined
# by Tcl procs of the form Doc_$contentType.  If those are present
# then they are responsible for processing the file and returning it.
# Otherwise the file is returned by Doc_Handle.
#
# If a file is not found then a limited form of content negotiation is
# done based on the browser's Accept header.  For example, this makes
# it easy to transition between foo.shtml and foo.html.  Just rename
# the file and content negotiation will find it from old links.
#
# Stephen Uhler / Brent Welch (c) 1997-1998 Sun Microsystems
# Brent Welch (c) 1998-2000 Ajuba Solutions
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: doc.tcl 6859 2011-10-07 13:40:40Z dino $

package provide httpd::doc 0.9

package require httpd::doc_error
package require httpd::cookie

# Doc_Root --
#
# Query or set the physical pathname of the document root
#
# Arguments:
#	real 	Optional.  The name of the file system directory
#		containing the root of the URL tree.  If this is empty,
#		then the current document root is returned instead.
#	args	"real" followed by "args" for Url_PrefixInstall.
#		"real" is the name of the file system directory
#		containing the root of the URL tree.  If no args are given,
#		then the current document root is returned instead.
#
# Results:
#	If querying, returns the name of the directory of the document root.
#	Otherwise returns nothing.
#
# Side Effects:
#	Sets the document root.

proc Doc_Root {args} {
    global Doc
    if {[llength $args] > 0} {
        set real [lindex $args 0]
	set Doc(root) $real
	Doc_AddRoot / $real {*}[lrange $args 1 end]
        return
    }
    return $Doc(root)
}

# Doc_AddRoot
#	Add a file system to the virtual document hierarchy
#
# Arguments:
#	virtual		The URL prefix of the document tree to add.
#	directory	The file system directory containing the doc tree.
#	args		Same as args for Url_PrefixInstall
#
# Results:
#	None
#
# Side Effects:
#	Sets up a document URL domain and the document-based access hook.

proc Doc_AddRoot {virtual directory args} {
    Doc_RegisterRoot $virtual $directory
    Url_PrefixInstall $virtual [list DocDomain $virtual $directory] {*}$args
    Url_AccessInstall DocAccessHook
    return
}

# Doc_RegisterRoot
#	Add a file system managed by any Domain Handler (e.g. CGI)
#	This is necessary for Doc_AccessControl to search directories right.
#
# Arguments:
#	virtual		The prefix of the URL
#	directory	The directory that corresponds to $virtual
#
# Results:
#	None
#
# Side Effects:
#	Registers the URL to directory mapping

proc Doc_RegisterRoot {virtual directory} {
    global Doc
    if {[info exist Doc(root,$virtual)] &&
	    [string compare $Doc(root,$virtual) $directory] != 0} {
	return -code error \
		"Doc_RegisterRoot will not change an existing url to directory mapping"
    }
    set Doc(root,$virtual) $directory
}

# DocAccessHook
#
#	Access handle for Doc domains.
#	This looks for special files in the file system that
#	determine access control.  This is registered via
#	Url_AccessInstall
#
# Arguments:
#	sock	Client connection
#	url	The full URL. We realy need the prefix/suffix, which
#		is stored for us in the connection state
#
# Results:
#	"denied", in which case an authorization challenge or
#	not found error has been returned.  Otherwise "skip"
#	which means other access checkers could be run, but
# 	most likely access will be granted.

proc DocAccessHook {sock url} {
    global Doc
    upvar #0 Httpd$sock data

    # Make sure the path doesn't sneak out via ..
    # This turns the URL suffix into a list of pathname components

    if {[catch {Url_PathCheck $data(suffix)} data(pathlist)]} {
	Doc_NotFound $sock
	return denied
    }

    # .htaccess Abfrage entfernt, da von hserver nicht benoetigt
    # -- Ficicchia

    return skip
}

# DocDomain --
#
# Main handler for Doc domains (i.e. file systems)
# This looks around for a file and, if found, uses Doc_Handle
# to return the contents.
#
# Arguments:
#	prefix		The URL prefix of the domain.
#	directory	The directory containing teh domain.
#	sock		The socket connection.
#	suffix		The URL after the prefix.
#
# Results:
#	None
#
# Side Effects:
#	Dispatch to the document handler that is in charge
#	of generating an HTTP response.

proc DocDomain {prefix directory sock suffix} {
    global Doc
    upvar #0 Httpd$sock data

    # The pathlist has been checked and URL decoded by
    # DocAccess, so we ignore the suffix and recompute it.

    set pathlist $data(pathlist)
    set suffix [join $pathlist /]

    # Handle existing files

    # The file join here is subject to attacks that create absolute
    # pathnames outside the URL tree.  We trim left the / and ~
    # to prevent those attacks.

    set path [file join $directory [string trimleft $suffix /~]]
    set path [file normalize $path]

    set data(path) $path	;# record this path for not found handling

    if {[file exists $path]} {
	Doc_Handle $prefix $path $suffix $sock
	return
    }

    Doc_NotFound $sock
}

# Doc_Handle --
#
# Handle a document URL.  Dispatch to the mime type handler, if defined.
#
# Arguments:
#	prefix	The URL prefix of the domain.
#	path	The file system pathname of the file.
#	suffix	The URL suffix.
#	sock	The socket connection.
#
# Results:
#	None
#
# Side Effects:
#	Dispatch to the correct document handler.

proc Doc_Handle {prefix path suffix sock} {
    upvar #0 Httpd$sock data
    if {[file isdirectory $path]} {
	if {[string length $data(url)] && ![regexp /$ $data(url)]} {

	    # Insist on the trailing slash

	    Httpd_RedirectDir $sock
	    return
	}
        
        set path [file join $path index.html]
    }
    
    if {[file readable $path]} {
	
	# Look for Tcl procedures whos name match the MIME Content-Type

	set cmd Doc_[Mtype $path]
	if {![iscommand $cmd]} {
	    Httpd_ReturnFile $sock [Mtype $path] $path
	} else {
	    $cmd $path $suffix $sock
	}
    } else {
        Doc_NotFound $sock
    }
}

# Doc_GetPath --
#	
#	Return a list of unique directories from domain root to a given path
#	Adjusts for Document roots and user directories
#
# Arguments:
#	sock		The client connection
#	file		The file endpoint of the path
# Results:
#	A list of directories from root to directory of $data(path)
#
# Side Effects:
#	None.

proc Doc_GetPath {sock {file ""}} {
    global Doc
    upvar #0 Httpd$sock data

    if {$file == ""} {
	set file $data(path)
    }

    # Start at the Doc_AddRoot point
    if {[info exist Doc(root,$data(prefix))]} {
	set root $Doc(root,$data(prefix))

	# always start in the rootdir
	set dirs $Doc(root)
    } else {
	set root $Doc(root,/)
	set dirs {}
    }

    set dirsplit [file split [file dirname $file]]
    if {[string match ${root}* $file]} {

	# Normal case of pathname under domain prefix

	set path $root
	set extra [lrange $dirsplit [llength [file split $root]] end]

    } else {
	# Don't know where we are - just use the current directory

	set path [file dirname $file] 
	set extra {}
    }

    foreach dir [concat [list {}] $extra] {
	set path [file join $path $dir]
	# Don't add duplicates to the list.
	if {[lsearch $dirs $path] == -1} {
	    lappend dirs $path
	}
    }

    return $dirs
}
