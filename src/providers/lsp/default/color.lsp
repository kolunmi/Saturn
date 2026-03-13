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
    ((rgba
      color-result-rgba
      "rgba" "GdkRGBA" t t)))

(gobject:define-gobject-subclass
    "SaturnColorWidget"
    color-widget
    (:superclass gtk:widget
     :export t
     :interfaces ())
    ((result
      color-widget-result
      "result" "SaturnColorResult" t t)))

(gobject:define-vtable ("SaturnColorWidget" color-widget)
  (:skip parent-instance (:struct gobject::object-class))
  ;; Methods for the GtkWidget class
  (:skip show :pointer)
  (:skip hide :pointer)
  (:skip map :pointer)
  (:skip unmap :pointer)
  (:skip realize :pointer)
  (:skip unrealize :pointer)
  (:skip root :pointer)
  (:skip unroot :pointer)
  (:skip size-allocate :pointer)
  (:skip state-flags-changed :pointer)
  (:skip direction-changed :pointer)
  (:skip get-request-mode :pointer)
  (:skip measure :pointer)
  (:skip mnemonic-activate :pointer)
  (:skip grab-focus :pointer)
  (:skip focus :pointer)
  (:skip set-focus-child :pointer)
  (:skip move-focus :pointer)
  (:skip keynav-failed :pointer)
  (:skip query-tooltip :pointer)
  (:skip compute-expand :pointer)
  (:skip css-changed :pointer)
  (:skip system-settings-changed :pointer)
  (:skip contains :pointer)
  (snapshot (:void (self (g:object color-widget))
                   (snapshot (g:object gtk:snapshot)))))

(defmethod color-widget-snapshot-impl ((self color-widget) (snapshot gtk:snapshot))
  (let ((result (color-widget-result self)))
    (graphene:with-rect (bounds 0 0
                                (gtk:widget-width self)
                                (gtk:widget-height self))
      (gtk:snapshot-append-color snapshot
                                 (color-result-rgba result)
                                 bounds))))

;; PROVIDER IMPLEMENTATION

(defun query (object)
  (let* ((str (gtk:string-object-string object))
         (rgba (gdk:rgba-parse str)))
    (when rgba
      (make-instance 'color-result
                     :rgba rgba))))

(defun score (item query)
  10000000000)

(defun select (item query)
  (format t "selected the color!~%")
  nil)

(defun bind-list-item (item)
  (let* ((label
           (saturn:make-widget 'gtk:label
               (:props (:label "color"
                        :hexpand t))))
         (color
           (saturn:make-widget 'color-widget
               (:props (:result item
                        :width-request 50))))
         (box
           (saturn:make-widget 'gtk:box
               (:props (:orientation :horizontal
                        :spacing 10))
               (lambda (x)
                 (gtk:box-append x label)
                 (gtk:box-append x color)))))
    box))

(defun bind-preview (item)
  (make-instance 'color-widget
                 :result item))
