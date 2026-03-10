;; this is to prevent objects from getting gc'd while held in C memory
(defvar held-objects '())

(defun hold (object)
  (setq held-objects (cons object held-objects)))
(export 'hold)

;; this is called by C when an object is no longer needed
(defun c-destroy-held-object (object)
  (setq held-objects (remove object held-objects :count 1)))
(export 'c-destroy-held-object)

(defmacro gnew (type &rest props)
  `(let ((obj (gobj-new ,type)))
     ,@(mapcar (lambda (x)
                 `(gobj-set obj
                            ,(first x)
                            ,(second x)))
               props)))
(export 'gnew)
