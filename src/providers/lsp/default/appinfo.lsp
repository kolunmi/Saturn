;; appinfo.lsp
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

(let ((icon-theme (gtk:icon-theme-for-display (gdk:display-default))))
  (mapcar (lambda (path)
            (gtk:icon-theme-add-search-path icon-theme path))
          '("/var/lib/flatpak/exports/share/icons/"
            "/run/host/share/icons/")))

(defparameter *extra-data-dirs*
  (remove-duplicates
   (flet ((home-dir-path (subpath)
            (concatenate 'string
                         "/run/host"
                         (uiop:unix-namestring (user-homedir-pathname))
                         subpath)))
     (mapcar #'merge-pathnames
             (list "/var/lib/flatpak/exports/share/"
                   (home-dir-path ".local/share/flatpak/exports/share/"))))
   :test #'equal))

(progn
  (defconstant +desktop-group-key+ "Desktop Entry")

  (defconstant +desktop-type-key+             "Type")
  (defconstant +desktop-version-key+          "Version")
  (defconstant +desktop-name-key+             "Name")
  (defconstant +desktop-generic-name-key+     "GenericName")
  (defconstant +desktop-no-display-key+       "NoDisplay")
  (defconstant +desktop-comment-key+          "Comment")
  (defconstant +desktop-icon-key+             "Icon")
  (defconstant +desktop-hidden-key+           "Hidden")
  (defconstant +desktop-only-show-in-key+     "OnlyShowIn")
  (defconstant +desktop-not-show-in-key+      "NotShowIn")
  (defconstant +desktop-try-exec-key+         "TryExec")
  (defconstant +desktop-exec-key+             "Exec")
  (defconstant +desktop-path-key+             "Path")
  (defconstant +desktop-terminal-key+         "Terminal")
  (defconstant +desktop-mime-type-key+        "MimeType")
  (defconstant +desktop-categories-key+       "Categories")
  (defconstant +desktop-startup-notify-key+   "StartupNotify")
  (defconstant +desktop-startup-wm-class-key+ "StartupWMClass")
  (defconstant +desktop-url-key+              "URL")
  (defconstant +desktop-dbus-activatable-key+ "DBusActivatable")
  (defconstant +desktop-actions-key+          "Actions")
  (defconstant +desktop-keywords-key+         "Keywords")

  (defconstant +desktop-application-type+     "Application")
  (defconstant +desktop-link-type+            "Link")
  (defconstant +desktop-directory-type+       "Directory")
  )

(defstruct app-info
  desktop-file
  desktop-name
  desktop-exec
  keywords
  needs-terminal
  startup-notify
  icon-name)

(defun read-app-infos-from-system ()
  (labels ((keep-desktop-files (files)
             (loop for file in files
                   for extension = (pathname-type file)
                   when (equal extension "desktop")
                     collect file))
           (make-data-dirs ()
             (remove-duplicates
              (append (mapcar #'(lambda (x)
                                  (merge-pathnames
                                   (concatenate 'string
                                                "/run/host"
                                                (uiop:unix-namestring x)
                                                "/")))
                              (uiop:xdg-data-dirs))
                      *extra-data-dirs*)
              :test #'equal))
           (collect-desktop-files ()
             (let ((data-dirs (make-data-dirs)))
               (apply #'append
                      (loop for data-dir in data-dirs
                            for appsdir = (uiop:subpathname data-dir "applications/")
                            for files = (uiop:directory-files appsdir)
                            collect (keep-desktop-files files)))))
           (file-to-info (file)
             (let* ((keyfile
                      (let ((keyfile (g:key-file-new)))
                        (g:key-file-load-from-file keyfile file :none)
                        keyfile))
                    (desktop-name
                      (g:key-file-string keyfile
                                         +desktop-group-key+
                                         +desktop-name-key+))
                    (desktop-exec
                      (g:key-file-string keyfile
                                         +desktop-group-key+
                                         +desktop-exec-key+))
                    (desktop-categories
                      (g:key-file-string keyfile
                                         +desktop-group-key+
                                         +desktop-categories-key+))
                    (desktop-categories-list
                      (split-sequence:split-sequence #\;
                                                     desktop-categories
                                                     :remove-empty-subseqs t))
                    (desktop-keywords
                      (g:key-file-string keyfile
                                         +desktop-group-key+
                                         +desktop-keywords-key+))
                    (desktop-keywords-list
                      (split-sequence:split-sequence #\;
                                                     desktop-keywords
                                                     :remove-empty-subseqs t))
                    (keywords-list (append desktop-categories-list
                                           desktop-keywords-list))
                    (needs-terminal
                      (g:key-file-boolean keyfile
                                          +desktop-group-key+
                                          +desktop-terminal-key+))
                    (startup-notify
                      (g:key-file-boolean keyfile
                                          +desktop-group-key+
                                          +desktop-startup-notify-key+))
                    (icon-name
                      (g:key-file-string keyfile
                                         +desktop-group-key+
                                         +desktop-icon-key+)))
               (make-app-info :desktop-file file
                              :desktop-name desktop-name
                              :desktop-exec desktop-exec
                              :keywords keywords-list
                              :needs-terminal needs-terminal
                              :startup-notify startup-notify
                              :icon-name icon-name))))
    (mapcar #'file-to-info (collect-desktop-files))))

(defvar *app-infos* (read-app-infos-from-system))


(gobject:define-gobject-subclass
    "SaturnAppinfoResult"
    appinfo-result
    (:superclass saturn:generic-result
     :export t
     :interfaces ())
    nil)
(gobject:define-gobject-subclass
    "SaturnAppinfoResultListItem"
    appinfo-result-list-item
    (:superclass saturn:signal-widget
     :export nil
     :interfaces ())
    nil)

(defvar list-item-ui
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<interface>
  <template class=\"SaturnAppinfoResultListItem\">
    <property name=\"child\">
      <object class=\"GtkBox\">
        <property name=\"orientation\">horizontal</property>
        <property name=\"spacing\">6</property>
        <child>
          <object class=\"GtkImage\">
            <binding name=\"icon-name\">
              <lookup name=\"string\" type=\"GtkStringObject\">
                <lookup name=\"obj1\" type=\"SaturnAppinfoResult\">
                  <lookup name=\"item\">SaturnAppinfoResultListItem</lookup>
                </lookup>
              </lookup>
            </binding>
            <property name=\"icon-size\">large</property>
          </object>
        </child>
        <child>
          <object class=\"GtkLabel\">
            <style>
              <class name=\"title-4\"/>
            </style>
            <property name=\"halign\">start</property>
            <property name=\"hexpand\">true</property>
            <property name=\"ellipsize\">end</property>
            <binding name=\"label\">
              <lookup name=\"string\" type=\"GtkStringObject\">
                <lookup name=\"obj0\" type=\"SaturnAppinfoResult\">
                  <lookup name=\"item\">SaturnAppinfoResultListItem</lookup>
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
            <property name=\"label\">Application</property>
          </object>
        </child>
      </object>
    </property>
  </template>
</interface>
")
(defmethod g:object-class-init :after
    ((subclass (eql (find-class 'appinfo-result-list-item))) class data)
  (gtk:widget-class-set-template "SaturnAppinfoResultListItem" list-item-ui))
(defmethod g:object-instance-init :after
    ((subclass (eql (find-class 'appinfo-result-list-item))) instance data)
  (declare (ignore class data))
  (gtk:widget-init-template instance))
(setf +list-bind-gtype+ "SaturnAppinfoResultListItem")


;; PROVIDER IMPLEMENTATION

(defun query (provider object store)
  (let* ((str (gtk:string-object-string object))
         (tokens (saturn:extract-tokens str)))
    (loop for info in *app-infos*
          for desktop-name = (app-info-desktop-name info)
          for keywords = (append (list desktop-name)
                                 (app-info-keywords info))
          for icon-name = (app-info-icon-name info)
          when (and desktop-name
                    icon-name
                    (saturn:match-str-tokens tokens keywords))
            do (saturn:submit-result
                (let ((result (make-instance 'appinfo-result
                                             :obj0 (gtk:string-object-new desktop-name)
                                             :obj1 (gtk:string-object-new icon-name))))
                  (setf (g:object-data result "info") info)
                  result)
                store provider))))

(defun score (provider item query)
  (let* ((str (gtk:string-object-string query))
         (info (g:object-data item "info"))
         (name (app-info-desktop-name info)))
    (* 100 (saturn:generic-str-score str name))))

(defun select (provider item query)
  (let* ((info (g:object-data item "info"))
         (desktop-file (uiop:unix-namestring (app-info-desktop-file info)))
         (run-host (search "/run/host" desktop-file)))
    (when (and run-host
               (= run-host 0))
      (setf desktop-file (subseq desktop-file (length "/run/host"))))
    (uiop:run-program (list "flatpak-spawn"
                            "--host"
                            "gio"
                            "launch"
                            desktop-file)))
  nil)

(defun bind-preview (provider item)
  (let* ((info (g:object-data item "info"))
         (name (app-info-desktop-name info))
         (icon-name (app-info-icon-name info))
         (image
           (saturn:make-widget
            'gtk:image
            (:props (:icon-name icon-name
                     :pixel-size 256)))))
    image))
