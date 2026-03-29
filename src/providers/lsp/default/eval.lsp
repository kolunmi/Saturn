;; eval.lsp
;;
;; Copyright 2026 Eva M
;;
;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.
;;
;; SPDX-License-Identifier: GPL-3.0-or-later

(gobject:define-gobject-subclass
    "SaturnEvalResult"
    eval-result
    (:superclass g:object
     :export t
     :interfaces ())
    nil)

;; PROVIDER IMPLEMENTATION

(defun deinit-global (selected-text)
  nil)

(defun query (provider object store)
  (let* ((str (gtk:string-object-string object)))
    (unless (string-equal str ":lisp")
      (return-from query))
    (saturn:submit-result (make-instance 'eval-result)
                          store provider)))

(defun score (provider item query)
  1000000000000)

(defun select (provider item query)
  nil)

(defun bind-list-item (provider item)
  (let ((label
          (saturn:make-widget 'gtk:label
              (:props (:label "LISP EVALUATION"
                       :hexpand t)
               :styles ("accent" "title-2")))))
    label))

(defun bind-preview (provider item)
  (let* ((source-list (multiple-value-list (saturn:make-lisp-buffer-view)))
         (source-widget (first source-list))
         (source-view (second source-list))
         (result-list (multiple-value-list (saturn:make-lisp-buffer-view)))
         (result-widget (first result-list))
         (result-view
           (let ((view (second result-list)))
             (setf (gtk:text-view-editable view) nil)
             view))
         (result-scrolled-window (third result-list)))
    (setf (gtk:widget-height-request source-widget) 50)
    (setf (gtk:widget-height-request result-widget) 150)
    (flet ((evaluate (self &rest args)
             (let* ((source-buffer (gtk:text-view-buffer source-view))
                    (source-contents (gtk:text-buffer-text source-buffer))
                    (result-buffer (gtk:text-view-buffer result-view))
                    (result (handler-case
                                (mapcar #'eval
                                        (read-from-string
                                         (format nil "(~a)"
                                                 source-contents)))
                              (error (e)
                                (format nil "ERROR ~a ~S"
                                        e e))
                              (:no-error (vals)
                                (format nil "~S"
                                        (if (> (length vals) 1)
                                            vals
                                            (first vals))))))
                    (hadjustment (gtk:scrolled-window-hadjustment result-scrolled-window))
                    (vadjustment (gtk:scrolled-window-vadjustment result-scrolled-window)))
               ;; append to the end of the buffer with newline
               (gtk:text-buffer-insert result-buffer
                                       (gtk:text-buffer-end-iter result-buffer)
                                       (format nil "~a~%" result))
               ;; we are using idle-add to give the source view a moment to
               ;; re-layout :)
               (g:idle-add (lambda ()
                             (setf (gtk:adjustment-value hadjustment)
                                   (gtk:adjustment-lower hadjustment))
                             (setf (gtk:adjustment-value vadjustment)
                                   (gtk:adjustment-upper vadjustment))
                             nil)))))
      ;; set up completions
      (saturn:package-completion-buffer-async source-view
                                              *package*)
      ;; if the user pressing ctrl-g, evaluate the buffer
      (saturn:add-shortcut-controller source-view (("<primary>g" #'evaluate)))
      ;; when we map, make it as convenient as possible to start typing code
      (g:signal-connect source-view "map"
                        (lambda (self)
                          (gtk:widget-grab-focus self))
                        :after t)
      (let* ((eval-button
               (saturn:make-widget 'gtk:button
                   (:props (:label "Evaluate Buffer <ctrl-g>"
                            :halign :end
                            :valign :start
                            :margin-start 15
                            :margin-end 15
                            :margin-top 15
                            :margin-bottom 15)
                    :styles ("osd")
                    :connect (("clicked" #'evaluate)))))
             (overlay
               (saturn:make-widget 'gtk:overlay
                   (:props (:child result-widget))
                   (lambda (x)
                     (gtk:overlay-add-overlay x eval-button))))
             (paned
               (saturn:make-widget 'gtk:paned
                   (:props (:orientation :vertical
                            :wide-handle t
                            :start-child source-widget
                            :end-child overlay
                            :shrink-start-child nil
                            :shrink-end-child t
                            :resize-end-child nil)))))
        paned))))
