;;;; maildefs.jl -- Mail configuration, and some utils
;;;  Copyright (C) 1997 John Harper <john@dcs.warwick.ac.uk>
;;;  $Id$

;;; This file is part of Jade.

;;; Jade is free software; you can redistribute it and/or modify it
;;; under the terms of the GNU General Public License as published by
;;; the Free Software Foundation; either version 2, or (at your option)
;;; any later version.

;;; Jade is distributed in the hope that it will be useful, but
;;; WITHOUT ANY WARRANTY; without even the implied warranty of
;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;; GNU General Public License for more details.

;;; You should have received a copy of the GNU General Public License
;;; along with Jade; see the file COPYING.  If not, write to
;;; the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

(require 'ring)
(require 'mailaddr)
(provide 'maildefs)


;; Configuration

(defvar mail-address-style 'angles
  "How to put the sender's full name into From: headers, options include:\n
	'angles		`Foo Bar <foo@bar.baz>'
	'parens		`foo@bar.baz (Foo Bar)'
	nil		`foo@bar.baz'")

(defvar mail-self-blind nil
  "When non-nil a BCC'd copy of the message is sent to the sender. The
header line is included in the initialised message, and so may be deleted
if necessary.

If a string, it is intepreted as a regular expression that must not match
any recipient addresses for a BCC header to be inserted.")

(defvar mail-archive-file-name nil
  "The name of a file to store a copy of each outgoing message in (through
the FCC header).")

(defvar mail-visible-headers
  "^((Resent-|)(From|Sender|Reply-To|To|Cc|Bcc|Date|Sender)|Subject|Keywords|Comments)[ \t]*:"
  "Regular expression matching the message headers that should be displayed
when showing a mail message.")

(defvar mail-highlighted-headers
  "^(Subject|From)[\t ]*:"
  "Regexp matching headers to be highlighted; currently only the first will
that matches will actually be highlighted.")

(defvar mail-highlight-face bold-face
  "Face used for highlighting message headers. The variable `mail-highlighted-
headers' defines which are selected.")

(defvar mail-folder-dir "~/Mail/"
  "The directory in which mail folders are stored by default.")

(defvar mail-default-folder "INBOX"
  "The default mail folder.")

(defvar mail-spool-files (concat "/usr/spool/mail/" (user-login-name))
  "The inboxes to check for new mail. This can be a single file name, a list
of file names, or a list of association-lists, each associating a mail
folder with a particular spool file (or list of spool files).

If any of the spool files is specified using a non-absolute pathname, it
is assumed to be relative to the `mail-folder-dir' directory.")

(defvar mail-header-separator "--text follows this line--"
  "Text used to separate headers from message body; removed before the
message is sent.")

(defvar mail-setup-hook nil
  "A hook called after initialising a mail message in the current buffer.")

(defvar mail-yank-hooks nil
  "A hook called when a message has been cited in a reply. Called with
two arguments, the start and end of the inserted text.")

(defvar mail-default-reply-to nil
  "If non-nil an address to put in the Reply-to header.")

(defvar mail-ignored-reply-tos nil
  "If non-nil, a regexp matching Reply-to headers to ignore.")

(defvar mail-reply-prefix "Re: "
  "String to prepend to subject of replied messages.")

(defvar mail-re-regexp "^[\t ]*(Re([[(][0-9]+[])]|\\^[0-9]+)?:[\t ]*)*"
  "This matches most forms of "Re: SUBJECT" strings, leaving SUBJECT
starting at the end of the first substring")

(defvar mail-yank-prefix ">"
  "String to insert before quoted text in mail messages.")

(defvar mail-fill-column 72
  "Column to wrap at when inserting lists, and filling messages.")

(defvar mail-signature nil
  "String inserted at end of message being sent. If t means to insert the
contents of the file specified by mail-signature-file.")

(defvar mail-signature-file "~/.signature"
  "File to insert at end of message being sent.")

(defvar mail-default-headers (concat "X-Mailer: Jade " jade-version)
  "Text to insert into the header section of all outgoing mail messages.")

(defvar mail-send-hook nil
  "Hook called immediately prior to sending the mail message in the current
buffer.")

(defvar mail-send-function (lambda () (sendmail-send-message))
  "Function called when the mail message in the current buffer needs to
be sent. Should throw an error when unsucessful.")

(defvar sendmail-program "/usr/lib/sendmail"
  "Location in the filesystem of the sendmail program.")

(defvar movemail-program nil
  "Location of the movemail program.")

;; This is defined as 1 or more characters followed by a colon. The
;; characters may not include SPC, any control characters, or ASCII DEL.
;; Unfortunately the regexp library doesn't allow NUL bytes in regexps so
;; they slip through..
(defvar mail-header-name "^([^\001- \^?]+)[ \t]*:"
  "A regexp matching a header field name. The name is left in the match
buffer as the first substring. No other substrings may be used.")

(defvar mail-two-digit-year-prefix (substring (current-time-string) 20 22)
  "A two-digit string that will be prepended to year specifications that
only have two, lower order, digits. This is picked up automatically from
the current year, i.e. 1997 -> \"19\", 2001 -> \"20\".")

(defvar mail-summary-percent 15
  "The percentage of the window that the summary view of a mail folder should
take up.")

(defvar mail-display-summary nil
  "When non-nil a summary of the current folder is always displayed when
reading mail. If set to the symbol `bottom' the summary will be displayed
at the bottom of the screen, otherwise it will be shown at the top.")

(defvar mail-message-start "^From "
  "The regular expression separating each message. Actually it's interpreted
as a single blank line or the start of the buffer, followed by this regexp.")

;; This is pretty close to RFC-822, all excluded chars but NUL are
;; handled by this simple regexp (regexp lib restrictions again)
(defvar mail-atom-re "[^][()<>@,;\\\".\001-\037 \t\177]+"
  "Regular expression defining a single atom in a mail header. May not
include any parenthesised expressions!")

(defvar mail-message-id-re (concat ?< mail-atom-re "(\\." mail-atom-re ")*@"
				   mail-atom-re "(\\." mail-atom-re ")*>")
  "Regular expression matching a message id.")

(defvar mail-month-alist '(("Jan" . 1) ("Feb" . 2) ("Mar" . 3) ("Apr" . 4)
			   ("May" . 5) ("Jun" . 6) ("Jul" . 7) ("Aug" . 8)
			   ("Sep" . 9) ("Oct" . 10) ("Nov" . 11) ("Dec" . 12)
			   ("January" . 1) ("February" . 2) ("March" . 3)
			   ("April" . 4) ("June" . 6) ("July" . 7)
			   ("August" . 8) ("September" . 9) ("October" . 10)
			   ("November" . 11) ("December" . 12))
   "Alist of (MONTH-NAME . MONTH-NUM).")

(defvar mail-timezone-alist
  '(("UT" . 0) ("GMT" . 0)
    ("EST" . -300) ("EDT" . -240)
    ("CST" . -360) ("CDT" . -300)
    ("MST" . -420) ("MDT" . -360)
    ("PST" . -480) ("PDT" . -420))
  "Alist of (TIMEZONE . MINUTES-DIFFERENCE).")


;; A couple of utility functions

;; Return a list of inboxes for the mail folder FOLDER
(defun mail-find-inboxes (folder)
  (cond
   ((null mail-spool-files)
    nil)
   ((stringp mail-spool-files)
    (list mail-spool-files))
   ((and (consp mail-spool-files) (stringp (car mail-spool-files)))
    mail-spool-files)
   (t
    ;; Must be a list of lists.
    (let
	((tem mail-spool-files)
	 lst)
      (while tem
	(when (or (and (file-name-absolute-p (car (car tem)))
		       (file-name= folder (car (car tem))))
		  (file-name= folder (expand-file-name (car (car tem))
						       mail-folder-dir)))
	  (setq lst (append lst (if (consp (cdr (car tem)))
				      (cdr (car tem))
				    (cons (cdr (car tem)))))))
	(setq tem (cdr tem)))
      ;; Ensure that inbox names are absolute
      (mapcar #'(lambda (inbox)
		  (if (file-name-absolute-p inbox)
		      inbox
		    (expand-file-name inbox mail-folder-dir))) lst)))))

;; History list of prompt-for-folder
(defvar mail-prompt-history (make-ring))

;; Prompt for the name of a mail folder
(defun prompt-for-folder (title &optional default)
  (prompt-for-file title nil
		   (and default (file-name-directory default))
		   default mail-prompt-history))
