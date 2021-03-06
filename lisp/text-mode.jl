;;;; text-mode.jl -- Modes for editing English text
;;;  Copyright (C) 1994 John Harper <john@dcs.warwick.ac.uk>
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

(eval-when-compile (require 'fill))
(provide 'text-mode)


;; Basic text mode

(defvar text-mode-keymap
  (bind-keys (make-sparse-keymap)
    "Meta-s" 'center-line
    "Meta-S" 'center-paragraph
    "Meta-=" 'word-count-area))

(defvar text-mode-indent-keymap
  (bind-keys (make-sparse-keymap text-mode-keymap)
    "TAB" 'text-mode-indent-tab))

(defun text-mode-init ()
  (set! major-mode-kill text-mode-kill)
  (set! word-regexp "[a-zA-Z0-9]")
  (set! word-not-regexp "[^a-zA-Z0-9]|$"))

;;;###autoload
(defun text-mode ()
  "Mode for editing English text in. Local bindings in this mode are:\n
\\{text-mode-keymap}"
  (interactive)
  (when major-mode-kill
    (major-mode-kill (current-buffer)))
  (set! mode-name "Text")
  (set! major-mode 'text-mode)
  (set! local-keymap 'text-mode-keymap)
  (text-mode-init)
  (call-hook 'text-mode-hook))

;;;###autoload
(defun indented-text-mode ()
  "Variant of `text-mode' in which the TAB key indents to the depth of the
previous line, then works as normal. Local bindings in this mode are:\n
\\{text-mode-keymap}"
  (interactive)
  (when major-mode-kill
    (major-mode-kill (current-buffer)))
  (set! mode-name "Indented Text")
  (set! major-mode 'indented-text-mode)
  (set! local-keymap 'text-mode-indent-keymap)
  (make-local-variable 'fill-prefix)
  (set! fill-prefix text-mode-fill-prefix)
  (text-mode-init)
  (call-hook 'text-mode-hook)
  (call-hook 'indented-text-mode-hook))

(defun text-mode-kill ()
  (set! mode-name nil)
  (set! local-keymap nil)
  (set! major-mode nil)
  (set! major-mode-kill nil)
  (when (and (variable-bound? 'fill-prefix) (function? fill-prefix))
    (set! fill-prefix nil))
  t)

(defun text-mode-indent-tab ()
  (interactive)
  (let
      ((p (re-search-backward "^.+$" (forward-line -1))))
    (if (or (null? p) (> (pos-col (cursor-pos)) (line-length p)))
	(indent-to-next-tab)
      (let
          ((gcurs (char-to-glyph-pos (cursor-pos))))
        (set! gcurs (pos (pos-col gcurs) (pos-line p)))
	(set! p (glyph-to-char-pos gcurs))
	(re-search-forward "[\t ]+|$" p)
	(if (equal? (match-end) (end-of-line p))
	    (indent-to-next-tab)
	  (set! p (pos (pos-col (char-to-glyph-pos (match-end)))
		       (pos-line (cursor-pos))))
	  (if (empty-line-p p)
	      (set-indent-pos p)
	    (indent-to (pos-col p))))))))

(defun text-mode-fill-prefix (op p)
  (let
      ((get-indent (lambda ()
		     (if (zero? (pos-line p))
			 0
		       (set! p (forward-line -1 p))
		       (while (and (not (zero? (pos-line p)))
				   (not (looking-at "^[ \t\f]*[^ \t\f\n]+" p)))
			 (set! p (forward-line -1 p)))
		       (pos-col (indent-pos p))))))
    (cond
     ((eq? op 'insert)
      (save-excursion
	(goto p)
	(indent-to (get-indent p))))
     ((eq? op 'delete)
      (when (looking-at "^[\t ]+" p)
	(delete-area (match-start) (match-end))))
     ((eq? op 'width)
      (get-indent)))))


;; Misc

;;;###autoload
(defun word-count-area (start end #!optional do-print)
  "Return the number of words in the area of text between START and END in
the current buffer. If PRINT is non-nil this number is also displayed in the
status line."
  (interactive "-m\nM\nt")
  (let
      ((tmp start)
       (count 0))
    ;; Catch the end-of-buffer error
    (condition-case nil
	(while (<= tmp end)
	  (set! count (1+ count))
	  (set! tmp (forward-word 1 tmp)))
      (error))
    (when do-print
      (prin1 count t))
    count))
