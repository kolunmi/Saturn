;; color.lsp
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
    "SaturnColorResult"
    color-result
    (:superclass g:object
     :export t
     :interfaces ())
    nil)

(defun make-color-widget (rgba)
  (saturn:make-widget 'saturn:signal-widget
      (:connect (("snapshot"
                  (lambda (self snapshot)
                    (graphene:with-rect (bounds 0 0
                                                (gtk:widget-width self)
                                                (gtk:widget-height self))
                      (gtk:snapshot-append-color snapshot
                                                 rgba
                                                 bounds))))))))

;; PROVIDER IMPLEMENTATION

(defun deinit-global (selected-text)
  nil)

(defun query (provider object store)
  (let* ((str (gtk:string-object-string object))
         (rgba (gdk:rgba-parse str)))
    (unless (> (length str) 0)
      (return-from query))
    (when rgba
      (saturn:submit-result (let ((result (make-instance 'color-result)))
                              (setf (g:object-data result "rgba") rgba)
                              result)
                            store provider))))

(defun score (provider item query)
  10000000000)

(defun select (provider item query)
  (let* ((rgba (g:object-data item "rgba"))
         (hex (gdk:rgba-to-string rgba)))
    (saturn:copy-to-clipboard hex))
  (make-instance 'saturn:selection-event
                 :kind :close
                 :selected-text (gtk:string-object-string query)))

(defun bind-list-item (provider item)
  (let* ((label
           (saturn:make-widget 'gtk:label
               (:props (:label "Valid Color"
                        :hexpand t))))
         (color
           (let ((widget (make-color-widget (g:object-data item "rgba"))))
             (setf (gtk:widget-width-request widget) 50)
             widget))
         (box
           (saturn:make-widget 'gtk:box
               (:props (:orientation :horizontal
                        :spacing 10))
               (lambda (x)
                 (gtk:box-append x label)
                 (gtk:box-append x color)))))
    box))

(defun bind-preview (provider item)
  (let* ((rgba (g:object-data item "rgba"))
         (color (make-color-widget rgba))
         (hex (gdk:rgba-to-string rgba))
         (copy-button
           (saturn:make-widget 'gtk:button
               (:props (:label "Copy Color"
                        :halign :center
                        :valign :center)
                :styles ("osd")
                :connect (("clicked"
                           (lambda (self)
                             (saturn:copy-to-clipboard hex)))))))
         (overlay
           (saturn:make-widget 'gtk:overlay
               (:props (:child color))
               (lambda (x)
                 (gtk:overlay-add-overlay x copy-button)))))
    overlay))
