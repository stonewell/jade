;;;; ispell.jl -- Interface with ispell(1)
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

(provide 'ispell)


;; Configuration and variables

(defvar ispell-program "ispell"
  "Filename of program used to start ispell(1).")

(defvar ispell-options nil
  "List of options to pass to ispell")

(defvar ispell-timeout 60
  "Seconds to wait for ispell output before giving up.")

(defvar ispell-word-re "[a-zA-Z]")

(defvar ispell-not-word-re "[^a-zA-Z]")

(defvar ispell-echo-output nil
  "Use for debugging only.")

(defvar ispell-process nil
  "Subprocess that ispell is running in, or nil if ispell isn't running.")

(defvar ispell-id-string nil
  "String sent by ispell identifying itself when it started executing.")

(defvar ispell-pending-output nil
  "String of output received from ispell but not processed.")

(defvar ispell-line-callback nil
  "Function to call asynchronously with a single line of output from ispell.")

(defvar ispell-prompt-buffer nil)
(defvar ispell-options-buffer nil)

(defvar ispell-keymap (bind-keys (make-sparse-keymap)
			"SPC" 'ispell-accept
			"0" '(ispell-replace 0)
			"1" '(ispell-replace 1)
			"2" '(ispell-replace 2)
			"3" '(ispell-replace 3)
			"4" '(ispell-replace 4)
			"5" '(ispell-replace 5)
			"6" '(ispell-replace 6)
			"7" '(ispell-replace 7)
			"8" '(ispell-replace 8)
			"9" '(ispell-replace 9)
			"r" 'ispell-replace
			"a" 'ispell-accept-and-remember
			"i" 'ispell-accept-and-always-remember
			"l" 'ispell-lookup
			"u" 'ispell-accept-and-always-remember-uncap
			"q" 'ispell-quit
			"x" 'ispell-quit
			"ESC" 'ispell-quit
			"Ctrl-g" 'ispell-quit
			"?" 'ispell-help
			"HELP" 'ispell-help
			"Ctrl-h" 'ispell-help))


;; Code

;; Start the ispell-process if it isn't already
(defun ispell-start-process ()
  (unless ispell-process
    (let
	((process (make-process #'ispell-output-filter)))
      (set-process-function process 'ispell-sentinel)
      (apply 'start-process process ispell-program "-a" ispell-options)
      (setq ispell-process process)
      (setq ispell-id-string (ispell-read-line)))))

;; Kill the ispell process
(defun ispell-kill-process ()
  (interactive)
  (when ispell-process
    ;; FIXME: should really send EOF
    (interrupt-process ispell-process)
    (while (and (not (accept-process-output ispell-timeout)) ispell-process)
      (kill-process ispell-process))))

;; Function to buffer output from Ispell
(defun ispell-output-filter (output)
  (when (integerp output)
    (setq output (make-string 1 output)))
  (and ispell-echo-output
       (stringp output)
       (let
	   ((print-escape t))
	 (format (stderr-file) "Ispell: %S\n" output)))
  (setq ispell-pending-output (concat ispell-pending-output output))
  (while (and ispell-line-callback
	      ispell-pending-output
	      (string-match "\n" ispell-pending-output))
    (let
	((line (substring ispell-pending-output 0 (match-end))))
      (setq ispell-pending-output (substring ispell-pending-output
					     (match-end)))
      (funcall ispell-line-callback line))))

;; Called when Ispell exits
(defun ispell-sentinel ()
  (setq ispell-process nil)
  (setq ispell-id-string nil))

;; Read one whole line from the ispell-process (including newline)
(defun ispell-read-line ()
  (let*
      ((ispell-read-line-out nil)
       (ispell-line-callback #'(lambda (l)
				 (setq ispell-read-line-out l)
				 ;; Only want the first line
				 (setq ispell-line-callback nil))))
    ;; Flush any pending output
    (ispell-output-filter nil)
    (while (and (not ispell-read-line-out)
		ispell-process
		(not (accept-process-output ispell-timeout))))
    (or ispell-read-line-out
	(error "Ispell timed out waiting for output"))))

;;;###autoload
(defun ispell-region (start end)
  "Run Ispell interactively over the region of the current buffer from START
to END. Any misspelt words will result in the correct spelling being prompted
for. When called interactively, spell-check the current block."
  (interactive "-m\nM")
  (let
      (ispell-prompt-buffer
       ispell-options-buffer)
    (while (< start end)
      (let
	  (word-start word-end word)
	(setq word-start (re-search-forward ispell-word-re start))
	(if word-start
	    (progn
	      (setq word-end (or (re-search-forward ispell-not-word-re
						    word-start)
				 (end-of-buffer)))
	      (ispell-start-process)
	      (setq word (copy-area word-start word-end))
	      (write ispell-process word)
	      (write ispell-process ?\n)
	      (let
		  ((response (ispell-read-line)))
		(cond
		 ((eq (aref response 0) ?\n)
		  ;; This shouldn't happen
		  (error "Null output from Ispell"))
		 ((string-looking-at "^[*+-]" response)
		  ;; Word spelt ok
		  (setq start word-end)
		  ;; Take the following newline
		  (ispell-read-line))
		 (t
		  ;; Not ok
		  (setq start (ispell-handle-failure word response
						     word-start word-end))
		  (ispell-read-line)))))
	  ;; Can't find end of word
	  (setq start end))))
    (when ispell-options-buffer
      (kill-buffer ispell-options-buffer))))

;;;###autoload
(defun ispell-buffer ()
  "Run the ispell-region command over the entire buffer."
  (interactive)
  (ispell-region (start-of-buffer) (end-of-buffer)))

(defun ispell-handle-failure (word response start end)
  (let
      ((old-buffer (current-buffer))
       options)
    (when (string-looking-at "^[&?].*: " response)
      (let
	  ((point (match-end)))
	(while (string-looking-at " *([^,\n]+),?" response point)
	  (setq options (cons (substring response
					 (match-start 1) (match-end 1))
			      options))
	  (setq point (match-end)))
	(setq options (nreverse options))))
    (unless ispell-options-buffer
      (setq ispell-options-buffer (make-buffer "*Ispell-options*")))
    (with-buffer ispell-options-buffer
      (let
	  ((inhibit-read-only t))
	(clear-buffer)
	(insert ispell-id-string)
	(insert "\n")
	(insert (with-buffer old-buffer
		  (copy-area (start-of-line start) (end-of-line end))))
	(make-extent (pos (pos-col start) 2)
		     (pos (pos-col end) 2)
		     (list 'face highlight-face))
	(insert "\n\n")
	(if options
	    (let
		((i 0)
		 (tem options))
	      (while tem
		(format (current-buffer) "%2d: %s\n" i (car tem))
		(setq i (1+ i))
		(setq tem (cdr tem))))
	  (insert "[No options]\n"))
	(setq read-only t)))
    (unless ispell-prompt-buffer
      (setq ispell-prompt-buffer (make-buffer "*Ispell-prompt*"))
      (with-buffer ispell-prompt-buffer
	(insert "[SP] <number> R\)epl A\)ccept I\)nsert L\)ookup U\)ncap Q\)uit e\(X\)it or ? for help")
	(setq keymap-path (cons ispell-keymap keymap-path))
	(setq read-only t)))
    (with-view (other-view)
      (goto-buffer ispell-options-buffer)
      (shrink-view-if-larger-than-buffer))
    (with-view (minibuffer-view)
      (with-buffer ispell-prompt-buffer
	(let
	    ((done nil)
	     command)
	  (while (not done)
	    (setq command (catch 'ispell-exit
			    (recursive-edit)))
	    (cond
	     ((eq (car command) 'accept)
	      (when (cdr command)
		(write ispell-process (cdr command))
		(write ispell-process word)
		(write ispell-process ?\n))
	      (setq done t))
	     ((eq (car command) 'replace)
	      (setq done t)
	      (if (integerp (cdr command))
		  (with-buffer old-buffer
		    (setq end (replace-string word
					      (ispell-strip-word
					       (nth (cdr command) options))
					      start)))
		(let
		    ((string (prompt-for-string "Replace with:" word)))
		  (if string
		      (setq end (replace-string word string start))
		    (setq done nil)))))
	     ((eq (car command) 'quit)
	      (setq done t)
	      (setq end (end-of-buffer old-buffer)))
	     (t
	      (error "Unknown ispell command, %S" command)))))))
    end))

(defun ispell-strip-word (word)
  (let
      ((out "")
       (point 0))
    (when (string-looking-at (concat ?\( ispell-word-re "+\)\\+") word point)
      ;; [prefix+]
      (setq out (substring word (match-start 1) (match-end 1)))
      (setq point (match-end)))
    (when (string-looking-at (concat ?\( ispell-word-re "+\)[+-]") word point)
      ;; root
      (setq out (concat out (substring word (match-start 1) (match-end 1))))
      (setq point (match-end 1)))
    (while (string-looking-at (concat "-\(" ispell-word-re "+\)") word point)
      ;; [-prefix] | [-suffix]
      (let
	  ((tem (quote-regexp (substring word (match-start 1) (match-end 1)))))
	(setq point (match-end))
	(cond
	 ((string-match (concat tem ?$) out)
	  (setq out (substring out 0 (match-start))))
	 ((string-match (concat ?^ tem) out)
	  (setq out (substring out (match-end)))))))
    (when (string-looking-at (concat "\\+\(" ispell-word-re "+\)") word point)
      (setq out (concat out (substring word (match-start 1) (match-end 1)))))))

(defun ispell-accept ()
  (interactive)
  (throw 'ispell-exit '(accept)))

(defun ispell-accept-and-remember ()
  (interactive)
  (throw 'ispell-exit '(accept . ?@)))

(defun ispell-accept-and-always-remember ()
  (interactive)
  (throw 'ispell-exit '(accept . ?*)))

(defun ispell-accept-and-always-remember-uncap ()
  (interactive)
  (throw 'ispell-exit '(accept . ?&)))

(defun ispell-replace (arg)
  (interactive "P")
  (throw 'ispell-exit (cons 'replace
			    (if (consp arg)
				(prefix-numeric-argument arg)
			      arg))))

(defun ispell-quit ()
  (interactive)
  (throw 'ispell-exit '(quit)))