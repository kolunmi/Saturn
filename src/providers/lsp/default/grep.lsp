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

(defvar +highlight-rgbas+
  (mapcar #'(lambda (spec)
              (gdk:rgba-parse spec))
          '("#3584e4aa"
            "#2190a4aa"
            "#3a944aaa"
            "#c88800aa"
            "#ed5b00aa"
            "#e62d42aa"
            "#d56199aa"
            "#9141acaa"
            "#6f8396aa")))

(defvar *possible-rg-bins*
  '("rg"
    "/var/home/linuxbrew/.linuxbrew/bin/rg"))
(defun make-grep-cmd (bin word)
  (list "flatpak-spawn"
        "--host"
        bin
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
            <property name=\"ellipsize\">start</property>
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

(defun deinit-global ()
  nil)

(let ((*timeout-source* 0))

  (defun query (provider object store)
    (when (not (= *timeout-source* 0))
      (g:source-remove *timeout-source*)
      (setf *timeout-source* 0))
    (let* ((str (gtk:string-object-string object))
           (strlen (length str)))
      (unless (>= strlen *min-query-length*)
        (return-from query))
      (labels ((run-cmd ()
                 (loop for bin in *possible-rg-bins*
                       when (saturn:flatpak-spawn-host-bin-exists bin)
                         return (uiop:launch-program (make-grep-cmd bin str)
                                                     :output :stream)))
               (populate-buffer-line (buffer cursor line match-offset match-color)
                 (let* ((start-seq (subseq line 0 match-offset))
                        (tag-seq (subseq line match-offset (+ match-offset strlen)))
                        (end-seq (format nil "~a~%" (subseq line (+ match-offset strlen))))
                        (tag (gtk:text-buffer-create-tag buffer nil
                                                         :background-rgba match-color)))
                   (gtk:text-iter-forward-to-end cursor)
                   (gtk:text-buffer-insert buffer cursor start-seq)
                   (gtk:text-iter-forward-to-end cursor)
                   (gtk:text-buffer-insert-with-tags buffer cursor tag-seq tag)
                   (gtk:text-iter-forward-to-end cursor)
                   (gtk:text-buffer-insert buffer cursor end-seq)))
               (finish-result (line path matches)
                 (saturn:submit-result
                  (make-instance
                   'grep-result
                   :obj0 (gtk:string-object-new path)
                   :obj1 (let* ((buffer (make-instance 'gtk:text-buffer))
                                (cursor (gtk:text-buffer-start-iter buffer)))
                           (loop for line in (reverse matches)
                                 for match-offset = (search str line)
                                 for idx from 0
                                 for match-color = (nth (mod idx
                                                             (length +highlight-rgbas+))
                                                        +highlight-rgbas+)
                                 when match-offset
                                   do (populate-buffer-line buffer
                                                            cursor
                                                            line
                                                            match-offset
                                                            match-color))
                           buffer))
                  store provider))
               (thread ()
                 (let ((process (run-cmd)))
                   (unless process
                     (return-from thread))
                   (with-open-stream (s (uiop:process-info-output process))
                     (let ((current-path nil)
                           (current-matches nil))
                       (loop for line = (ignore-errors (read-line s nil nil))
                             while line
                             do (if (uiop:emptyp line)
                                    (progn
                                      (when (and current-path
                                                 current-matches)
                                        (finish-result line
                                                       current-path
                                                       current-matches))
                                      (setf current-path nil
                                            current-matches nil))
                                    (if current-path
                                        (push line current-matches)
                                        (setf current-path line))))))))
               (idle-timeout ()
                 (setf *timeout-source* 0)
                 (bordeaux-threads:make-thread #'thread)
                 ;; don't run again
                 nil))
        (setf *timeout-source*
              ;; debounce 0.5 seconds
              (g:timeout-add 500 #'idle-timeout)))))

  )

(defun score (provider item query)
  (let ((str (gtk:string-object-string query))
        (n-matched-lines (gtk:text-buffer-line-count (g:object-property item "obj1"))))
    (* 100 n-matched-lines)))

(defun select (provider item query)
  ;; same as fs.lsp
  (let* ((path (gtk:string-object-string (g:object-property item "obj0"))))
    (ignore-errors
     (uiop:run-program (list "flatpak-spawn"
                             "--host"
                             "xdg-open"
                             path))))
  ;; exit saturn
  t)

(defun bind-preview (provider item)
  (let* ((buffer (g:object-property item "obj1"))
         (text-view
           (saturn:make-widget
               'gtk:text-view
               (:props (:buffer buffer
                        :monospace t)
                :styles ("text-preview"))))
         (scrolled-window
           (saturn:make-widget
               'gtk:scrolled-window
               (:props (:child text-view)))))
    scrolled-window))
