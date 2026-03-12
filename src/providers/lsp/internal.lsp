(defun make-object-for-lisp (ptr)
  (let ((obj (make-instance (g:symbol-for-gtype
                             (g:gtype-name
                              (g:type-from-instance ptr)))
                            :pointer ptr)))
    obj))
(export 'make-object-for-lisp)

(defun make-object-for-c (obj)
  (g:object-pointer obj))
(export 'make-object-for-c)

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
