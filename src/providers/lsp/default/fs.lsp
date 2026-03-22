;; fs.lsp
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
    "SaturnFsResult"
    fs-result
    (:superclass saturn:generic-result
     :export nil
     :interfaces ())
    nil)
(gobject:define-gobject-subclass
    "SaturnFsResultListItem"
    fs-result-list-item
    (:superclass saturn:signal-widget
     :export nil
     :interfaces ())
    nil)

(defvar list-item-ui
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<interface>
  <template class=\"SaturnFsResultListItem\">
    <property name=\"child\">
      <object class=\"GtkBox\">
        <property name=\"orientation\">horizontal</property>
        <property name=\"spacing\">6</property>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"subtitle\"/>
            </style>
            <property name=\"halign\">start</property>
            <property name=\"hexpand\">true</property>
            <property name=\"ellipsize\">start</property>
            <binding name=\"label\">
              <lookup name=\"string\" type=\"GtkStringObject\">
                <lookup name=\"obj1\" type=\"SaturnFsResult\">
                  <lookup name=\"item\">SaturnFsResultListItem</lookup>
                </lookup>
              </lookup>
            </binding>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"title-4\"/>
            </style>
            <property name=\"margin-end\">15</property>
            <property name=\"halign\">end</property>
            <property name=\"ellipsize\">middle</property>
            <binding name=\"label\">
              <lookup name=\"string\" type=\"GtkStringObject\">
                <lookup name=\"obj0\" type=\"SaturnFsResult\">
                  <lookup name=\"item\">SaturnFsResultListItem</lookup>
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
    ((subclass (eql (find-class 'fs-result-list-item))) class data)
  (gtk:widget-class-set-template "SaturnFsResultListItem" list-item-ui))
(defmethod g:object-instance-init :after
    ((subclass (eql (find-class 'fs-result-list-item))) instance data)
  (declare (ignore class data))
  (gtk:widget-init-template instance))
(setf +list-bind-gtype+ "SaturnFsResultListItem")

(defvar *min-query-length* 2)

(let ((*work-lock* (bordeaux-threads:make-lock))
      (*files-array* (make-array 0 :fill-pointer t :adjustable t)))

  (defun gather-files (path)
    (labels ((gather (path)
               (flet ((is-hidden-file (x)
                        (let ((name (or (pathname-name x)
                                        (car (last (pathname-directory x))))))
                          (when (> (length name) 1)
                            (search "." name :end2 1)))))
                 (let ((files (uiop:directory-files path)))
                   (bordeaux-threads:with-lock-held (*work-lock*)
                     (loop for f in files
                           unless (is-hidden-file f)
                             do (vector-push-extend f *files-array*))))
                 (let ((dirs (uiop:subdirectories path)))
                   (loop for d in dirs
                         unless (is-hidden-file d)
                           do (gather d))))))
      (gather path)))

  ;; PROVIDER IMPLEMENTATION

  (let ((*gather-thread* (bordeaux-threads:make-thread
                          (lambda ()
                            (gather-files #P"~/")))))
    (defun deinit-global ()
      (when (bordeaux-threads:thread-alive-p *gather-thread*)
        (bordeaux-threads:destroy-thread *gather-thread*))))

  (defun query (provider object store)
    (let ((str (gtk:string-object-string object)))
      (unless (>= (length str) *min-query-length*)
        (return-from query))
      (bordeaux-threads:make-thread
       (lambda ()
         (bordeaux-threads:with-lock-held (*work-lock*)
           (block root
             (loop for path across *files-array*
                   when (search str (file-namestring path) :test #'char-equal)
                     do (let* ((name (file-namestring path))
                               (directory (uiop:unix-namestring (uiop:pathname-directory-pathname path)))
                               (result (make-instance 'fs-result
                                                      :obj0 (gtk:string-object-new name)
                                                      :obj1 (gtk:string-object-new directory))))
                          ;; gobject properties weirdly don't work
                          (setf (g:object-data result "path") path)
                          (unless (saturn:submit-result result store provider)
                            (return-from root))))))))))

  )


(defun score (provider item query)
  (let ((str (gtk:string-object-string query))
        (name (file-namestring (g:object-data item "path"))))
    (saturn:generic-str-score str name)))

(defun select (provider item query)
  (let* ((path (g:object-data item "path"))
         (file (uiop:unix-namestring path)))
    (ignore-errors
     (uiop:run-program (list "flatpak-spawn"
                             "--host"
                             "xdg-open"
                             file))))
  ;; exit saturn
  t)

(defun bind-preview (provider item)
  (let* ((label
           (saturn:make-widget 'gtk:label
               (:props (:label (uiop:unix-namestring (g:object-data item "path"))
                        :ellipsize :middle
                        :hexpand t)
                :styles ("monospace")))))
    label))
