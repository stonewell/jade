;;;; texinfo-mode.jl -- Mode for editing Texinfo files
;;;  Copyright (C) 1993, 1994 John Harper <john@dcs.warwick.ac.uk>
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

(provide 'texinfo-mode)

(unless (boundp 'texinfo-keymap)
  (setq texinfo-keymap (make-keylist)
	texinfo-ctrl-c-keymap (make-keylist)
	texinfo-ctrl-c-ctrl-c-keymap (make-keylist))
  (bind-keys texinfo-keymap
    "TAB" 'tab-with-spaces)
  (bind-keys texinfo-ctrl-c-keymap
    "Ctrl-c" '(setq next-keymap-path '(texinfo-ctrl-c-ctrl-c-keymap)))
  (bind-keys texinfo-ctrl-c-ctrl-c-keymap
    "c" '(texinfo-insert-braces "@code")
    "d" '(texinfo-insert-braces "@dfn")
    "e" 'texinfo-insert-@end
    "f" '(texinfo-insert-braces "@file")
    "i" '(texinfo-insert "@item")
    "l" '(texinfo-insert "@lisp\n")
    "m" '(texinfo-insert "@menu\n")
    "Ctrl-m" 'texinfo-insert-menu-item
    "n" 'texinfo-insert-@node
    "s" '(texinfo-insert-braces "@samp")
    "v" '(texinfo-insert-braces "@var")
    "{" 'texinfo-insert-braces
    "]" 'texinfo-move-over-braces
    "}" 'texinfo-move-over-braces))

;;;###autoload
(defun texinfo-mode ()
  "Texinfo Mode:\n
Major mode for editing Texinfo source files.\n
Special commands available are,\n
  `Ctrl-c Ctrl-c c'	Insert `@code'
  `Ctrl-c Ctrl-c d'	Insert `@dfn'
  `Ctrl-c Ctrl-c e'	Insert the correct `@end' command for the current
			context.
  `Ctrl-c Ctrl-c f'	Insert `@file'
  `Ctrl-c Ctrl-c i'	Insert `@item'
  `Ctrl-c Ctrl-c l'	Insert `@lisp'
  `Ctrl-c Ctrl-c m'	Insert `@menu'
  `Ctrl-c Ctrl-c Ctrl-m' Insert a menu item.
  `Ctrl-c Ctrl-c n'	Prompt for each part of an `@node' line and insert
			the constructed line.
  `Ctrl-c Ctrl-c s'	Insert `@samp'
  `Ctrl-c Ctrl-c v'	Insert `@var'
  `Ctrl-c Ctrl-c {'	Insert a pair of braces and place the cursor between
			them.
  `Ctrl-c Ctrl-c }',
  `Ctrl-c Ctrl-c ]'	Move the cursor to the character after the next closing
			brace."
  (interactive)
  (when major-mode-kill
    (funcall major-mode-kill (current-buffer)))
  (setq mode-name "Texinfo"
	major-mode 'texinfo-mode
	major-mode-kill 'texinfo-mode-kill
	ctrl-c-keymap texinfo-ctrl-c-keymap
	paragraph-regexp "^@node"
	keymap-path (cons 'texinfo-keymap keymap-path))
  (eval-hook 'text-mode-hook)
  (eval-hook 'texinfo-mode-hook))

(defun texinfo-mode-kill ()
  (setq mode-name nil
	major-mode nil
	major-mode-kill nil
	keymap-path (delq 'texinfo-keymap keymap-path)))

(defun texinfo-insert-@end ()
  (interactive)
  (let
      ((pos (line-end (prev-line)))
       (depth 0))
    (if (catch 'foo
	  (while (find-prev-regexp "^@(end |)(cartouche|deffn|defun|defmac\
|defspec|defvr|defvar|defopt|deftypefn|deftypefun|deftypevr|deftypevar|defcv\
|defivar|defop|defmethod|deftp|display|enumerate|example|flushleft|flushright\
|format|ftable|group|ifclear|ifinfo|ifset|iftex|ignore|itemize|lisp|menu\
|quotation|smallexample|smalllisp|table|tex|titlepage|vtable)" pos)
	    (if (equal (match-start 1) (match-end 1))
		;; no end
		(if (zerop depth)
		    (throw 'foo t)
		  (setq depth (1- depth)))
	      (setq depth (1+ depth)))
	    (setq pos (prev-char 1 (match-start)))))
	(format (current-buffer) "@end %s\n" (copy-area (match-start 2)
							(match-end 2)))
      (insert "@end ")
      (error "Can't find a command to @end"))))

(defun texinfo-insert (string)
  (insert string))

(defun texinfo-insert-@node ()
  (interactive)
  (insert "@node ")
  (let
      ((tmp (prompt "Node name: ")))
    (when tmp
      (insert tmp)
      (when (setq tmp (prompt "Next node: "))
	(insert ", ")
	(insert tmp)
	(when (setq tmp (prompt "Previous node: "))
	  (insert ", ")
	  (insert tmp)
	  (when (setq tmp (prompt "Up node: "))
	    (insert ", ")
	    (insert tmp)))))))

(defun texinfo-insert-braces (&optional command)
  (interactive)
  (when command
    (insert command))
  (if current-prefix-arg
      (progn
	;; Surround n words with braces
	(insert "{")
	(forward-word (prefix-numeric-argument current-prefix-arg) nil t)
	(insert "}"))
    (insert "{}")
    (goto-prev-char)))

(defun texinfo-move-over-braces ()
  (interactive)
  (goto-char (next-char 1 (find-next-char ?}))))

(defun texinfo-insert-menu-item ()
  (interactive)
  (let
      ((tmp (prompt "Item node: ")))
    (when tmp
      (insert (concat "* " tmp "::")))))
