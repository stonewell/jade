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
(defvar word-not-regexp "[^a-zA-Z0-9]"
  "Regular expression which defines anything that is not in a word.")

(defvar paragraph-separate "^[\t\f\n ]*\n"
  "Regexp matching a paragraph-separating string. The character immediately
following the matched text is taken as the start of the following
paragraph.")

(defvar page-start "^\f"
  "Regular expression that matches the start of a page of text.")

(make-variable-buffer-local 'word-regexp)
(make-variable-buffer-local 'word-not-regexp)
(make-variable-buffer-local 'paragraph-separate)
(make-variable-buffer-local 'page-start)

(defvar toggle-read-only-function nil
  "May contain function to call when toggling a buffer between read-only
and writable.")
(make-variable-buffer-local 'toggle-read-only-function)

(defvar auto-mark nil
  "Buffer-local mark which some commands use to track the previous cursor
position.")
(make-variable-buffer-local 'auto-mark)


;; Marks

(defun goto-mark (mark &optional dont-set-auto)
  "Switches (if necessary) to the buffer containing MARK at the position
of the mark. If the file containing MARK is not in memory then we
attempt to load it by calling find-file. Unless DONT-SET-AUTO is non-nil
the auto-mark of the current buffer will be updated before the move."
  (when (markp mark)
    (let
	((file (mark-file mark))
	 (pos (mark-pos mark)))
      (unless dont-set-auto
	(set-auto-mark))
      (if (stringp file)
	  (find-file file)
	(goto-buffer file))
      (goto pos))))

(defun set-auto-mark (&optional pos)
  "Sets the mark `auto-mark' to POS (or the current position)."
  (interactive)
  (if auto-mark
      (progn
	(set-mark-pos auto-mark (or pos (cursor-pos)))
	(set-mark-file auto-mark (current-buffer)))
    (setq auto-mark (make-mark pos)))
  (message "Set auto-mark."))

(defun swap-cursor-and-auto-mark ()
  "Sets the `auto-mark' to the current position and then sets the current
position (buffer and cursor-pos) to the old value of `auto-mark'."
  (interactive)
  (unless auto-mark
    (error "The auto mark isn't set in this buffer."))
  (let
      ((a-m-file (mark-file auto-mark))
       (a-m-pos (mark-pos auto-mark)))
    (set-auto-mark)
    (if (stringp a-m-file)
	(find-file a-m-file)
      (goto-buffer a-m-file))
    (goto a-m-pos)))


;; Characters

(defun backward-char (&optional count pos buf move)
  (interactive "p\n\n\nt")
  (forward-char (if count (- count) -1) pos buf move))

(defun transpose-chars (count)
  "Move the character before the cursor COUNT characters forwards."
  (interactive "p")
  (transpose-items 'forward-char 'backward-char count))

(defun backward-tab (&optional count pos size move)
  (interactive "p\n\n\nt")
  (forward-tab (if count (- count) -1) pos size move))

(defmacro right-char (count pos)
  "Return the position COUNT characters to the right of POS."
  (list 'pos (list '+ (list 'pos-col pos) count)
	(list 'pos-line pos)))

(defmacro left-char (count pos)
  "Return the position COUNT characters to the left of POS."
  (list 'right-char (list '- count) pos))


;; Lines

(defun backward-line (&optional count pos move)
  (interactive "p\n\nt")
  (forward-line (if count (- count) -1) pos move))

(defun split-line (&optional count pos)
  "Insert COUNT newline characters before position POS (or before the
cursor)."
  (interactive "p")
  (insert (if (or (not count) (= count 1))
	      "\n"
	    (make-string count ?\n)) pos))

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
      (setq end (if (>= start (end-of-line))
		    (start-of-line (forward-line))
		  (end-of-line))))
     ((> count 0)
      (setq end (start-of-line (forward-line count))))
     (t
      (setq end start
	    start (start-of-line (forward-line count)))))
    (kill-area start end)))

(defun kill-whole-line (count)
  "Kill the whole of the current line."
  (interactive "p")
  (kill-area (start-of-line) (start-of-line (forward-line count))))

(defun backward-kill-line ()
  "Kill from the cursor to the start of the line."
  (interactive)
  (kill-area (if (zerop (pos-col (cursor-pos)))
		 (forward-char -1)
	       (start-of-line))
	     (cursor-pos)))

(defun goto-line (line)
  "Goto line number LINE. LINE counts from 1."
  (interactive "NLine: ")
  (set-auto-mark)
  (goto (pos nil (1- line))))


;; Words

(defun forward-word (&optional number pos move)
  "Return the position of first character after the end of this word.
NUMBER is the number of words to move, negative values mean go backwards.
If MOVE is t then the cursor is moved to the result."
  (interactive "p\n\nt")
  (unless number
    (setq number 1))
  (unless pos (setq pos (cursor-pos)))
  (cond
    ((< number 0)
      ;; go backwards
      (while (/= number 0)
	(setq pos (forward-char -1 pos))
	(when (looking-at word-not-regexp pos)
	  ;; not in word
	  (unless (setq pos (re-search-backward word-regexp pos))
	    (setq pos (start-of-buffer))))
	;; in middle of word
	(unless (setq pos (re-search-backward word-not-regexp pos))
	  (setq pos (start-of-buffer)))
	(setq
	  pos (re-search-forward word-regexp pos)
	  number (1+ number))))
    (t
      ;; forwards
      (while (/= number 0)
	(when (looking-at word-not-regexp pos)
	  ;; already at end of a word
	  (unless (setq pos (re-search-forward word-regexp pos))
	    (setq pos (end-of-buffer))))
	(unless (setq pos (re-search-forward word-not-regexp pos))
	  (setq pos (end-of-buffer)))
	(setq number (1- number)))))
  (when move
    (goto pos))
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
    (if (re-search-backward word-not-regexp pos)
	(re-search-forward word-regexp (match-end))
      (re-search-forward word-regexp (start-of-buffer)))))

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

(defun forward-paragraph (count &optional pos move)
  "Return the end of the COUNT'th paragraph. If MOVE is t, or the function
is called interactively, the cursor is set to this position."
  (interactive "p\n\nt")
  (unless pos
    (setq pos (cursor-pos)))
  ;; Positive arguments
  (while (and (> count 0)
	      (< pos (end-of-buffer)))
    ;; Skip any lines at POS matching the separator
    (while (and (< (pos-line pos) (buffer-length))
		(looking-at paragraph-separate pos))
      (setq pos (forward-line 1 pos)))
    ;; Search for the next separator
    (if (re-search-forward paragraph-separate (forward-char 1 pos))
	(setq count (1- count)
	      pos (if (zerop count) (match-start) (match-end)))
      (setq pos (end-of-buffer)
	      count 0)))
  ;; Negative arguments
  (while (and (< count 0)
	      (> pos (start-of-buffer)))
    ;; Search for the previous separator
    (if (re-search-backward paragraph-separate (backward-char 1 pos))
	(progn
	  ;; Check if we actually moved
	  (unless (>= (match-end) pos)
	    (setq count (1+ count)))
	  (setq pos (if (zerop count) (match-end) (match-start)))
	  ;; Skip lines above the current match, if they match
	  ;; the separator as well, skip them
	  (while (and (> (pos-line (match-start)) 0)
		      (looking-at paragraph-separate
				  (forward-line -1 (match-start))))
	    (setq pos (if (zerop count) (match-end) (match-start)))))
      (setq pos (start-of-buffer)
	    count 0)))
  (if move
      (goto pos)
    pos))

(defun backward-paragraph (count &optional pos move)
  "Returns the start of the COUNT'th previous paragraph. If MOVE is t, or the
function is called interactively, the cursor is set to this position."
  (interactive "p\n\nt")
  (forward-paragraph (- (or count 1)) pos move))

(defun paragraph-edges (count &optional pos mark)
  "Return (START . END), the positions defining the outermost characters of
the COUNT paragraphs around POS. Positive COUNTs mean to search forwards,
negative to search backwards.
When MARK is t, the block marks are set to START and END."
  (interactive "p\n\nt")
  (unless pos
    (setq pos (cursor-pos)))
  (let
      (start end)
    (if (> count 0)
	(setq start (forward-paragraph -1 (min (forward-char 1 pos)
					       (end-of-buffer)))
	      end (forward-paragraph count start))
      (setq end (forward-paragraph 1 (max (forward-char -1 pos)
					  (start-of-buffer)))
	    start (forward-paragraph count end)))
    (when mark
      (set-rect-blocks nil nil)
      (mark-block start end))
    (cons start end)))

(defun transpose-paragraphs (count)
  "Move the paragrah at (before) the cursor COUNT paragraphs forwards."
  (interactive "p")
  (transpose-items 'forward-paragraph 'backward-paragraph count))


;; Page handling

(defun forward-page (&optional count pos movep)
  "Return the position COUNT pages forwards. If COUNT is negative, go
backwards. If MOVEP is non-nil move the cursor to the position."
  (interactive "p\n\nt")
  (unless count
    (setq count 1))
  (if (> count 0)
      (progn
	(when (looking-at page-start pos)
	  (setq pos (match-end)))
	(while (and (> count 0) (re-search-forward page-start pos))
	  (setq pos (match-end)
		count (1- count)))
	(when (= count 1)
	  (setq pos (end-of-buffer)
		count 0)))
    (when (looking-at page-start (start-of-line pos))
      (setq pos (forward-line -1 pos)))
    (while (and (< count 0) (re-search-backward page-start pos))
      (setq pos (if (= count -1)
		    (match-end)
		  (forward-char -1 (match-start)))
	    count (1+ count)))
    (when (= count -1)
      (setq pos (start-of-buffer)
	    count 0)))
  (if (zerop count)
      (when movep (goto pos))
    (error (if (> count 0) "End of buffer" "Start of buffer")))
  pos)

(defun backward-page (&optional count pos movep)
  "Basically (forward-page (- COUNT) POS MOVEP)."
  (interactive "p\n\nt")
  (forward-page (- (or count 1)) pos movep))

(defun mark-page ()
  "Set the block to mark the current page of text."
  (interactive)
  (let*
      ((end (forward-page 1))
       (start (backward-page 1 end)))
    (set-rect-blocks nil nil)
    (mark-block start end)))

(defun restrict-to-page ()
  "Restrict the buffer to the current page of text."
  (interactive)
  (let*
      ((end (forward-page 1))
       (start (backward-page 1 end)))
    (restrict-buffer start end)))


;; Block handling

(defun x11-block-status-function ()
  (if (blockp)
      (x11-set-selection 'xa-primary (block-start) (block-end))
    (x11-lose-selection 'xa-primary)))
(when (x11-p)
  (add-hook 'block-status-hook 'x11-block-status-function))

(defun copy-block (&aux rc)
  "If a block is marked in the current window, return the text it contains and
unmark the block."
  (when (blockp)
    (setq rc (funcall (if (rect-blocks-p) 'copy-rectangle 'copy-area)
		      (block-start) (block-end)))
    (block-kill))
  rc)

(defun cut-block (&aux rc)
  "Similar to `copy-block' except the block is cut (copied then deleted) from
the buffer."
  (when (blockp)
    (setq rc (funcall (if (rect-blocks-p) 'cut-rectangle 'cut-area)
		      (block-start) (block-end)))
    (block-kill))
  rc)

(defun delete-block ()
  "Deletes the block marked in the current window (if one exists)."
  (interactive)
  (when (blockp)
    (funcall (if (rect-blocks-p) 'delete-rectangle 'delete-area)
	     (block-start) (block-end))
    (block-kill)))

(defun insert-block (&optional pos)
  "If a block is marked in the current window, copy it to position POS, then
unmark the block."
  (interactive)
  (when (blockp)
    (if (rect-blocks-p)
	(insert-rectangle (copy-rectangle (block-start) (block-end)) pos)
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
  "Mark a block from START to END."
  (block-kill)
  (block-start start)
  (block-end end))

(defun mark-whole-buffer ()
  "Mark a block containing the whole of the buffer."
  (interactive)
  (set-rect-blocks nil nil)
  (mark-block (start-of-buffer) (end-of-buffer)))


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
    (goto pos)))

(defun capitalize-word (count)
  "The first character of the COUNT'th next word is made upper-case, the
rest lower-case."
  (interactive "p")
  (forward-word (if (> count 0) (- count 1) count) nil t)
  (unless (in-word-p)
    (goto (re-search-forward word-regexp)))
  (translate-area (cursor-pos) (forward-char) upcase-table)
  (goto (forward-char))
  (when (in-word-p)
    (downcase-word 1)))

(defun downcase-word (count)
  "Makes the word under the cursor lower case."
  (interactive "p")
  (let
      ((pos (forward-word count)))
    (downcase-area (cursor-pos) pos)
    (goto pos)))


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
  (when (and (stringp string) (not (string= string "")))
    (if (eq last-command 'kill)
	(set-ring-head kill-ring (concat (killed-string) string))
      (add-to-ring kill-ring string))
    (call-hook 'after-kill-hook))
  ;; this command did some killing
  (setq this-command 'kill)
  string)

(when (x11-p)
  (add-hook 'after-kill-hook
	    #'(lambda ()
		(x11-set-selection 'xa-primary (killed-string)))))
							       
(defun killed-string (&optional depth)
  "Returns the string in the kill-buffer at position DEPTH."
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
	   ;; If a block is marked use that, otherwise (in X11) look for a
	   ;; selection not owned by us.
	   (or (blockp) (and (x11-p) (x11-selection-active-p 'xa-primary)
			     (not (x11-own-selection-p 'xa-primary)))))
      (progn
	(let
	    ((string (if (blockp)
			 (copy-block)
		       (x11-get-selection 'xa-primary))))
	  (when (not (string= string ""))
	    (insert string)))
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
	(insert-rectangle string))))

(defun yank-next ()
  "If the last command was a yank, replace the yanked text with the next
string in the kill ring. Currently this doesn't work when the last command
yanked a rectangle of text."
  (interactive)
  (when (and (eq last-command 'yank)
	     yank-last-item
	     (< yank-last-item (1- (ring-size kill-ring))))
    (goto yank-last-start)
    (delete-area yank-last-start yank-last-end)
    (setq yank-last-end (insert-rectangle (killed-string (1+ yank-last-item)))
	  yank-last-item (1+ yank-last-item)
	  this-command 'yank)))

(defun yank-to-mouse ()
  "Calls `yank inserting at the current position of the mouse cursor. The
cursor is left at the end of the inserted text."
  (interactive)
  (goto (mouse-pos))
  (yank))


;; Transposing

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
	    end1 (funcall forward-item 1 start1)
	    end2 (funcall forward-item 1 end1)
	    start2 (funcall backward-item 1 end2))
      (transpose-1)
      (setq count (1- count)))
    (while (< count 0)
      ;; go backwards
      (setq start1 (funcall backward-item 1)
	    end1 (funcall forward-item 1 start1)
	    start2 (funcall backward-item 1 start1)
	    end2 (funcall forward-item 1 start2))
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
	  (goto (insert text1 start2)))
      (setq text1 (copy-area start1 end1)
	    text2 (cut-area start2 end2))
      (goto (insert text1 start2))
      (delete-area start1 end1)
      (insert text2 start1))))

(defun transpose-lines (count)
  "Move the line under the cursor COUNT lines forwards."
  (interactive "p")
  (transpose-items 'forward-line 'backward-line count))


(defun abort-recursive-edit (&optional ret-val)
  "Exits the innermost recursive edit with a value of VALUE (or nil)."
  (interactive)
  (throw 'exit ret-val))

(defun top-level ()
  "Abort all recursive-edits."
  (interactive)
  (throw 'top-level nil))


;; Overwrite mode

(defvar overwrite-mode-active nil
  "Non-nil when overwrite-mode is enabled.")
(make-variable-buffer-local 'overwrite-mode-active)

(defun overwrite-mode ()
  "Minor mode to toggle overwrite/insert."
  (interactive)
  (if overwrite-mode-active
      (progn
	(setq overwrite-mode-active nil)
	(remove-minor-mode 'overwrite-mode "Overwrite")
	(remove-hook 'unbound-key-hook 'overwrite-insert))
    (add-minor-mode 'overwrite-mode "Overwrite")
    (setq overwrite-mode-active t)
    (add-hook 'unbound-key-hook 'overwrite-insert)))

(defun overwrite-insert (&optional str)
  (unless str
    (setq str (current-event-string)))
  (when str
    (setq len (length str))
    (delete-area (cursor-pos) (forward-char len))
    (insert str)))


;; Miscellaneous editing commands

(defun backspace-char (count)
  "Delete COUNT characters preceding the cursor, if the cursor is past the
end of the line simply move COUNT characters to the left."
  (interactive "p")
  (let
      ((start (forward-char (- count))))
    (if (> (cursor-pos) (end-of-line))
	(if (> start (end-of-line))
	    (goto start)
	  (goto (end-of-line))
	  (delete-area start (cursor-pos)))
      (delete-area start (cursor-pos)))))
  
(defun delete-char (count)
  "Delete the character under the cursor."
  (interactive "p")
  (delete-area (cursor-pos) (forward-char count)))

(defun tab-with-spaces ()
  "Insert enough spaces before the cursor to move it to the next tab position."
  (interactive)
  (indent-to (pos-col (forward-tab)) t))

(defun just-spaces (count)
  "Ensure that there are only COUNT spaces around the cursor."
  (interactive "p")
  (when (member (get-char) '(?\  ?\t))
    (let
	((pos (re-search-backward "[^\t ]|^")))
      (when pos
	(unless (zerop (pos-col pos))
	  (setq pos (forward-char 1 pos)))
	(when (and pos (looking-at "[\t ]+" pos))
	  (delete-area (match-start) (match-end))
	  (goto (match-start))))))
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
    (goto opos)))

(defun delete-blank-lines ()
  "Delete all but the bottom-most of the blank lines surrounding the cursor.
If the cursor isn't actually on a series of blank lines, the next series
is found and deleted. If the cursor is on a single blank line, the line is
deleted."
  (interactive)
  (unless (empty-line-p)
    (if (re-search-forward "^[\t ]*$")
	(goto (match-end))
      (error "End of buffer")))
  (let
      ((start (or (and (re-search-backward "^.*[^\t\n ].*\n")
		       (match-end))
		  (start-of-buffer)))
       (end (or (re-search-forward "^[\t ]*\n.*[^\t\n ].*$") (end-of-buffer))))
    (delete-area start (if (equal start end)
			   (forward-line 1 end)
			 end))))

(defun toggle-buffer-read-only ()
  "Toggle the current buffer between being writable and read-only."
  (interactive)
  (if toggle-read-only-function
      (funcall toggle-read-only-function)
    (set-buffer-read-only nil (not (buffer-read-only-p)))))

(defun goto-start-of-buffer ()
  "Move the cursor to the start of the current buffer."
  (interactive)
  (set-auto-mark)
  (goto (start-of-buffer)))

(defun goto-end-of-buffer ()
  "Move the cursor to the end of the current buffer."
  (interactive)
  (set-auto-mark)
  (goto (end-of-buffer)))

(defun top-of-buffer ()
  "Move the cursor to the first line in the current buffer."
  (interactive)
  (set-auto-mark)
  (goto (pos nil (pos-line (start-of-buffer)))))

(defun bottom-of-buffer ()
  "Move the cursor to the last line in the current buffer."
  (interactive)
  (set-auto-mark)
  (goto (pos nil (pos-line (end-of-buffer)))))


;; Some macros

(defmacro save-restriction (&rest forms)
  "Evaluate FORMS, restoring the original buffer restriction when they
finish (as long as the original buffer still exists)."
  `(let
       (_s_r_start_ _s_r_end_)
     (when (buffer-restricted-p)
       (setq _s_r_start_ (make-mark (restriction-start))
	     _s_r_end_ (make-mark (restriction-end))))
     (unwind-protect
	 ,(cons 'progn forms)
       (when (and _s_r_start_ (mark-resident-p _s_r_start_))
	 (restrict-buffer (mark-pos _s_r_start_)
			  (mark-pos _s_r_end_)
			  (mark-file _s_r_start_))))))
      
(defmacro save-excursion (&rest forms)
  "Evaluate FORMS, ensuring that the original current buffer and the original
position in this buffer are restored afterwards, even in case of a non-local
exit occurring (as long as the original buffer wasn't killed)."
  `(let
       ((_s_c_mark_ (make-mark)))
     (unwind-protect
	 ,(cons 'progn forms)
       (when (mark-resident-p _s_c_mark_)
	 (goto-mark _s_c_mark_ t)))))


;; Mouse dragging etc

(defvar mouse-select-pos nil)
(defvar mouse-dragging nil)
(defvar mouse-dragging-objects nil)

(defun mouse-pos ()
  "Return the position of the character underneath the mouse pointer in
the current view. Returns nil if no such character can be found."
  (let
      ((pos (raw-mouse-pos)))
    (when (and pos (setq pos (translate-pos-to-view pos)))
      (display-to-char-pos pos))))

(defun mouse-select ()
  (interactive)
  (let*
      ((raw-pos (raw-mouse-pos))
       (mouse-view (find-view-by-pos raw-pos)))
    (when mouse-view
      (unless (eq (current-view) mouse-view)
	(set-current-view mouse-view))
      (setq raw-pos (translate-pos-to-view raw-pos))
      (when raw-pos
	(setq mouse-select-pos (display-to-char-pos raw-pos)
	      mouse-dragging nil
	      mouse-dragging-objects nil)
	(block-kill)
	(goto mouse-select-pos)))))

(defun mouse-double-select ()
  (interactive)
  (setq mouse-dragging-objects t))

(defun mouse-select-drag ()
  (interactive)
  (let
      ((pos (mouse-pos)))
    (when mouse-dragging-objects
      (setq pos (forward-word (if (> pos mouse-select-pos) 1 -1) pos)))
    (goto pos)
    (if (equal pos mouse-select-pos)
	(block-kill)
      (setq mouse-dragging pos)
      (block-kill)
      (block-start mouse-select-pos)
      (block-end pos))))
