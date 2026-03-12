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
