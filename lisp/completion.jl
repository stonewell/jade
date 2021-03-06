;;;; completion.jl -- Code for displaying completions
;;;  Copyright (C) 1998 John Harper <john@dcs.warwick.ac.uk>
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

(provide 'completion)

(defvar completion-sorted-lists t
  "When t, displayed completion-lists are sorted into alphabetical order.")

(defvar completion-column-extra 2
  "Number of glyphs added to the width of the longest completion to find
the column width when displaying completions.")

(defvar completion-abbrev-function nil
  "When non-nil a function that will be called on each string to be printed
in the completion list; it should return an abbreviated version of this
string if desirable.")

(defvar completion-hooks nil
  "List of functions called to complete a word. Each function is called as
(FUNCTION WORD [WORD-START WORD-END]) and should return a list of all
matching strings.")
(make-variable-buffer-local 'completion-hooks)

(defvar completion-fold-case nil)

;; t when the view displaying this buffer was created specially
(defvar completion-deletable-view nil)
(make-variable-buffer-local 'completion-deletable-view)

(defun completion-find-view ()
  (catch 'return
    (for-each (lambda (v)
		(and (string-match "^\\*completions\\*"
				   (buffer-name (current-buffer v)))
		     (throw 'return v)))
	      (window-view-list))
    nil))

;; Return a view visible in the current window that is used to display
;; completion lists (in its current buffer)
(defun completion-setup-view ()
  (or (completion-find-view)
      ;; No existing view has a *completions* buffer. Make one
      (let
	  ((new-view (and (= (window-view-count) 2) (split-view))))
	(with-view (or new-view (if (minibuffer-view-p (next-view))
				    (previous-view)
				  (next-view)))
	  (goto-buffer (make-buffer "*completions*"))
	  (set! completion-deletable-view new-view)
	  (current-view)))))

;; Try to remove any completion buffer, and if a view was created specially
;; the view itself
(defun completion-remove-view ()
  (let
      ((view (completion-find-view)))
    (when view
      (when (prog1
		(with-view view completion-deletable-view)
	      (kill-buffer (current-buffer view)))
	;; Ok to remove this view
	(delete-view view)))))

;; Display a list of completions in a *completions* buffer in the
;; current window
(defun completion-list (completions)
  (set! completions ((if completion-sorted-lists sort! identity)
		     (if completion-abbrev-function
			 (mapcar completion-abbrev-function
				 completions)
		       (copy-sequence completions))))
  (let*
      ((max-width 0)
       column-width columns
       (view (completion-setup-view))
       (view-width (car (view-dimensions view))))
    (for-each (lambda (c)
		(when (> (string-length c) max-width)
		  (set! max-width (string-length c))))
	      completions)
    (if (= max-width view-width)
	(progn
	  (set! columns 1)
	  (set! column-width view-width))
      (set! columns (max 1 (quotient view-width
				     (+ max-width completion-column-extra))))
      (set! column-width (quotient view-width columns)))
    (with-view view
      (let
	  ((column 0))
	(clear-buffer)
	(insert "Completions:\n\n")
	(while completions
	  (indent-to (* column column-width))
	  (insert (car completions))
	  (set! column (remainder (1+ column) columns))
	  (when (zero? column)
	    (insert "\n"))
	  (set! completions (cdr completions)))
	(goto (start-of-buffer))
	(when completion-deletable-view
	  (shrink-view-if-larger-than-buffer))))))

(defun completion-insert (completions word #!optional only-display)
  (let
      ((count (list-length completions))
       (w-start (forward-char (- (string-length word)))))
    (if (zero? count)
	(progn
	  (completion-remove-view)
	  (message "[No completions!]"))
      (if (and (not only-display) (= count 1))
	  (progn
	    (goto (replace-string word (car completions) w-start))
	    (completion-remove-view)
	    (message "[Unique completion]"))
	(unless only-display
	  (goto (replace-string word
				(complete-string word completions
						 completion-fold-case)
				w-start)))
	(completion-list completions)
	(message (format nil "[%d completion(s)]" count))))))


;; Completion commands

(defun complete-from-buffer (word)
  (let
      ((point (start-of-buffer))
       completions tem)
    (while (search-forward word point nil completion-fold-case)
      (set! point (match-end))
      (unless (equal? point (cursor-pos))
	(let ((start (match-start))
	      (end (forward-exp 1 (forward-char -1 point))))
	  (unless (/= (pos-line start) (pos-line end))
	    (set! tem (copy-area start end))
	    (unless (member tem completions)
	      (set! completions (cons tem completions)))))))
    completions))

(variable-set-default! 'completion-hooks
	      (append! completion-hooks (list complete-from-buffer)))

(defun complete-at-point (#!optional only-display)
  "Complete the word immediately before the cursor. If ONLY-DISPLAY is non-nil,
don't insert anything, just display the list of possible completions."
  (interactive "P")
  (let*
      ((w-start (forward-exp -1))
       (w-end (cursor-pos))
       (word (copy-area (forward-exp -1) (cursor-pos)))
       (completions (sort!
		     (apply append! (map (lambda (h)
					   (h word w-start w-end))
					 completion-hooks)))))
    ;; remove duplicates
    (when completions
      (while (and (cdr completions)
		  (equal? (car completions) (car (cdr completions))))
	(set! completions (cdr completions)))
      (when completions
	(let
	    ((tem completions))
	  (while (pair? (cdr tem))
	    (if (equal? (car tem) (car (cdr tem)))
		(set-cdr! tem (cdr (cdr tem)))
	      (set! tem (cdr tem)))))))
    (completion-insert completions word only-display)))

(defun show-completions ()
  "Display all words in the current buffer matching the word immediately before
the cursor."
  (interactive)
  (complete-at-point t))
