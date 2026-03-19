;; grep.lsp
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

(defvar *min-query-length* 3)
(defun make-grep-cmd (word)
  (list "flatpak-spawn"
        "--host"
        "/var/home/linuxbrew/.linuxbrew/bin/rg"
        ;; From ripgrep manual: This flag prints the file path above clusters of
        ;; matches from each file instead of printing the file path as a prefix
        ;; for each matched line.
        "--heading"
        ;; From ripgrep manual: Show line numbers (1-based).
        "--line-number"
        word
        (uiop:unix-namestring (user-homedir-pathname))))

(gobject:define-gobject-subclass
    "SaturnGrepResult"
    grep-result
    (:superclass saturn:generic-result
     :export t
     :interfaces ())
    nil)
(gobject:define-gobject-subclass
    "SaturnGrepResultListItem"
    grep-result-list-item
    (:superclass saturn:signal-widget
     :export nil
     :interfaces ())
    nil)

(defvar list-item-ui
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<interface>
  <template class=\"SaturnGrepResultListItem\">
    <property name=\"child\">
      <object class=\"GtkBox\">
        <property name=\"orientation\">horizontal</property>
        <property name=\"spacing\">6</property>
        <child>
          <object class=\"GtkImage\">
            <style>
              <class name=\"dimmed\"/>
            </style>
            <property name=\"icon-name\">folder-saved-search-symbolic</property>
            <property name=\"icon-size\">normal</property>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"subtitle\"/>
            </style>
            <property name=\"label\">File Contents</property>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <property name=\"halign\">end</property>
            <property name=\"hexpand\">true</property>
            <property name=\"ellipsize\">end</property>
            <binding name=\"label\">
              <lookup name=\"string\" type=\"GtkStringObject\">
                <lookup name=\"obj0\" type=\"SaturnGrepResult\">
                  <lookup name=\"item\">SaturnGrepResultListItem</lookup>
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
    ((subclass (eql (find-class 'grep-result-list-item))) class data)
  (gtk:widget-class-set-template "SaturnGrepResultListItem" list-item-ui))
(defmethod g:object-instance-init :after
    ((subclass (eql (find-class 'grep-result-list-item))) instance data)
  (declare (ignore class data))
  (gtk:widget-init-template instance))
(setf +list-bind-gtype+ "SaturnGrepResultListItem")


;; PROVIDER IMPLEMENTATION

(let ((*timeout-source* 0))

  (defun query (provider object store)
    (when (not (= *timeout-source* 0))
      (g:source-remove *timeout-source*)
      (setf *timeout-source* 0))
    (let* ((str (gtk:string-object-string object)))
      (when (>= (length str) *min-query-length*)
        (setf *timeout-source*
              ;; debounce 0.5 seconds
              (g:timeout-add
               500
               (lambda ()
                 (setf *timeout-source* 0)
                 (bordeaux-threads:make-thread
                  (lambda ()
                    (block root
                      (let ((process
                              (ignore-errors
                               (uiop:launch-program (make-grep-cmd str)
                                                    :output :stream))))
                        (when process
                          (with-open-stream (s (uiop:process-info-output process))
                            (let ((current-path nil)
                                  (current-matches nil))
                              (loop for line = (ignore-errors (read-line s nil nil))
                                    while line
                                    do (progn
                                         (if (uiop:emptyp line)
                                             (progn
                                               (when (and current-path
                                                          current-matches)
                                                 (unless (saturn:submit-result
                                                          (make-instance
                                                           'grep-result
                                                           :obj0 (gtk:string-object-new current-path)
                                                           :obj1 (gtk:string-object-new
                                                                  (apply #'concatenate 'string
                                                                   (mapcar #'(lambda (x)
                                                                               (format nil "~A~%" x))
                                                                           (reverse current-matches)))))
                                                          store provider)
                                                   (return-from root)))
                                               (setf current-path nil
                                                     current-matches nil))
                                             (if current-path
                                                 (push line current-matches)
                                                 (setf current-path line))))))))))))
                 ;; don't run again
                 nil))))))

  )

(defconstant newline-char #\
  )
(defun score (provider item query)
  (let ((str (gtk:string-object-string query))
        (matched-lines (gtk:string-object-string (g:object-property item "obj1"))))
    (* 1000 (count newline-char matched-lines))))

(defun select (provider item query)
  (format t "selected the file!~%")
  nil)

(defun bind-preview (provider item)
  (let* ((matched-lines
           (gtk:string-object-string (g:object-property item "obj1")))
         (text-view
           (saturn:make-widget
               'gtk:text-view
               (:props (:buffer (make-instance 'gtk:text-buffer
                                               :text matched-lines)
                        :monospace t)
                :styles ("monospace"))))
         (scrolled-window
           (saturn:make-widget
               'gtk:scrolled-window
               (:props (:child text-view)))))
    scrolled-window))
