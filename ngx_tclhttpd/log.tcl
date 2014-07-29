# log.tcl
#
#	This is a file-based logging module for TclHttpd.
#
#	This starts a new log file each day with Log_SetFile
#	It also maintains an error log file that is always appeneded to
#	so it grows over time even when you restart the server.
#
# Stephen Uhler / Brent Welch (c) 1997 Sun Microsystems
# Brent Welch (c) 1998-2000 Ajuba Solutions
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# RCS: @(#) $Id: log.tcl 6166 2011-02-18 17:36:43Z dino $

package provide httpd::log 0.9
package require hserver::log

# log an Httpd transaction

# Log --
#
#	Log information about the activity of TclHttpd.  There are two kinds of
#	log entries.  One "normal" entry that goes into its own log, one line
#	for each HTTP transaction.  All other log records are appended to an
#	error log file.
#
# Arguments:
#	sock	The client connection.
#	reason	If "Close", then this is the normal completion of a request.
#		Otherwise, this is some error tag and the record goes to
#		the error log.
#	args	Additional information to put into the logs.
#
# Results:
#	None
#
# Side Effects:
#	Writes data to the log files.

proc Log {sock reason args} {
    ::hserver::log::logN HTTP ERRORS SOCK $sock REASON $reason MSG $args
}
