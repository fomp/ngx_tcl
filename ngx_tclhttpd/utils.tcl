# utils.tcl
#
# Brent Welch (c) 1998-2000 Ajuba Solutions
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: utils.tcl 5904 2010-10-18 07:28:06Z dino $

package provide httpd::utils 0.9

# iscommand - returns true if the command is defined  or lives in auto_index.

proc iscommand {name} {
    expr {([string length [info command $name]] > 0) || [auto_load $name]}
}

# lappendOnce - add to a list if not already there

proc lappendOnce {listName value} {
    upvar $listName list
    if {![info exists list]} {
	lappend list $value
    } else {
	set ix [lsearch $list $value]
	if {$ix < 0} {
	    lappend list $value
	}
    }
}

# Delete a list item by value.  Returns 1 if the item was present, else 0

proc ldelete {varList value} {
    upvar $varList list
    if {![info exist list]} {
	return 0
    }
    set ix [lsearch $list $value]
    if {$ix >= 0} {
	set list [lreplace $list $ix $ix]
	return 1
    } else {
	return 0
    }
}

# escape html characters (simple version)

proc protect_text {text} {
    array set Map { < lt   > gt   & amp   \" quot}
    regsub -all {[\\$]} $text {\\&} text
    regsub -all {[><&"]} $text {\&$Map(&);} text
    subst -nocommands $text
}

# parray - version of parray that returns the result instead
# of printing it out.

proc parray {aname {pat *}} {
    upvar $aname a
    set max 0
    foreach name [array names a $pat] {
        if {[string length $name] > $max} {
            set max [string length $name]
        }
    }
    incr max [string length $aname]
    incr max 2
    set result {}
    foreach name [lsort [array names a $pat]] {
	append result [list set ${aname}($name) $a($name)]
	append result \n
    }
    return $result
}
