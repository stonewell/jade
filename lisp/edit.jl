;;;; edit.jl -- High-level editing functions
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

(require 'ring)

(defvar word-regexp "[a-zA-Z0-9]"
  "Regular expression which defines a character in a word.")
(defvar word-not-regexp "[^a-zA-Z0-9]|$"
  "Regular expression which defines anything that is not in a word.")
(defvar paragraph-regexp "^[\t ]*$"
  "Regular expression which matches a paragraph-separating piece of text.")

(make-variable-buffer-local 'word-regexp)
(make-variable-buffer-local 'word-not-regexp)
(make-variable-buffer-local 'paragraph-regexp)

(defvar toggle-read-only-function nil
  "May contain function to call when toggling a buffer between read-only
and writable.")
(make-variable-buffer-local 'toggle-read-only-function)


(defvar auto-mark (make-mark)
  "Mark which some commands use to track the previous cursor position.")


;; Words

(defun forward-word (&optional number pos move)
  "Return the position of first character after the end of this word.
NUMBER is the number of words to move, negative values mean go backwards.
If MOVE is t then the cursor is moved to the result."
  (interactive "p\n\nt")
  (unless number
    (setq number 1))
  (unless pos
    (setq pos (cursor-pos)))
  (cond
    ((< number 0)
      ;; go backwards
      (while (/= number 0)
	(setq pos (prev-char 1 pos))
	(when (looking-at word-not-regexp pos)
	  ;; not in word
	  (unless (setq pos (find-prev-regexp word-regexp pos))
	    (setq pos (buffer-start))))
	;; in middle of word
	(unless (setq pos (find-prev-regexp word-not-regexp pos))
	  (setq pos (buffer-start)))
	(setq
	  pos (find-next-regexp word-regexp pos)
	  number (1+ number))))
    (t
      ;; forwards
      (while (/= number 0)
	(when (looking-at word-not-regexp pos)
	  ;; already at end of a word
	  (unless (setq pos (find-next-regexp word-regexp pos))
	    (setq pos (buffer-end))))
	(unless (setq pos (find-next-regexp word-not-regexp pos))
	  (setq pos (buffer-end)))
	(setq number (1- number)))))
  (when move
    (goto-char pos))
  pos)

(defun backward-word (&optional number pos move)
  "Basically `(forward-word -NUMBER POS MOVE)'"
  (interactive "p\n\nt")
  (forward-word (if number (- number) -1) pos move))

(defun kill-word (count)
  "Kills from the cursor to the end of the word."
  (interactive "p")
  (kill-area (cursor-pos) (forward-word count)))

(defun backward-kill-word (count)
  "Kills from the start of the word to the cursor."
  (interactive "p")
  (kill-area (forward-word (- count)) (cursor-pos)))

(defun word-start (&optional pos)
  "Returns the position of the start of *this* word."
  (when (looking-at word-regexp pos)
    (if (find-prev-regexp word-not-regexp pos)
	(find-next-regexp word-regexp (match-end))
      (find-next-regexp word-regexp (buffer-start)))))

(defun in-word-p (&optional pos)
  "Returns t if POS is inside a word."
  (when (looking-at word-regexp pos)
    t))

(defun mark-word (count &optional pos)
  "Marks COUNT words from POS."
  (interactive "p")
  (set-rect-blocks nil nil)
  (mark-block (or pos (cursor-pos)) (forward-word count pos)))

(defun transpose-words (count)
  "Move the word at (before) the cursor COUNT words forwards."
  (interactive "p")
  (transpose-items 'forward-word 'backward-word count))


;; Paragraphs

(defun forward-paragraph (&optional pos buf move)
  "Returns the position of the start of the next paragraph. If MOVE
is t then the cursor is set to this position."
  (interactive "\n\nt")
  (setq pos (or (find-next-regexp paragraph-regexp
				  (next-char 1 (if pos
						   (copy-pos pos) 
						 (cursor-pos)))
				  buf)
		(buffer-end)))
  (when move
    (goto-char pos))
  pos)

(defun backward-paragraph (&optional pos buf move)
  "Returns the start of the previous paragraph. If MOVE is t the cursor is
set to this position."
  (interactive "\n\nt")
  (setq pos (or (find-prev-regexp paragraph-regexp
				  (prev-char 1 (if pos
						   (copy-pos pos)
						 (cursor-pos)))
				  buf)
		(buffer-start)))
  (when move
    (goto-char pos))
  pos)

(defun mark-paragraph ()
  "Set the block-marks to the current paragraph."
  (interactive)
  (let
      ((par (forward-paragraph)))
    (set-rect-blocks nil nil)
    (mark-block (backward-paragraph par) par)))


;; Block handling

(defun x11-block-status-function (status)
  (if status
      (x11-set-selection 'xa-primary (block-start) (block-end))
    (x11-lose-selection 'xa-primary)))
(when (x11-p)
  (add-hook 'block-status-hook 'x11-block-status-function))

(defun copy-block (&aux rc)
  "If a block is marked in the current window, return the text it contains and
unmark the block."
  (when (blockp)
    (setq rc (funcall (if (rect-blocks-p) 'copy-rect 'copy-area)
		      (block-start) (block-end)))
    (block-kill))
  rc)

(defun cut-block (&aux rc)
  "Similar to `copy-block' except the block is cut (copied then deleted) from
the buffer."
  (when (blockp)
    (setq rc (funcall (if (rect-blocks-p) 'cut-rect 'cut-area)
		      (block-start) (block-end)))
    (block-kill))
  rc)

(defun delete-block ()
  "Deletes the block marked in the current window (if one exists)."
  (interactive)
  (when (blockp)
    (funcall (if (rect-blocks-p) 'delete-rect 'delete-area)
	     (block-start) (block-end))
    (block-kill)))

(defun insert-block (&optional pos)
  "If a block is marked in the current window, copy it to position POS, then
unmark the block."
  (interactive)
  (when (blockp)
    (if (rect-blocks-p)
	(insert-rect (copy-rect (block-start) (block-end)) pos)
      (insert (copy-area (block-start) (block-end)) pos))
    (block-kill)))

(defun toggle-rect-blocks ()
  "Toggles the state of the flag saying whether blocks in this window are
marked sequentially (the default) or as rectangles."
  (interactive)
  (set-rect-blocks nil (not (rect-blocks-p))))

(defun kill-block ()
  "Kills the block marked in this window."
  (interactive)
  (kill-string (cut-block)))

(defun copy-block-as-kill ()
  "Kills the block marked in this window but doesn't actually delete it from
the buffer."
  (interactive)
  (kill-string (copy-block)))

(defun mark-block (start end)
  "Mark a block from START to END. This does an extra redraw if there's already
a block marked to save lots of flicker."
  (if (blockp)
      (progn
	(block-kill)
	;; Cunning hack -- the refresh algorithm(?) doesn't like the block
	;; killed then reset in one go, the whole screen is redraw :-( So
	;; do two refreshes...
	(refresh-all))
    (block-kill))
  (block-start start)
  (block-end end))

(defun mark-whole-buffer ()
  "Mark a block containing the whole of the buffer."
  (interactive)
  (set-rect-blocks nil nil)
  (mark-block (buffer-start) (buffer-end)))


(defun upcase-area (start end &optional buffer)
  "Makes all alpha characters in the specified region of text upper-case."
  (interactive "-m\nM")
  (translate-area start end upcase-table buffer))

(defun downcase-area (start end &optional buffer)
  "Makes all alpha characters in the specified region of text lower-case."
  (interactive "-m\nM")
  (translate-area start end downcase-table buffer))

(defun upcase-word (count)
  "Makes the next COUNT words from the cursor upper-case."
  (interactive "p")
  (let
      ((pos (forward-word count)))
    (upcase-area (cursor-pos) pos)
    (goto-char pos)))

(defun capitalize-word ()
  "The first character of this word (the one under the cursor) is made
upper-case, the rest lower-case."
  (interactive)
  (unless (in-word-p)
    (goto-char (find-next-regexp word-regexp)))
  (translate-area (cursor-pos) (next-char) upcase-table)
  (goto-next-char)
  (when (in-word-p)
    (downcase-word 1)))

(defun downcase-word (count)
  "Makes the word under the cursor lower case."
  (interactive "p")
  (let
      ((pos (forward-word count)))
    (downcase-area (cursor-pos) pos)
    (goto-char pos)))


(defun mark-region ()
  "Sets the block-marks to the area between the cursor position and the
auto-mark"
  (interactive)
  (block-kill)
  (when (eq (mark-file auto-mark) (current-buffer))
    (let
	((curs (cursor-pos)))
      (cond
       ((> curs (mark-pos auto-mark))
	(mark-block (mark-pos auto-mark) curs))
       (t
	(mark-block curs (mark-pos auto-mark)))))))


;; Killing

(defvar kill-ring (make-ring 32)
  "The ring buffer containing killed strings.")

(defun kill-string (string)
  "Adds STRING to the kill storage. If the last command also kill'ed something
the string is appended to."
  (when (stringp string)
    (if (eq last-command 'kill)
	(set-ring-head kill-ring (concat (killed-string) string))
      (add-to-ring kill-ring string)))
  ;; this command did some killing
  (setq this-command 'kill)
  string)

(defun killed-string (&optional depth)
  "Returns the string in the kill-buffer at position DEPTH. Currently only one
string is stored so DEPTH must be zero or not specified."
  (unless (numberp depth)
    (setq depth 0))
  (get-from-ring kill-ring (1+ depth)))

(defun kill-area (start end)
  "Kills a region of text in the current buffer from START to END."
  (interactive "-m\nM")
  (kill-string (cut-area start end)))

(defun copy-area-as-kill (start end)
  "Copies a region of text in the current buffer (from START to END) to the
kill storage."
  (interactive "-m\nM")
  (kill-string (copy-area start end)))

(defun kill-line (&optional arg)
  "If the cursor is not at the end of the line kill the text from the cursor
to the end of the line, else kill from the end of the line to the start of
the next line."
  (interactive "P")
  (let
      ((count (prefix-numeric-argument arg))
       (start (cursor-pos))
       end)
    (cond
     ((null arg)
      (setq end (if (>= start (line-end))
		    (line-start (next-line))
		  (line-end))))
     ((> count 0)
      (setq end (line-start (next-line count))))
     (t
      (setq end start
	    start (line-start (next-line count)))))
    (kill-area start end)))

(defun kill-whole-line (count)
  "Kill the whole of the current line."
  (interactive "p")
  (kill-area (line-start) (line-start (next-line count))))

(defun backward-kill-line ()
  "Kill from the cursor to the start of the line."
  (interactive)
  (kill-area (if (zerop (pos-col (cursor-pos)))
		 (prev-char)
	       (line-start))
	     (cursor-pos)))


;; Yank

;; Last item in the kill-ring that was yanked
(defvar yank-last-item nil)

;; Start and end of the last yank insertion
(defvar yank-last-start nil)
(defvar yank-last-end nil)

(defun yank (&optional dont-yank-block)
  "Inserts text before the cursor. If a block is marked in the current buffer
and DONT-YANK-BLOCK is nil insert the text in the block. Else yank the last
killed text."
  (interactive "P")
  (if (and (null dont-yank-block)
	   (or (blockp) (and (x11-p) (x11-selection-active-p 'xa-primary))))
      (progn
	(let
	    ((string (if (blockp)
			 (copy-block)
		       (x11-get-selection 'xa-primary))))
	  (insert string))
	(setq yank-last-item nil))
    (setq yank-last-item 0
	  yank-last-start (cursor-pos)
	  yank-last-end (insert (killed-string))
	  this-command 'yank)))

(defun yank-rectangle (&optional dont-yank-block)
  "Similar to `yank' except that the inserted text is treated as a rectangle."
  (interactive "P")
  (if (and (null dont-yank-block)
	   (or (blockp) (and (x11-p) (x11-selection-active-p 'xa-primary))))
      (let
	  ((string (if (blockp)
		       (copy-block)
		     (x11-get-selection 'xa-primary))))
	(insert-rect string))))

(defun yank-next ()
  "If the last command was a yank, replace the yanked text with the next
string in the kill ring. Currently this doesn't work when the last command
yanked a rectangle of text."
  (interactive)
  (when (and (eq last-command 'yank)
	     yank-last-item
	     (< yank-last-item (1- (ring-size kill-ring))))
    (goto-char yank-last-start)
    (delete-area yank-last-start yank-last-end)
    (setq yank-last-end (insert-rect (killed-string (1+ yank-last-item)))
	  yank-last-item (1+ yank-last-item)
	  this-command 'yank)))

(defun yank-to-mouse ()
  "Calls `yank inserting at the current position of the mouse cursor. The
cursor is left at the end of the inserted text."
  (interactive)
  (goto-char (mouse-pos))
  (yank))


(defun transpose-items (forward-item backward-item count)
  "Transpose the areas defined by the functions FORWARD-ITEM and BACKWARD-
ITEM (in the style of `forward-word', `backward-word' etc).
COUNT is the number of items to drag the item at the cursor past.\n
What actually happens is that the item before the cursor is dragged forward
over the COUNT following items."
  (let
      (start1 start2 end1 end2)
    (while (> count 0)
      ;; go forwards
      (setq start1 (funcall backward-item 1)
	    end1 (funcall forward-item 1 (copy-pos start1))
	    end2 (funcall forward-item 1 (copy-pos end1))
	    start2 (funcall backward-item 1 (copy-pos end2)))
      (transpose-1)
      (setq count (1- count)))
    (while (< count 0)
      ;; go backwards
      (setq start1 (funcall backward-item 1)
	    end1 (funcall forward-item 1 (copy-pos start1))
	    start2 (funcall backward-item 1 (copy-pos start1))
	    end2 (funcall forward-item 1 (copy-pos start2)))
      (transpose-1)
      (setq count (1+ count)))))

(defun transpose-1 ()
  (let
      (text1 text2)
    (if (< start2 start1)
	(progn
	  (setq text1 (cut-area start1 end1)
		text2 (copy-area start2 end2))
	  (insert text2 start1)
	  (delete-area start2 end2)
	  (goto-char (insert text1 start2)))
      (setq text1 (copy-area start1 end1)
	    text2 (cut-area start2 end2))
      (goto-char (insert text1 start2))
      (delete-area start1 end1)
      (insert text2 start1))))


(defun abort-recursive-edit (&optional ret-val)
  "Exits the innermost recursive edit with a value of VALUE (or nil)."
  (interactive)
  (throw 'exit ret-val))

(defun top-level ()
  "Abort all recursive-edits."
  (interactive)
  (throw 'top-level nil))


;; Overwrite mode

(defvar overwrite-mode-p nil
  "Non-nil when overwrite-mode is enabled.")
(make-variable-buffer-local 'overwrite-mode-p)

(defun overwrite-mode ()
  "Minor mode to toggle overwrite."
  (interactive)
  (if overwrite-mode-p
      (progn
	(setq overwrite-mode-p nil)
	(remove-minor-mode 'overwrite-mode "Overwrite")
	(remove-hook 'unbound-key-hook 'overwrite-insert))
    (add-minor-mode 'overwrite-mode "Overwrite")
    (setq overwrite-mode-p t)
    (add-hook 'unbound-key-hook 'overwrite-insert)))

(defun overwrite-insert (&optional str)
  (unless str
    (setq str (current-event-string)))
  (when str
    (setq len (length str))
    (delete-area (cursor-pos) (right-char len))
    (insert str)))


;; Miscellaneous editing commands

(defun backspace-char (count)
  "Delete COUNT characters preceding the cursor, if the cursor is past the
end of the line simply move COUNT characters to the left."
  (interactive "p")
  (let
      ((start (prev-char count)))
    (if (> (cursor-pos) (line-end))
	(if (> start (line-end))
	    (goto-char start)
	  (goto-line-end)
	  (delete-area start (cursor-pos)))
      (delete-area start (cursor-pos)))))
  
(defun delete-char (count)
  "Delete the character under the cursor."
  (interactive "p")
  (delete-area (cursor-pos) (next-char count)))

(defun tab-with-spaces ()
  "Insert enough spaces before the cursor to move it to the next tab position."
  (interactive)
  (indent-to (pos-col (next-tab)) t))

(defun just-spaces (count)
  "Ensure that there are only COUNT spaces around the cursor."
  (interactive "p")
  (when (member (get-char) '(?\  ?\t))
    (let
	((pos (find-prev-regexp "[^\t ]|^")))
      (when pos
	(unless (zerop (pos-col pos))
	  (next-char 1 pos))
	(when (and pos (looking-at "[\t ]+" pos))
	  (delete-area (match-start) (match-end))
	  (goto-char (match-start))))))
  (unless (zerop count)
    (insert (make-string count ?\ ))))

(defun no-spaces ()
  "Delete all space and tab characters surrounding the cursor."
  (interactive)
  (just-spaces 0))

(defun open-line (count)
  "Break the current line creating COUNT new lines, leaving the cursor in
its original position."
  (interactive "p")
  (let
      ((opos (cursor-pos)))
    (insert (make-string count ?\n))
    (goto-char opos)))

(defun transpose-chars (count)
  "Move the character before the cursor COUNT characters forwards."
  (interactive "p")
  (transpose-items 'next-char 'prev-char count))

(defun toggle-buffer-read-only ()
  "Toggle the current buffer between being writable and read-only."
  (interactive)
  (if toggle-read-only-function
      (funcall toggle-read-only-function)
    (set-buffer-read-only nil (not (buffer-read-only-p)))))
