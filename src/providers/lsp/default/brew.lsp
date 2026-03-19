;; brew.lsp
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
(defvar *brew-cmd* '("flatpak-spawn"
                     "--host"
                     "/var/home/linuxbrew/.linuxbrew/bin/brew"))

(gobject:define-gobject-subclass
    "SaturnBrewResult"
    brew-result
    (:superclass saturn:generic-result
     :export t
     :interfaces ())
    nil)
(gobject:define-gobject-subclass
    "SaturnBrewResultListItem"
    brew-result-list-item
    (:superclass saturn:signal-widget
     :export nil
     :interfaces ())
    nil)

(defvar list-item-ui
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<interface>
  <template class=\"SaturnBrewResultListItem\">
    <property name=\"child\">
      <object class=\"GtkBox\">
        <property name=\"orientation\">horizontal</property>
        <property name=\"spacing\">6</property>
        <child>
          <object class=\"GtkImage\">
            <style>
              <class name=\"dimmed\"/>
            </style>
            <property name=\"icon-name\">package-x-generic-symbolic</property>
            <property name=\"icon-size\">normal</property>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"subtitle\"/>
            </style>
            <property name=\"label\">Brew</property>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"title-4\"/>
            </style>
            <property name=\"halign\">start</property>
            <property name=\"ellipsize\">end</property>
            <binding name=\"label\">
              <lookup name=\"string\" type=\"GtkStringObject\">
                <lookup name=\"obj0\" type=\"SaturnBrewResult\">
                  <lookup name=\"item\">SaturnBrewResultListItem</lookup>
                </lookup>
              </lookup>
            </binding>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"subtitle\"/>
            </style>
            <property name=\"halign\">end</property>
            <property name=\"hexpand\">true</property>
            <property name=\"ellipsize\">end</property>
            <binding name=\"label\">
              <lookup name=\"string\" type=\"GtkStringObject\">
                <lookup name=\"obj1\" type=\"SaturnBrewResult\">
                  <lookup name=\"item\">SaturnBrewResultListItem</lookup>
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
    ((subclass (eql (find-class 'brew-result-list-item))) class data)
  (gtk:widget-class-set-template "SaturnBrewResultListItem" list-item-ui))
(defmethod g:object-instance-init :after
    ((subclass (eql (find-class 'brew-result-list-item))) instance data)
  (declare (ignore class data))
  (gtk:widget-init-template instance))
(setf +list-bind-gtype+ "SaturnBrewResultListItem")


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
                      (let ((results
                              (ignore-errors
                               (uiop:run-program (append *brew-cmd*
                                                         (list "search"
                                                               "--desc"
                                                               str))
                                                 :output :string))))
                        (when results
                          (with-input-from-string (s results)
                            (loop for line = (read-line s nil nil)
                                  while line
                                  do (let* ((colon-idx (search ":" line)))
                                       (when colon-idx
                                         (let ((pkg-name (subseq line 0 colon-idx))
                                               (pkg-desc (subseq line (1+ colon-idx))))
                                           (when (and pkg-name pkg-desc)
                                             (let ((result (make-instance 'brew-result
                                                                          :obj0 (gtk:string-object-new pkg-name)
                                                                          :obj1 (gtk:string-object-new pkg-desc))))
                                               (unless (saturn:submit-result result store provider)
                                                 (return-from root))))))))))))))
                 ;; don't run again
                 nil))))))

  )

(defun score (provider item query)
  (let ((str (gtk:string-object-string query))
        (pkg-name (gtk:string-object-string (g:object-property item "obj0"))))
    (round (/ (saturn:generic-str-score str pkg-name) 10))))

(defun select (provider item query)
  (format t "selected the package!~%")
  nil)

(defun bind-preview (provider item)
  (let* ((pkg-name (gtk:string-object-string (g:object-property item "obj0")))
         (pkg-desc (gtk:string-object-string (g:object-property item "obj1")))
         (icon
           (saturn:make-widget
               'gtk:image
               (:props (:icon-name "package-x-generic-symbolic"
                        :halign :center
                        :pixel-size 64))))
         (top-label
           (saturn:make-widget
               'gtk:label
               (:props (:label pkg-name
                        :xalign 0.5
                        :halign :center
                        :wrap t)
                :styles ("title-2"))))
         (bottom-label
           (saturn:make-widget
               'gtk:label
               (:props (:label pkg-desc
                        :xalign 0.5
                        :halign :center
                        :justify :center
                        :wrap t
                        :hexpand t)
                :styles ("subtitle"))))
         (box
           (saturn:make-widget
               'gtk:box
               (:props (:orientation :vertical
                        :spacing 10
                        :valign :center
                        :halign :center))
               (lambda (x)
                 (gtk:box-append x icon)
                 (gtk:box-append x top-label)
                 (gtk:box-append x bottom-label)))))
    box))
