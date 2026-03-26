(require :asdf)
;; (require :slynk)

(defvar deps
  '(
    (:saturn-cl-deps       "./"                     )
    (:asdf                 nil                      )
    ;; (:slynk                nil                      )
    (:split-sequence       "./split-sequence/"      )
    (:closer-mop           "./closer-mop/"          )
    (:trivial-garbage      "./trivial-garbage/"     )
    (:global-vars          "./global-vars/"         )
    (:bordeaux-threads     "./bordeaux-threads/"    )
    (:iterate              "./iterate/"             )
    (:babel                "./babel/"               )
    (:alexandria           "./alexandria/"          )
    (:trivial-features     "./trivial-features/"    )
    (:cffi                 "./cffi/"                )
    (:cl-cffi-graphene     "./cl-cffi-graphene/"    )
    (:cl-cffi-cairo        "./cl-cffi-cairo/"       )
    (:cl-cffi-gtk4         "./cl-cffi-gtk4/"        )
    (:cl-cffi-gtk4-init    "./cl-cffi-gtk4/"        )
    (:cl-cffi-glib         "./cl-cffi-glib/"        )
    (:cl-cffi-glib-init    "./cl-cffi-glib/"        )
    (:cl-cffi-pango        "./cl-cffi-pango/"       )
    (:cl-cffi-gdk-pixbuf   "./cl-cffi-gdk-pixbuf/"  )
    ))


(mapcar #'(lambda (x)
            (destructuring-bind (pkg path) x
              (when path
                (push (merge-pathnames path)
                      asdf:*central-registry*))))
        deps)

(asdf:load-system :cffi)
(setf cffi-sys:*cffi-ecl-method* :c/c++)

(defun pkg-config-libs (name)
  (uiop:run-program (list "pkg-config" "--libs" name) :output :string))
(setf c:*user-linker-flags* (pkg-config-libs "gtk4"))

(mapcar #'(lambda (x)
            (let ((init-name
                    (concatenate 'string
                                 "init_lib_"
                                 (substitute #\_ #\- (string x)))))
              (asdf:make-build x
                               :type :static-library
                               :move-here "./"
                               :init-name init-name
                               :monolithic t)))
        '(:saturn-cl-deps))

(quit)
