;; history.lsp
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

(defvar +history-file+
  (uiop:subpathname (saturn:get-saturn-cache-dir)
                    "history.txt"))

(gobject:define-gobject-subclass
    "SaturnHistoryResult"
    history-result
    (:superclass saturn:generic-result
     :export t
     :interfaces ())
    nil)
(gobject:define-gobject-subclass
    "SaturnHistoryResultListItem"
    history-result-list-item
    (:superclass saturn:signal-widget
     :export nil
     :interfaces ())
    nil)

(defvar list-item-ui
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<interface>
  <template class=\"SaturnHistoryResultListItem\">
    <property name=\"child\">
      <object class=\"GtkBox\">
        <property name=\"orientation\">horizontal</property>
        <property name=\"spacing\">6</property>
        <child>
          <object class=\"GtkImage\">
            <style>
              <class name=\"dimmed\"/>
            </style>
            <property name=\"icon-name\">preferences-system-time-symbolic</property>
            <property name=\"icon-size\">normal</property>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"subtitle\"/>
            </style>
            <property name=\"label\">History</property>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"title-4\"/>
            </style>
            <property name=\"halign\">end</property>
            <property name=\"hexpand\">true</property>
            <property name=\"ellipsize\">end</property>
            <binding name=\"label\">
              <lookup name=\"string\" type=\"GtkStringObject\">
                <lookup name=\"obj0\" type=\"SaturnHistoryResult\">
                  <lookup name=\"item\">SaturnHistoryResultListItem</lookup>
                </lookup>
              </lookup>
            </binding>
          </object>
        </child>
      </object>
    </property>
  </template>
</interface>
")
(defmethod g:object-class-init :after
    ((subclass (eql (find-class 'history-result-list-item))) class data)
  (gtk:widget-class-set-template "SaturnHistoryResultListItem" list-item-ui))
(defmethod g:object-instance-init :after
    ((subclass (eql (find-class 'history-result-list-item))) instance data)
  (declare (ignore class data))
  (gtk:widget-init-template instance))
(setf +list-bind-gtype+ "SaturnHistoryResultListItem")


(defvar +history+
  (ignore-errors
   (with-open-file (s +history-file+
                      :direction :input)
     (loop for line = (read-line s nil)
           while line
           collect (make-instance 'history-result
                                  :obj0 (gtk:string-object-new line))))))

;; PROVIDER IMPLEMENTATION

(defun deinit-global (selected-text)
  (uiop/common-lisp:ensure-directories-exist +history-file+)
  (unless selected-text
    (return-from deinit-global))
  ;; append to history
  (with-open-file (s +history-file+
                     :direction :output
                     :if-exists :append
                     :if-does-not-exist :create)
    (format s "~a~%" selected-text)))

(defun query (provider object store)
  (unless (= 0 (length (gtk:string-object-string object)))
    (return-from query))
  (loop for result in +history+
        do (saturn:submit-result result store provider)))

(defun score (provider item query)
  1)

(defun select (provider item query)
  (let* ((selection-text (gtk:string-object-string
                          (g:object-property item "obj0"))))
    (saturn:copy-to-clipboard selection-text))
  nil)

(defun bind-preview (provider item)
  (let* ((selection-text (gtk:string-object-string
                          (g:object-property item "obj0")))
         (label
           (saturn:make-widget 'gtk:label
               (:props (:label "History"
                        :halign :center
                        :valign :center)
                :styles ("title-4")))))
    label))
