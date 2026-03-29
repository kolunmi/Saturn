;;;;;;;;;;;;;
;; use from c

(defun make-object-for-lisp (ptr)
  (make-instance (let ((gtype (g:symbol-for-gtype
                               (g:gtype-name
                                (g:type-from-instance ptr)))))
                   (if gtype gtype 'g:object))
                 :pointer ptr))
(export 'make-object-for-lisp)

(defun make-object-for-c (obj)
  (g:object-pointer obj))
(export 'make-object-for-c)





;;;;;;;;;;
;; utility

(defun extract-tokens (str)
  (split-sequence:split-sequence #\  str
                                 :remove-empty-subseqs t))
(export 'extract-tokens)

(defun match-str-tokens (query match-against)
  (not (loop for q in query
             unless (loop for a in match-against
                          when (search q a :test #'char-equal)
                            return t)
               return t)))
(export 'match-str-tokens)

(defun generic-str-score (query match)
  (round (/ 100000.0
            (- (/ (length match) (length query))
               (/ (or (search query match :test #'char-equal) 0.0) (length match))))))
(export 'generic-str-score)

(defun copy-to-clipboard (str)
  (gdk:clipboard-set-text (gdk:display-clipboard
                           (gdk:display-default))
                          str)
  ;; do this as well in case there are issues with the window closing after the
  ;; clipboard was modified with gdk
  (ignore-errors
   (uiop:run-program (list "flatpak-spawn"
                           "--host"
                           "wl-copy"
                           str))))
(export 'copy-to-clipboard)

(defun flatpak-spawn-host-bin-exists (bin)
  (handler-case
      (uiop:run-program
       (list "flatpak-spawn"
             "--host"
             "which"
             bin))
    (uiop:subprocess-error (e)
      nil)
    (:no-error (output error-output exit-code)
      (= exit-code 0))))
(export 'flatpak-spawn-host-bin-exists)



;;;;;;;;;;;;;;;;;;;;;;;;
;; widget initialization

(defmacro add-shortcuts (controller specs)
  `(progn
     ,@(loop for spec in specs
             collect (destructuring-bind (accel cb) spec
                       `(let* ((trigger (gtk:shortcut-trigger-parse-string ,accel))
                               (action (gtk:callback-action-new ,cb))
                               (shortcut (make-instance 'gtk:shortcut
                                                        :trigger trigger
                                                        :action action)))
                          (gtk:shortcut-controller-add-shortcut ,controller
                                                                shortcut))))))
(export 'add-shortcuts)

(defmacro add-shortcut-controller (widget specs)
  `(let ((controller (gtk:shortcut-controller-new)))
     (add-shortcuts controller ,specs)
     (gtk:widget-add-controller ,widget controller)))
(export 'add-shortcut-controller)

(defmacro make-widget (type
                       (&key props styles connect shortcuts)
                       &optional init-fn)
  `(let ((widget (make-instance ,type ,@props)))
     ,@(mapcar (lambda (class)
                 `(gtk:widget-add-css-class widget ,class))
               styles)
     ,@(mapcar (lambda (spec)
                 (destructuring-bind (name cb) spec
                   `(g:signal-connect widget ,name ,cb)))
               connect)
     ,(when shortcuts
        `(add-shortcut-controller widget ,shortcuts))
     ,(when init-fn
        `(funcall ,init-fn widget))
     widget))
(export 'make-widget)




;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Saturn GObject definitions

(gobject:define-gobject
    "SaturnGenericResult"
    generic-result
    (:superclass g:object
     :export t
     :interfaces ())
    ((obj0
      generic-result-obj0
      "obj0" "GObject" t t)
     (obj1
      generic-result-obj1
      "obj1" "GObject" t t)
     (obj2
      generic-result-obj2
      "obj2" "GObject" t t)
     (obj3
      generic-result-obj3
      "obj3" "GObject" t t)))

(gobject:define-gobject
    "SaturnSignalWidget"
    signal-widget
    (:superclass gtk:widget
     :export t
     :interfaces ())
    ((child
      signal-widget-child
      "child" "GtkWidget" t t)))

(gobject:define-gobject
    "GtkSourceView"
    source-view
    (:superclass gtk:text-view
     :export t
     :interfaces ())
    nil)

(gobject:define-genum
    "SaturnSelectKind"
    select-kind
    (:export t)
  (:none 0)
  (:close 1)
  (:substitute 2))

(gobject:define-gobject
    "SaturnClSelectionEvent"
    selection-event
    (:superclass g:object
     :export t
     :interfaces ())
    ((kind
      selection-event-kind
      "kind" "SaturnSelectKind" t t)
     (selected-text
      selection-event-selected-text
      "selected-text" "gchararray" t t)))

(gobject:define-genum
    "SaturnClCompletionProposalKind"
    completion-proposal-kind
    (:export nil)
  (:normal 0)
  (:package 1)
  (:function 2)
  (:macro 3))

(gobject:define-gobject
    "SaturnClCompletionProposal"
    completion-proposal
    (:superclass g:object
     :export nil
     :interfaces ())
    ((kind
      completion-proposal-kind
      "kind" "SaturnClCompletionProposalKind" t t)
     (string
      completion-proposal-string
      "string" "gchararray" t t)
     (lambda-args
      completion-proposal-lambda-args
      "lambda-args" "GListModel" t t)))





;;;;;;;;;;;;;;;;;;;;;;;;
;; sourceview completion

(defun package-completion-buffer (&rest pkgs)
  (let ((completions (g:list-store-new "GObject")))
    (loop for pkg in pkgs
          for prefix = (string-downcase (package-name pkg))
          do (do-symbols (s pkg)
               (let* ((symbol-kind
                        (cond
                          ((find-package s) :package)
                          ((macro-function s) :macro)
                          ((fboundp s) :function)
                          (t :normal)))
                      (symbol-string
                        (string-downcase (string s)))
                      (symbol-args
                        (when (eql symbol-kind :function)
                          (let ((fn (ignore-errors (symbol-function s))))
                            (when (functionp fn)
                              (let* ((args (si:function-lambda-list fn))
                                     (store (g:list-store-new "GObject")))
                                (loop for arg in args
                                      for str-rep = (format nil "~a" arg)
                                      for str-obj = (gtk:string-object-new str-rep)
                                      do (g:list-store-append store str-obj))
                                store)))))
                      (proposal
                        (make-instance 'completion-proposal
                                       :kind symbol-kind
                                       :string symbol-string
                                       :lambda-args symbol-args)))
                 (g:list-store-append completions proposal))))
    completions))
(export 'package-completion-buffer)

(defun package-completion-buffer-async (text-view &rest pkgs)
  (bordeaux-threads:make-thread
   (lambda ()
     (let ((model (apply #'package-completion-buffer pkgs)))
       (g:idle-add
        (lambda ()
          (finish-source-view-completions model text-view)
          nil))))))
(export 'package-completion-buffer-async)
