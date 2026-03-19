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

(defun generic-str-score (query match)
  (round (/ 100000.0
            (- (/ (length match) (length query))
               (/ (or (search query match) 0.0) (length match))))))
(export 'generic-str-score)

(defmacro make-widget (type (&key props styles connect) &optional init-fn)
  `(let ((widget (make-instance ,type ,@props)))
     ,@(mapcar (lambda (class)
                 `(gtk:widget-add-css-class widget ,class))
               styles)
     ,@(mapcar (lambda (spec)
                 (destructuring-bind (name cb) spec
                   `(g:signal-connect widget ,name ,cb)))
               connect)
     ,(when init-fn
        `(funcall ,init-fn widget))
     widget))
(export 'make-widget)


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
(setf (g:symbol-for-gtype (g:gtype "SaturnGenericResult")) 'generic-result)

(gobject:define-gobject
    "SaturnSignalWidget"
    signal-widget
    (:superclass gtk:widget
     :export t
     :interfaces ())
    ((child
      signal-widget-child
      "child" "GtkWidget" t t)))
(setf (g:symbol-for-gtype (g:gtype "SaturnSignalWidget")) 'signal-widget)
