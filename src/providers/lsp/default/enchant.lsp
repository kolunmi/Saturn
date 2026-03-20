;; enchant.lsp
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

(defvar *min-query-length* 2)

(gobject:define-gobject-subclass
    "SaturnEnchantResult"
    enchant-result
    (:superclass saturn:generic-result
     :export t
     :interfaces ())
    nil)
(gobject:define-gobject-subclass
    "SaturnEnchantResultListItem"
    enchant-result-list-item
    (:superclass saturn:signal-widget
     :export nil
     :interfaces ())
    nil)

(defvar list-item-ui
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<interface>
  <template class=\"SaturnEnchantResultListItem\">
    <property name=\"child\">
      <object class=\"GtkBox\">
        <property name=\"orientation\">horizontal</property>
        <property name=\"spacing\">6</property>
        <child>
          <object class=\"GtkImage\">
            <style>
              <class name=\"dimmed\"/>
            </style>
            <property name=\"icon-name\">tools-check-spelling-symbolic</property>
            <property name=\"icon-size\">normal</property>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"subtitle\"/>
            </style>
            <property name=\"label\">Spelling Suggestion</property>
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
                <lookup name=\"obj0\" type=\"SaturnEnchantResult\">
                  <lookup name=\"item\">SaturnEnchantResultListItem</lookup>
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
    ((subclass (eql (find-class 'enchant-result-list-item))) class data)
  (gtk:widget-class-set-template "SaturnEnchantResultListItem" list-item-ui))
(defmethod g:object-instance-init :after
    ((subclass (eql (find-class 'enchant-result-list-item))) instance data)
  (declare (ignore class data))
  (gtk:widget-init-template instance))
(setf +list-bind-gtype+ "SaturnEnchantResultListItem")


;; PROVIDER IMPLEMENTATION

(let ((*timeout-source* 0))

  (defun query (provider object store)
    (when (not (= *timeout-source* 0))
      (g:source-remove *timeout-source*)
      (setf *timeout-source* 0))
    (let* ((str (gtk:string-object-string object)))
      (unless (>= (length str) *min-query-length*)
        (return-from query))
      (labels ((run-cmd ()
                 (ignore-errors
                  (with-input-from-string (s str)
                    (uiop:run-program '("enchant-2" "-a")
                                      ;; passing str as stdin
                                      :input s
                                      :output :string))))
               (output-line-to-suggestions (line)
                 (let ((colon-idx (search ":" line)))
                   (unless colon-idx
                     (return-from output-line-to-suggestions))
                   (split-sequence:split-sequence-if
                    (let ((split-next nil))
                      (lambda (ch)
                        (cond
                          ((eql ch #\,) (setf split-next t))
                          (split-next (progn (setf split-next nil) t)))))
                    (subseq line (+ 2 colon-idx))
                    :remove-empty-subseqs t)))
               (idle-timeout ()
                 (setf *timeout-source* 0)
                 (let ((output (run-cmd)))
                   (unless output
                     (return-from idle-timeout))
                   (with-input-from-string (s output)
                     ;; discard line the first line, which looks like this:
                     ;; ```
                     ;; @(#) International Ispell Version 3.1.20 (but really Enchant 2.8.15)
                     ;; ```
                     (read-line s nil nil)
                     (let* ((line (read-line s nil nil))
                            (suggestions (output-line-to-suggestions line)))
                       (loop for suggestion in suggestions
                             do (saturn:submit-result
                                 (make-instance 'enchant-result
                                                :obj0 (gtk:string-object-new suggestion))
                                 store provider)))))
                 ;; don't run again
                 nil))
      (setf *timeout-source*
            ;; debounce 0.25 seconds
            (g:timeout-add 250 #'idle-timeout)))))

  )

(defun score (provider item query)
  (let ((str (gtk:string-object-string query))
        (suggestion (gtk:string-object-string (g:object-property item "obj0"))))
    (saturn:generic-str-score str suggestion)))

(defun select (provider item query)
  (let* ((suggestion (gtk:string-object-string
                      (g:object-property item "obj0"))))
    (gdk:clipboard-set-text
     (gdk:display-clipboard
      (gdk:display-default))
     suggestion)))

(defun bind-preview (provider item)
  (let* ((suggestion (gtk:string-object-string
                      (g:object-property item "obj0")))
         (copy-button
           (saturn:make-widget 'gtk:button
               (:props (:label (concatenate 'string "Copy \"" suggestion "\"")
                        :halign :center
                        :valign :center)
                :connect (("clicked"
                           (lambda (self)
                             (gdk:clipboard-set-text
                              (gdk:display-clipboard
                               (gdk:display-default))
                              suggestion))))))))
    copy-button))
