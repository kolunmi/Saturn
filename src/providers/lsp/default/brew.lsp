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

(gobject:define-gobject-subclass
    "SaturnBrewResult"
    brew-result
    (:superclass g:object
     :export t
     :interfaces ())
    nil)

(defvar *brew-cmd* '("flatpak-spawn"
                     "--host"
                     "/var/home/linuxbrew/.linuxbrew/bin/brew"))

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
                                             (let ((result (make-instance 'brew-result)))
                                               (setf (g:object-data result "pkg-name") pkg-name)
                                               (setf (g:object-data result "pkg-desc") pkg-desc)
                                               (unless (saturn:submit-result result store provider)
                                                 (return-from root))))))))))))))
                 ;; don't run again
                 nil))))))

  )

(defun score (provider item query)
  100000000)

(defun select (provider item query)
  (format t "selected the package!~%")
  nil)

(defun bind-list-item (provider item)
  (let* ((pkg-name (g:object-data item "pkg-name"))
         (pkg-desc (g:object-data item "pkg-desc"))
         (icon
           (saturn:make-widget
               'gtk:image
               (:props (:icon-name "package-x-generic-symbolic"
                        :icon-size :normal))))
         (start-label
           (saturn:make-widget
               'gtk:label
               (:props (:label pkg-name
                        :xalign 0.0
                        :ellipsize :end
                        :margin-end 25)
                :styles ("title-4"))))
         (end-label
           (saturn:make-widget
               'gtk:label
               (:props (:label (concatenate 'string
                                            "Brew Package: "
                                            pkg-desc)
                        :xalign 1.0
                        :ellipsize :end
                        :hexpand t)
                :styles ("subtitle"))))
         (box
           (saturn:make-widget
               'gtk:box
               (:props (:orientation :horizontal
                        :spacing 5))
               (lambda (x)
                 (gtk:box-append x icon)
                 (gtk:box-append x start-label)
                 (gtk:box-append x end-label)))))
    box))

(defun bind-preview (provider item)
  (let* ((pkg-name (g:object-data item "pkg-name"))
         (pkg-desc (g:object-data item "pkg-desc"))
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
