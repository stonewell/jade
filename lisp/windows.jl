;;;; windows.jl -- Window and view handling
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

(defun in-new-window (command)
  (goto-new-window)
  (call-command command))

(defun goto-new-window ()
  (interactive)
  (set-current-window (make-window) t))

(defun toggle-iconic ()
  "Toggle the current window between iconified and normal states."
  (interactive)
  (if (window-asleep-p)
      (unsleep-window)
    (sleep-window)))


;; View handling

(defun delete-other-views (&optional view)
  "Close all views in the current window except for VIEW, or the current one."
  (interactive)
  (unless view
    (setq view (current-view)))
  (let
      ((doomed (window-first-view))
       next)
    (while (> (window-view-count) 2)
      (setq next (next-view doomed))
      (unless (or (eq doomed view) (minibuffer-view-p doomed))
	(delete-view doomed))
      (setq doomed next))))

(defun in-other-view (command)
  "Switches to the `other' view in this window then calls the command
COMMAND in it."
  (goto-other-view)
  (call-command command))

(defun goto-other-view ()
  "Switch to a different view in the current window."
  (interactive)
  (set-current-view (other-view)))

(defun other-view (&optional lines)
  "Return a different view in the current window. LINES defines the lower
bound on the number of lines in the new view; alternately, if it is the
symbol nil the other view will be roughly half the size of the current view,
or if it is the symbol t the size of the other view won't be changed."
  (if (= 2 (window-view-count))
      (split-view nil lines)
    (let
	((view (next-view))
	 total desired)
      (when (minibuffer-view-p view)
	(setq view (previous-view)))
      (unless (eq lines t)
	(setq total (+ (cdr (view-dimensions))
		       (cdr (view-dimensions view)))
	      desired (or lines (/ total 2)))
	;; Only enlarge if the other-view is currently _smaller_
	;; than it's desired size
	(unless (> (cdr (view-dimensions view)) desired)
	  (enlarge-view (- total desired (cdr (view-dimensions))))))
      view)))

(defun goto-next-view (&optional all-windows-p)
  "Cycles through the available views. If ALL-WINDOWS-P is t views in
windows other than the current window are used when needed."
  (interactive "P")
  (set-current-view (next-view nil all-windows-p) all-windows-p))

(defun scroll-next-view (&optional count)
  "Scroll the view following the current view in this window by COUNT
screenfuls. When called interactively COUNT is taken from the prefix arg.
Negative arguments scroll backwards."
  (interactive "p")
  (with-view (next-view)
    (next-screen count)))

(defun enlarge-view (&optional count)
  "Enlarge the current view by one line. If COUNT is specified enlarge
by COUNT lines. When called interactively, COUNT is taken from the prefix
argument."
  (interactive "p")
  (unless count (setq count 1))
  (let*
      ((views (window-view-list))
       (view-count (1- (window-view-count)))	;ignore minibuf
       (view-index (- view-count (1- (length (memq (current-view) views)))))
       view)
    (cond
     ((= view-count 1)
      (error "Can't resize a single view"))
     ((= view-index (1- view-count))
      ;; Last view in window, expand the previous window negatively
      (setq view (previous-view)
	    count (- count)))
     (t
      (setq view (current-view))))
    (set-view-dimensions view nil (+ (cdr (view-dimensions view)) count))))

(defun shrink-view (&optional count)
  "Shrink the current view by one line. If COUNT is specified shrink by
COUNT lines. When called interactively, COUNT is taken from the prefix
argument."
  (interactive "p")
  (enlarge-view (- (or count 1))))

(defun shrink-view-if-larger-than-buffer ()
  "If the current view is larger than the buffer that it is displaying, shrink
it so that the buffer just fits the view."
  (interactive)
  (if (<= (window-view-count) 2)
      (error "Can't resize a single view")
    (when (equal (view-origin) (start-of-buffer))
      (let
	  ((view-rows (cdr (view-dimensions)))
	   (end (char-to-display-pos (end-of-buffer))))
	(when (and end (>= view-rows (pos-line end)))
	  (goto (start-of-buffer))
	  ;; add one since positions count from zero
	  (enlarge-view (- (pos-line end) view-rows -1)))))))
