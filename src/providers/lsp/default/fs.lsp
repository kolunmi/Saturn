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

  (bordeaux-threads:make-thread
   (lambda ()
     (gather-files #P"~/")))

  (gobject:define-gobject-subclass
      "SaturnFsResult"
      fs-result
      (:superclass g:object
       :export t
       :interfaces ())
      ((name
        fs-result-name
        "name" "gchararray" t t)))

  ;; PROVIDER IMPLEMENTATION

  (defun query (provider object store)
    (let ((str (gtk:string-object-string object)))
      (bordeaux-threads:make-thread
       (lambda ()
         (bordeaux-threads:with-lock-held (*work-lock*)
           (block root
             (loop for path across *files-array*
                   when (search str (file-namestring path))
                     do (let ((result (make-instance 'fs-result)))
                          ;; gobject properties weirdly don't work
                          (setf (g:object-data result "path") path)
                          (unless (saturn:submit-result result store provider)
                            (return-from root))))))))))

  )


(defun score (provider item query)
  (let ((str (gtk:string-object-string query))
        (name (file-namestring (g:object-data item "path"))))
    (round (/ 10000.0
              (- (/ (length name) (length str))
                 (/ (search str name) (length name)))))))

(defun select (provider item query)
  (format t "selected the file!~%")
  nil)

(defun bind-list-item (provider item)
  (let* ((path (g:object-data item "path"))
         (start-label
           (saturn:make-widget
            'gtk:label
            (:props (:label (uiop:unix-namestring
                             (uiop:pathname-directory-pathname path))
                     :xalign 0.0
                     :ellipsize :start
                     :hexpand t)
             :styles ("subtitle"))))
         (end-label
           (saturn:make-widget
            'gtk:label
            (:props (:label (file-namestring path)
                     :xalign 1.0
                     :ellipsize :start
                     :margin-end 25)
             :styles ("title-4"))))
         (box
           (saturn:make-widget
            'gtk:box
            (:props (:orientation :horizontal
                     :spacing 25))
            (lambda (x)
              (gtk:box-append x start-label)
              (gtk:box-append x end-label)))))
    box))

(defun bind-preview (provider item)
  (let* ((label
           (saturn:make-widget 'gtk:label
               (:props (:label (uiop:unix-namestring (g:object-data item "path"))
                        :ellipsize :middle
                        :hexpand t)
                :styles ("monospace")))))
    label))
